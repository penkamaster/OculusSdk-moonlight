/************************************************************************************

Filename    :   GlSetup.cpp
Content     :   GL Setup
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "GlSetup.h"
#include "Kernel/OVR_Types.h"

#if defined( OVR_OS_WIN32 )

#include <string.h>
#include <stdlib.h>

#include "Kernel/OVR_LogUtils.h"
#include "OVR_Input.h"
#include "AppLocal.h"

namespace OVR {

#define OPENGL_VERSION_MAJOR	4
#define OPENGL_VERSION_MINOR	1
#define GLSL_PROGRAM_VERSION	"410"

typedef GLuint64 Microseconds_t;

static Microseconds_t GetTimeMicroseconds()
{
	static Microseconds_t ticksPerSecond = 0;
	static Microseconds_t timeBase = 0;

	if ( ticksPerSecond == 0 )
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency( &li );
		ticksPerSecond = (Microseconds_t) li.QuadPart;
		QueryPerformanceCounter( &li );
		timeBase = (Microseconds_t) li.LowPart + 0xFFFFFFFFLL * li.HighPart;
	}

	LARGE_INTEGER li;
	QueryPerformanceCounter( &li );
	Microseconds_t counter = (Microseconds_t) li.LowPart + 0xFFFFFFFFLL * li.HighPart;
	return ( counter - timeBase ) * 1000000LL / ticksPerSecond;
}

typedef enum
{
	MOUSE_LEFT		= 0,
	MOUSE_RIGHT		= 1
} MouseButton_t;

typedef struct
{
	HDC				hDC;
	HGLRC			hGLRC;
} GlContext_t;

typedef struct
{
	const GLubyte *			glRenderer;
	const GLubyte *			glVersion;
	GlContext_t				glContext;
	int						windowWidth;
	int						windowHeight;
	float					windowRefreshRate;
	bool					windowFullscreen;
	bool					windowActive;
	bool					keyInput[OVR_KEY_MAX];
	bool					mouseInput[8];
	int						mouseInputX[8];
	int						mouseInputY[8];
	Microseconds_t			lastSwapTime;
	HINSTANCE				hInstance;
	HWND					hWnd;
} GlWindow_t;

GlWindow_t glWindow;

static const int	MIN_SLOTS_AVAILABLE_FOR_INPUT = 12;
AppLocal * app;

LRESULT APIENTRY WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	GlWindow_t * window = (GlWindow_t *) GetWindowLongPtr( hWnd, GWLP_USERDATA );

	switch ( message )
	{
		case WM_SIZE:
		{
			if ( window != NULL )
			{
				window->windowWidth = (int) LOWORD( lParam );
				window->windowHeight = (int) HIWORD( lParam );
			}
			return 0;
		}
		case WM_ACTIVATE:
		{
			if ( window != NULL )
			{
				window->windowActive = !HIWORD( wParam );
			}
			return 0;
		}
		case WM_ERASEBKGND:
		{
			return 0;
		}
		case WM_SETFOCUS:
		{
			return 0;
		}
		case WM_CLOSE:
		{
			PostQuitMessage( 0 );
			return 0;
		}
		case WM_KEYDOWN:
		{
			if ( window != NULL )
			{
				const ovrKeyCode key = OSKeyToKeyCode( (int)wParam );
				if ( app && !window->keyInput[key] )
				{
					app->GetMessageQueue().PostPrintfIfSpaceAvailable( MIN_SLOTS_AVAILABLE_FOR_INPUT, "key %i %i %i", key, 1, 0 );
				}
				window->keyInput[key] = true;
				LOG( "%s down\n", GetNameForKeyCode( key ) );
			}
			break;
		}
		case WM_KEYUP:
		{
			if ( window != NULL )
			{
				const ovrKeyCode key = OSKeyToKeyCode( ( int ) wParam );
				window->keyInput[key] = false;
				LOG( "%s up\n", GetNameForKeyCode( key ) );
				if ( app )
				{
					app->GetMessageQueue().PostPrintfIfSpaceAvailable( MIN_SLOTS_AVAILABLE_FOR_INPUT, "key %i %i %i", key, 0, 0 );
				}
			}
			break;
		}
		case WM_LBUTTONDOWN:
		{
			window->mouseInput[MOUSE_LEFT] = true;
			window->mouseInputX[MOUSE_LEFT] = LOWORD( lParam );
			window->mouseInputY[MOUSE_LEFT] = window->windowHeight - HIWORD( lParam );
			break;
		}
		case WM_RBUTTONDOWN:
		{
			window->mouseInput[MOUSE_RIGHT] = true;
			window->mouseInputX[MOUSE_RIGHT] = LOWORD( lParam );
			window->mouseInputY[MOUSE_RIGHT] = window->windowHeight - HIWORD( lParam );
			break;
		}
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}

static bool GlWindow_ProcessEvents( GlWindow_t * window, int32_t & exitCode )
{
	OVR_UNUSED( window );

	bool quit = false;
	exitCode = 0;
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) > 0 )
	{
		if ( msg.message == WM_QUIT )
		{
			quit = true;
			exitCode = static_cast<int32_t>( msg.wParam );
			break;
		}
		else
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
	return quit;
}

#if 0
static void GlWindow_SwapInterval( GlWindow_t * window, const int swapInterval )
{
	OVR_UNUSED( window );
	wglSwapIntervalEXT( swapInterval );
}

static void GlWindow_SwapBuffers( GlWindow_t * window )
{
	SwapBuffers( window->glContext.hDC );
	window->lastSwapTime = GetTimeMicroseconds();
}

static Microseconds_t GlWindow_GetNextSwapTime( GlWindow_t * window )
{
	return window->lastSwapTime + (Microseconds_t)( 1000.0f * 1000.0f / window->windowRefreshRate );
}

static bool GlWindow_CheckMouseButton( GlWindow_t * window, const MouseButton_t button )
{
	if ( window->mouseInput[button] )
	{
		window->mouseInput[button] = false;
		return true;
	}
	return false;
}

static bool GlWindow_CheckKeyboardKey( GlWindow_t * window, const ovrKeyCode keyCode ) 
{
	if ( window->keyInput[keyCode] )
	{
		window->keyInput[keyCode] = false;
		return true;
	}
	return false;
}
#endif

static void GlWindow_Destroy( GlWindow_t * window )
{
	if ( window->windowFullscreen )
	{
		ChangeDisplaySettings( NULL, 0 );
		ShowCursor( TRUE );
	}

	if ( window->glContext.hGLRC )
	{
		if ( !wglMakeCurrent( NULL, NULL ) )
		{
			FAIL( "Failed to release context." );
		}

		if ( !wglDeleteContext( window->glContext.hGLRC ) )
		{
			FAIL( "Failed to delete context." );
		}
		window->glContext.hGLRC = NULL;
	}

	if ( window->glContext.hDC )
	{
		if ( !ReleaseDC( window->hWnd, window->glContext.hDC ) )
		{
			FAIL( "Failed to release device context." );
		}
		window->glContext.hDC = NULL;
	}

	if ( window->hWnd )
	{
		if ( !DestroyWindow( window->hWnd ) )
		{
			FAIL( "Failed to destroy the window." );
		}
		window->hWnd = NULL;
	}

	if ( window->hInstance )
	{
		if ( !UnregisterClass( "OpenGL", window->hInstance ) )
		{
			FAIL( "Failed to unregister window class." );
		}
		window->hInstance = NULL;
	}

}

static bool GlWindow_Create( GlWindow_t * window, const int width, const int height,
							 const int colorBits, const int depthBits,
							 const bool fullscreen,
							 const char * title, const unsigned int iconResId )
{
	memset( window, 0, sizeof( GlWindow_t ) );

	window->windowWidth = width;
	window->windowHeight = height;
	window->windowRefreshRate = 60;
	window->windowFullscreen = fullscreen;
	window->windowActive = true;
	window->lastSwapTime = GetTimeMicroseconds();

	window->hInstance = GetModuleHandle( NULL );

	WNDCLASS wc;
	wc.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc		= (WNDPROC) WndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= window->hInstance;
	wc.hIcon			= LoadIcon( window->hInstance, MAKEINTRESOURCE( iconResId ) );
	wc.hCursor			= LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground	= NULL;
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= "OpenGL";

	if ( !RegisterClass( &wc ) )
	{
		FAIL( "Failed to register window class." );
	}
	
	if ( fullscreen )
	{
		DEVMODE dmScreenSettings;
		memset( &dmScreenSettings, 0, sizeof( dmScreenSettings ) );
		dmScreenSettings.dmSize			= sizeof( dmScreenSettings );
		dmScreenSettings.dmPelsWidth	= width;
		dmScreenSettings.dmPelsHeight	= height;
		dmScreenSettings.dmBitsPerPel	= 32;
		dmScreenSettings.dmFields		= DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

		if ( ChangeDisplaySettings( &dmScreenSettings, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
		{
			FAIL( "The requested fullscreen mode is not supported." );
		}
	}

	DEVMODE lpDevMode;
	memset( &lpDevMode, 0, sizeof( DEVMODE ) );
	lpDevMode.dmSize = sizeof( DEVMODE );
	lpDevMode.dmDriverExtra = 0;

	if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &lpDevMode ) != FALSE )
	{
		window->windowRefreshRate = (float)lpDevMode.dmDisplayFrequency;
	}

	DWORD dwExStyle = 0;
	DWORD dwStyle = 0;
	if ( fullscreen )
	{
		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP;
		ShowCursor( FALSE );
	}
	else
	{
		// Fixed size window.
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	}

	RECT windowRect;
	windowRect.left = (long)0;
	windowRect.right = (long)width;
	windowRect.top = (long)0;
	windowRect.bottom = (long)height;

	AdjustWindowRectEx( &windowRect, dwStyle, FALSE, dwExStyle );

	if ( !fullscreen )
	{
		RECT desktopRect;
		GetWindowRect( GetDesktopWindow(), &desktopRect );

		const int offsetX = ( desktopRect.right - ( windowRect.right - windowRect.left ) ) / 2;
		const int offsetY = ( desktopRect.bottom - ( windowRect.bottom - windowRect.top ) ) / 2;

		windowRect.left += offsetX;
		windowRect.right += offsetX;
		windowRect.top += offsetY;
		windowRect.bottom += offsetY;
	}

	window->hWnd = CreateWindowEx( dwExStyle,						// Extended style for the window
								"OpenGL",							// Class name
								title,								// Window title
								dwStyle |							// Defined window style
								WS_CLIPSIBLINGS |					// Required window style
								WS_CLIPCHILDREN,					// Required window style
								windowRect.left,					// Window X position
								windowRect.top,						// Window Y position
								windowRect.right - windowRect.left,	// Window width
								windowRect.bottom - windowRect.top,	// Window height
								NULL,								// No parent window
								NULL,								// No menu
								window->hInstance,					// Instance
								NULL );
	if ( !window->hWnd )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to create window." );
	}

	SetWindowLongPtr( window->hWnd, GWLP_USERDATA, (LONG_PTR) window );

	window->glContext.hDC = GetDC( window->hWnd );
	if ( !window->glContext.hDC )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to acquire device context." );
	}

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof( PIXELFORMATDESCRIPTOR ),
		1,						// version
		PFD_DRAW_TO_WINDOW |	// must support windowed
		PFD_SUPPORT_OPENGL |	// must support OpenGL
		PFD_DOUBLEBUFFER,		// must support double buffering
		PFD_TYPE_RGBA,			// iPixelType
		(BYTE)colorBits,		// cColorBits
		0, 0,					// cRedBits, cRedShift
		0, 0,					// cGreenBits, cGreenShift
		0, 0,					// cBlueBits, cBlueShift
		0, 0,					// cAlphaBits, cAlphaShift
		0,						// cAccumBits
		0,						// cAccumRedBits
		0,						// cAccumGreenBits
		0,						// cAccumBlueBits
		0,						// cAccumAlphaBits
		(BYTE)depthBits,		// cDepthBits
		0,						// cStencilBits
		0,						// cAuxBuffers
		PFD_MAIN_PLANE,			// iLayerType
		0,						// bReserved
		0,						// dwLayerMask
		0,						// dwVisibleMask
		0						// dwDamageMask
	};

	GLuint pixelFormat = ChoosePixelFormat( window->glContext.hDC, &pfd );
	if ( pixelFormat == 0 )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to find a suitable PixelFormat." );
	}

	if ( !SetPixelFormat( window->glContext.hDC, pixelFormat, &pfd ) )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to set the PixelFormat." );
	}

	// temporarily create a context to get the extensions
	{
		HGLRC hGLRC = wglCreateContext( window->glContext.hDC );
		wglMakeCurrent( window->glContext.hDC, hGLRC );

		GL_InitExtensions();

		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( hGLRC );
	}

	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB,	OPENGL_VERSION_MAJOR,
		WGL_CONTEXT_MINOR_VERSION_ARB,	OPENGL_VERSION_MINOR,
		WGL_CONTEXT_PROFILE_MASK_ARB,	WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB,			WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		0
	};

	window->glContext.hGLRC = wglCreateContextAttribsARB( window->glContext.hDC, NULL, attribs );
	if ( !window->glContext.hGLRC )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to create GL context." );
	}

	if ( !wglMakeCurrent( window->glContext.hDC, window->glContext.hGLRC ) )
	{
		GlWindow_Destroy( window );
		FAIL( "Failed to activate GL context." );
	}

	ShowWindow( window->hWnd, SW_SHOW );
	SetForegroundWindow( window->hWnd );
	SetFocus( window->hWnd );

	window->glRenderer = glGetString( GL_RENDERER );
	window->glVersion = glGetString( GL_VERSION );

	LOG( "--------------------------------\n" );
	//LOG( "OS     : %s\n", GetOSVersion() );
	LOG( "GPU    : %s\n", window->glRenderer );
	LOG( "DRIVER : %s\n", window->glVersion );
	LOG( "MODE   : %s %dx%d %1.0f Hz\n", fullscreen ? "fullscreen" : "windowed", window->windowWidth, window->windowHeight, window->windowRefreshRate );

	return true;
}

glSetup_t GL_Setup( const int windowWidth, const int windowHeight, const bool fullscreen, 
				    const char * windowTitle, const unsigned int iconResId,
					AppLocal * appLocal )
{
	glSetup_t gl = {};

	app = appLocal;

	GlWindow_Create( &glWindow,
						windowWidth,
						windowHeight,
						32, 0, fullscreen,
						windowTitle, iconResId );

	return gl;
}

void GL_Shutdown( glSetup_t & glSetup )
{
	app = NULL;
	GlWindow_Destroy( &glWindow );
}

bool GL_ProcessEvents( int32_t & exitCode )
{		
	bool exit = false;
	//const Microseconds_t time = GetTimeMicroseconds();

	if ( GlWindow_ProcessEvents( &glWindow, exitCode ) )
	{
		exit = true;
	}

	if ( glWindow.windowActive )
	{
		//GlWindow_SwapBuffers( &glWindow );

		//sceneThreadData.nextSwapTime = GlWindow_GetNextSwapTime( &glWindow );
	}

	return exit;
}

}	// namespace OVR

#endif	// OVR_OS_WIN32
