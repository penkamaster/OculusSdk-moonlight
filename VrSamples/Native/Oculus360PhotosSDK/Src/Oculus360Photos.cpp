/************************************************************************************

Filename    :   Oculus360Photos.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Oculus360Photos.h"
#include "OVR_Input.h"
#include "GuiSys.h"
#include "PanoBrowser.h"
#include "PanoMenu.h"
#include "FileLoader.h"
#include "ImageData.h"
#include "PackageFiles.h"
#include "PhotosMetaData.h"
#include "OVR_Locale.h"

#if defined( OVR_OS_ANDROID )
#include "unistd.h"
#endif

static const char * DEFAULT_PANO = "assets/placeholderBackground.jpg";

#if defined( OVR_OS_ANDROID )
extern "C" {

long Java_com_oculus_oculus360photossdk_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
	jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.
	LOG( "nativeSetAppInterface" );
	return (new OVR::Oculus360Photos())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

#endif

namespace OVR
{
static const char * TextureMvpVertexShaderSrc =
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"uniform mediump vec4 UniformColor;\n"
	"varying  lowp vec4 oColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"	oTexCoord = TexCoord;\n"
	"   oColor = UniformColor;\n"
	"}\n";

static const char * TexturedMvpFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4	oColor;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = oColor * texture2D( Texture0, oTexCoord );\n"
	"}\n";

//-------------------------------------------------------------------------

static const char * CubeMapPanoVertexShaderSrc =
	"attribute vec4 Position;\n"
	"uniform mediump vec4 UniformColor;\n"
	"varying  lowp vec4 oColor;\n"
	"varying highp vec3 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"	oTexCoord = Position.xyz;\n"
	"   oColor = UniformColor;\n"
	"}\n";

static const char * CubeMapPanoFragmentShaderSrc =
	"uniform samplerCube Texture0;\n"
	"varying highp vec3 oTexCoord;\n"
	"varying lowp vec4	oColor;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = oColor * textureCube( Texture0, oTexCoord );\n"
	"}\n";

//==============================================================
// ovrGuiSoundEffectPlayer
class ovrGuiSoundEffectPlayer : public OvrGuiSys::SoundEffectPlayer
{
public:
	ovrGuiSoundEffectPlayer( ovrSoundEffectContext & context )
		: SoundEffectContext( context )
	{
	}

	virtual bool Has( const char * name ) const OVR_OVERRIDE { return SoundEffectContext.GetMapping().HasSound( name ); }
	virtual void Play( const char * name ) OVR_OVERRIDE { SoundEffectContext.Play( name ); }

private:
	ovrGuiSoundEffectPlayer &	operator = ( ovrGuiSoundEffectPlayer & );
private:
	ovrSoundEffectContext & SoundEffectContext;
};


Oculus360Photos::DoubleBufferedTextureData::DoubleBufferedTextureData() :
	TextureSwapChain(),
	Width(),
	Height(),
	CurrentIndex( 0 )
{
}

Oculus360Photos::DoubleBufferedTextureData::~DoubleBufferedTextureData()
{
	vrapi_DestroyTextureSwapChain( TextureSwapChain[ 0 ] );
	vrapi_DestroyTextureSwapChain( TextureSwapChain[ 1 ] );
}

ovrTextureSwapChain * Oculus360Photos::DoubleBufferedTextureData::GetRenderTextureSwapChain() const
{
	return TextureSwapChain[ CurrentIndex ^ 1 ];
}

ovrTextureSwapChain * Oculus360Photos::DoubleBufferedTextureData::GetLoadTextureSwapChain() const
{
	return TextureSwapChain[ CurrentIndex ];
}

void Oculus360Photos::DoubleBufferedTextureData::SetLoadTextureSwapChain( ovrTextureSwapChain * chain )
{
	TextureSwapChain[ CurrentIndex ] = chain;
}

void Oculus360Photos::DoubleBufferedTextureData::Swap()
{
	CurrentIndex ^= 1;
}

void Oculus360Photos::DoubleBufferedTextureData::SetSize( const int width, const int height )
{
	Width[ CurrentIndex ] = width;
	Height[ CurrentIndex ] = height;
}

bool Oculus360Photos::DoubleBufferedTextureData::SameSize( const int width, const int height ) const
{
	return ( Width[ CurrentIndex ] == width && Height[ CurrentIndex ] == height );
}

Oculus360Photos::Oculus360Photos()
	: SoundEffectContext( NULL )
	, SoundEffectPlayer( NULL )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( NULL )
	, GlobeProgramColor( 1.0f )
	, Fader( 0.0f )
	, MetaData( NULL )
	, PanoMenu( NULL )
	, Browser( NULL )
	, ActivePano( NULL )
	, BackgroundPanoTexData()
	, BackgroundCubeTexData()
	, CurrentPanoIsCubeMap( false )
	, MessageQueue( 100 )
	, MenuState( MENU_NONE )
	, FadeOutRate( 1.0f / 0.45f )
	, FadeInRate( 1.0f / 0.5f )
	, PanoMenuVisibleTime( 7.0f )
	, CurrentFadeRate( FadeOutRate )
	, CurrentFadeLevel( 0.0f )
	, PanoMenuTimeLeft( -1.0f )
	, BrowserOpenTime( 0.0f )
	, UseOverlay( true )
	, UseSrgb( true )
	, BackgroundCommands( 100 )
#if defined( OVR_OS_ANDROID )
	, EglClientVersion( 0 )
	, EglDisplay( 0 )
	, EglConfig( 0 )
	, EglPbufferSurface( 0 )
	, EglShareContext( 0 )
#endif
{
	ShutdownRequest.SetState( false );
}

Oculus360Photos::~Oculus360Photos()
{
	LOG( "--------------- ~Oculus360Photos() ---------------" );

	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;

	// Shut down background loader
	ShutdownRequest.SetState( true );

	GlobeSurfaceDef.geo.Free();
	GlProgram::Free( TexturedMvpProgram );
	GlProgram::Free( CubeMapPanoProgram );

	if ( MetaData )
	{
		delete MetaData;
	}

#if defined( OVR_OS_ANDROID )
	if ( eglDestroySurface( EglDisplay, EglPbufferSurface ) == EGL_FALSE )
	{
		FAIL( "eglDestroySurface: shutdown failed" );
	}
#endif

	OvrGuiSys::Destroy( GuiSys );
}

void Oculus360Photos::Configure( ovrSettings & settings )
{
	// Use Srgb source layers if they are supported.
	UseSrgb = vrapi_GetSystemPropertyInt( app->GetJava(), VRAPI_SYS_PROP_SRGB_LAYER_SOURCE_AVAILABLE ) != 0;
	LOG( "Oculus360Photos::Configure : UseSrgb %d", UseSrgb );

	settings.UseSrgbFramebuffer = UseSrgb;

	// We need very little CPU for pano browsing, but a fair amount of GPU.
	// The CPU clock should ramp up above the minimum when necessary.
	settings.CpuLevel = 1;	// jpeg loading is slow, but otherwise we need little
	settings.GpuLevel = 3;	// we need a fair amount for cube map overlay support

	// No hard edged geometry, so no need for MSAA
	settings.EyeBufferParms.multisamples = 1;
	settings.EyeBufferParms.colorFormat = UseSrgb ? COLOR_8888_sRGB : COLOR_8888;
	settings.EyeBufferParms.depthFormat = DEPTH_16;

	settings.TrackingTransform = VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL;

	settings.RenderMode = RENDERMODE_MULTIVIEW;
}

Thread loadingThread;

void Oculus360Photos::EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );

	if ( intentType == INTENT_LAUNCH )
	{
		// This is called by the VR thread, not the java UI thread.
		LOG( "--------------- Oculus360Photos OneTimeInit ---------------" );

		const ovrJava * java = app->GetJava();
		SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
		SoundEffectContext->Initialize( &app->GetFileSys() );
		SoundEffectPlayer = new ovrGuiSoundEffectPlayer( *SoundEffectContext );

		Locale = ovrLocale::Create( *java->Env, java->ActivityObject, "default" );

		String fontName;
		GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
		GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );

		GuiSys->GetGazeCursor().ShowCursor();

		//-------------------------------------------------------------------------
		static ovrProgramParm uniformParms[] =	// both TextureMvpProgram and CubeMapPanoProgram use the same parm mapping
		{
			{ "UniformColor",	ovrProgramParmType::FLOAT_VECTOR4 },
			{ "Texture0",		ovrProgramParmType::TEXTURE_SAMPLED },
		};

		TexturedMvpProgram = GlProgram::Build(  TextureMvpVertexShaderSrc,
												TexturedMvpFragmentShaderSrc,
												uniformParms, sizeof( uniformParms ) / sizeof( ovrProgramParm ) );

		CubeMapPanoProgram = GlProgram::Build(  CubeMapPanoVertexShaderSrc,
												CubeMapPanoFragmentShaderSrc,
												uniformParms, sizeof( uniformParms ) / sizeof( ovrProgramParm ) );

		// launch cube pano -should always exist!
		StartupPano = DEFAULT_PANO;

		LOG( "Creating Globe" );
		GlobeSurfaceDef.surfaceName = "Globe";
		GlobeSurfaceDef.geo = BuildGlobe();
		GlobeSurfaceDef.graphicsCommand.Program = CubeMapPanoProgram;
		GlobeSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
		GlobeSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

		GlobeProgramColor = Vector4f( 1.0f, 1.0f, 1.0f, 1.0f );

		InitFileQueue( app, this );

		// meta file used by OvrMetaData
		const char * relativePath = "Oculus/360Photos/";
		const char * metaFile = "meta.json";

		MetaData = new OvrPhotosMetaData();
		if ( MetaData == NULL )
		{
			FAIL( "Oculus360Photos::OneTimeInit failed to create MetaData" );
		}

		OvrMetaDataFileExtensions fileExtensions;

		fileExtensions.GoodExtensions.PushBack( ".jpg" );

		fileExtensions.BadExtensions.PushBack( ".jpg.x" );
		fileExtensions.BadExtensions.PushBack( "_px.jpg" );
		fileExtensions.BadExtensions.PushBack( "_py.jpg" );
		fileExtensions.BadExtensions.PushBack( "_pz.jpg" );
		fileExtensions.BadExtensions.PushBack( "_nx.jpg" );
		fileExtensions.BadExtensions.PushBack( "_ny.jpg" );

		const OvrStoragePaths & storagePaths = app->GetStoragePaths();
		storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
		storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );

		LOG( "360 PHOTOS using %d searchPaths", SearchPaths.GetSizeI() );

		const double startTime = SystemClock::GetTimeInSeconds();

		String AppFileStoragePath;
		LOG( "360 PHOTOS using %d searchPaths", SearchPaths.GetSizeI() );

		if ( !storagePaths.GetPathIfValidPermission( EST_PRIMARY_EXTERNAL_STORAGE, EFT_CACHE, "", permissionFlags_t( PERMISSION_READ ), AppFileStoragePath ) )
		{
			FAIL( "Oculus360Photos::OneTimeInit - failed to access app cache storage" );
		}
		LOG( "Oculus360Photos::OneTimeInit found AppCacheStoragePath: %s", AppFileStoragePath.ToCStr() );

		MetaData->InitFromDirectory( relativePath, SearchPaths, fileExtensions );
		MetaData->InsertCategoryAt( 0, "Favorites" );
		JSON * storedMetaData = MetaData->CreateOrGetStoredMetaFile( AppFileStoragePath.ToCStr(), metaFile );
		MetaData->ProcessMetaData( storedMetaData, SearchPaths, metaFile );

		LOG( "META DATA INIT TIME: %f", SystemClock::GetTimeInSeconds() - startTime );

		// Start building the PanoMenu
		PanoMenu = ( OvrPanoMenu * )GuiSys->GetMenu( OvrPanoMenu::MENU_NAME );
		if ( PanoMenu == NULL )
		{
			PanoMenu = OvrPanoMenu::Create( *GuiSys, *MetaData, 2.0f, 2.0f );
			OVR_ASSERT( PanoMenu );
			GuiSys->AddMenu( PanoMenu );
		}

		PanoMenu->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP );

		// Start building the FolderView
		Browser = ( PanoBrowser * )GuiSys->GetMenu( OvrFolderBrowser::MENU_NAME );
		if ( Browser == NULL )
		{
			Browser = PanoBrowser::Create(
				*this,
				*GuiSys,
				*MetaData,
				256, 20.0f,
				160, 220.0f,
				7,
				5.3f );
			OVR_ASSERT( Browser );
			GuiSys->AddMenu( Browser );
		}

		Browser->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_BACK_KEY_EXITS_APP );
		Browser->SetFolderTitleSpacingScale( 0.35f );
		Browser->SetScrollBarSpacingScale( 0.82f );
		Browser->SetScrollBarRadiusScale( 0.97f );
		Browser->SetPanelTextSpacingScale( 0.28f );
		Browser->OneTimeInit( *GuiSys );
		Browser->BuildDirtyMenu( *GuiSys, *MetaData );
		Browser->ReloadFavoritesBuffer( *GuiSys );

#if defined( OVR_OS_ANDROID )
		//---------------------------------------------------------
		// OpenGL initialization for shared context for
		// background loading thread done on the main thread
		//---------------------------------------------------------

		// Get values for the current OpenGL context
		EglDisplay = eglGetCurrentDisplay();
		if ( EglDisplay == EGL_NO_DISPLAY )
		{
			FAIL( "EGL_NO_DISPLAY" );
		}

		EglShareContext = eglGetCurrentContext();
		if ( EglShareContext == EGL_NO_CONTEXT )
		{
			FAIL( "EGL_NO_CONTEXT" );
		}

		EGLint attribList[] =
		{
			EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_NONE
		};

		EGLint numConfigs;
		if ( !eglChooseConfig( EglDisplay, attribList, &EglConfig, 1, &numConfigs ) )
		{
			FAIL( "eglChooseConfig failed" );
		}

		if ( EglConfig == NULL )
		{
			FAIL( "EglConfig NULL" );
		}
		if ( !eglQueryContext( EglDisplay, EglShareContext, EGL_CONTEXT_CLIENT_VERSION, ( EGLint * )&EglClientVersion ) )
		{
			FAIL( "eglQueryContext EGL_CONTEXT_CLIENT_VERSION failed" );
		}
		LOG( "Current EGL_CONTEXT_CLIENT_VERSION:%i", EglClientVersion );

		EGLint SurfaceAttribs [ ] =
		{
			EGL_WIDTH, 1,
			EGL_HEIGHT, 1,
			EGL_NONE
		};

		EglPbufferSurface = eglCreatePbufferSurface( EglDisplay, EglConfig, SurfaceAttribs );
		if ( EglPbufferSurface == EGL_NO_SURFACE ) {
			FAIL( "eglCreatePbufferSurface failed: %s", GL_GetErrorString() );
		}

		EGLint bufferWidth, bufferHeight;
		if ( !eglQuerySurface( EglDisplay, EglPbufferSurface, EGL_WIDTH, &bufferWidth ) ||
			!eglQuerySurface( EglDisplay, EglPbufferSurface, EGL_HEIGHT, &bufferHeight ) )
		{
			FAIL( "eglQuerySurface failed:  %s", GL_GetErrorString() );
		}
#endif

		loadingThread = Thread( Thread::CreateParams( &BackgroundGLLoadThread, this, 128 * 1024, -1, Thread::NotRunning, Thread::NormalPriority) );
		loadingThread.Start();

		// We might want to save the view state and position for perfect recall
	}
	else if ( intentType == INTENT_NEW )
	{
	}

	LOG( "Oculus360Photos::EnteredVrMode" );
	Browser->SetMenuPose( Posef() );
	PanoMenu->SetMenuPose( Posef() );
}

void Oculus360Photos::LeavingVrMode()
{
}

void * Oculus360Photos::BackgroundGLLoadThread( Thread * thread, void * v )
{
	thread->SetThreadName( "BackgrndGLLoad" );

	Oculus360Photos * photos = ( Oculus360Photos * )v;

#if defined( OVR_OS_ANDROID )
	// Create a new GL context on this thread, sharing it with the main thread context
	// so the loaded background texture can be passed.
	EGLint loaderContextAttribs [ ] =
	{
		EGL_CONTEXT_CLIENT_VERSION, photos->EglClientVersion,
		EGL_NONE, EGL_NONE,
		EGL_NONE
	};

	EGLContext EglBGLoaderContext = eglCreateContext( photos->EglDisplay, photos->EglConfig, photos->EglShareContext, loaderContextAttribs );
	if ( EglBGLoaderContext == EGL_NO_CONTEXT )
	{
		FAIL( "eglCreateContext failed: %s", GL_GetErrorString() );
	}

	// Make the context current on the window, so no more makeCurrent calls will be needed
	if ( eglMakeCurrent( photos->EglDisplay, photos->EglPbufferSurface, photos->EglPbufferSurface, EglBGLoaderContext ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglMakeCurrent failed: %s", GL_GetErrorString() );
	}
#endif

	// run until Shutdown requested
	for ( ; ; )
	{
		if ( photos->ShutdownRequest.GetState() )
		{
			LOG( "BackgroundGLLoadThread ShutdownRequest received" );
			break;
		}

		photos->BackgroundCommands.SleepUntilMessage();
		const char * msg = photos->BackgroundCommands.GetNextMessage();
		LOG( "BackgroundGLLoadThread Commands: %s", msg );
		if ( MatchesHead( "pano ", msg ) )
		{
			unsigned char * data;
			int width, height;
			sscanf( msg, "pano %p %i %i", &data, &width, &height );

			const double start = SystemClock::GetTimeInSeconds();

			// Resample oversize images so gl can load them.
			// We could consider resampling to GL_MAX_TEXTURE_SIZE exactly for better quality.
			GLint maxTextureSize = 0;
			glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );

			while ( width > maxTextureSize || width > maxTextureSize )
			{
				LOG( "Quartering oversize %ix%i image", width, height );
				unsigned char * newBuf = QuarterImageSize( data, width, height, false );
				free( data );
				data = newBuf;
				width >>= 1;
				height >>= 1;
			}

			photos->LoadRgbaTexture( data, width, height, photos->GetUseSrgb() );
			free( data );

			// Wait for the upload to complete.
			glFinish();

			photos->GetMessageQueue().PostPrintf( "%s", "loaded pano" );

			const double end = SystemClock::GetTimeInSeconds();
			LOG( "%4.2fs to load %ix%i res pano map", end - start, width, height );
		}
		else if ( MatchesHead( "cube ", msg ) )
		{
			unsigned char * data[ 6 ];
			int size;
			sscanf( msg, "cube %i %p %p %p %p %p %p", &size, &data[ 0 ], &data[ 1 ], &data[ 2 ], &data[ 3 ], &data[ 4 ], &data[ 5 ] );

			const double start = SystemClock::GetTimeInSeconds();

			photos->LoadRgbaCubeMap( size, data, photos->GetUseSrgb() );
			for ( int i = 0; i < 6; i++ )
			{
				free( data[ i ] );
			}

			// Wait for the upload to complete.
			glFinish();

			photos->GetMessageQueue().PostPrintf( "%s", "loaded cube" );

			const double end = SystemClock::GetTimeInSeconds();
			LOG( "%4.2fs to load %i res cube map", end - start, size );
		}
	}

#if defined( OVR_OS_ANDROID )
	// release the window so it can be made current by another thread
	if ( eglMakeCurrent( photos->EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglMakeCurrent: shutdown failed" );
	}

	if ( eglDestroyContext( photos->EglDisplay, EglBGLoaderContext ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglDestroyContext: shutdown failed" );
	}
#endif
	return NULL;
}

void Oculus360Photos::Command( const char * msg )
{
	if ( MatchesHead( "loaded pano", msg ) )
	{
		BackgroundPanoTexData.Swap();
		CurrentPanoIsCubeMap = false;
		SetMenuState( MENU_PANO_FADEIN );
		GuiSys->GetGazeCursor().ClearGhosts();
		return;
	}

	if ( MatchesHead( "loaded cube", msg ) )
	{
		BackgroundCubeTexData.Swap();
		CurrentPanoIsCubeMap = true;
		SetMenuState( MENU_PANO_FADEIN );
		GuiSys->GetGazeCursor().ClearGhosts();
		return;
	}
}

bool Oculus360Photos::GetUseSrgb() const
{
	return UseSrgb;
}

bool Oculus360Photos::GetUseOverlay() const
{
	// Don't enable the overlay when in throttled state
	return ( UseOverlay && !FrameInput.DeviceStatus.PowerLevelStateThrottled );
}

bool Oculus360Photos::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}

	if ( ( ( keyCode == OVR_KEY_BACK ) && ( eventType == KEY_EVENT_SHORT_PRESS ) ) ||
		 ( ( keyCode == OVR_KEY_BUTTON_B ) && ( eventType == KEY_EVENT_UP ) ) )
	{
		SetMenuState( MENU_BROWSER );
		return true;
	}

	return false;
}

void Oculus360Photos::LoadRgbaCubeMap( const int resolution, const unsigned char * const rgba[ 6 ], const bool useSrgbFormat )
{
	GL_CheckErrors( "enter LoadRgbaCubeMap" );

	// Create texture storage once
	ovrTextureSwapChain * chain = BackgroundCubeTexData.GetLoadTextureSwapChain();
	if ( chain == NULL || !BackgroundCubeTexData.SameSize( resolution, resolution ) )
	{
		vrapi_DestroyTextureSwapChain( chain );
		const ovrTextureFormat textureFormat = useSrgbFormat ?
												VRAPI_TEXTURE_FORMAT_8888_sRGB :
												VRAPI_TEXTURE_FORMAT_8888;
		chain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_CUBE, textureFormat, resolution, resolution,
										ComputeFullMipChainNumLevels( resolution, resolution ), false );

		BackgroundCubeTexData.SetLoadTextureSwapChain( chain );

		BackgroundCubeTexData.SetSize( resolution, resolution );
	}
	glBindTexture( GL_TEXTURE_CUBE_MAP, vrapi_GetTextureSwapChainHandle( chain, 0 ) );
	for ( int side = 0; side < 6; side++ )
	{
		glTexSubImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, 0, 0, 0, resolution, resolution, GL_RGBA, GL_UNSIGNED_BYTE, rgba[ side ] );
	}
	glGenerateMipmap( GL_TEXTURE_CUBE_MAP );
	glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

	GL_CheckErrors( "leave LoadRgbaCubeMap" );
}

void Oculus360Photos::LoadRgbaTexture( const unsigned char * data, int width, int height, const bool useSrgbFormat )
{
	GL_CheckErrors( "enter LoadRgbaTexture" );

	// Create texture storage once
	ovrTextureSwapChain * chain = BackgroundPanoTexData.GetLoadTextureSwapChain();
	if ( chain == NULL || !BackgroundPanoTexData.SameSize( width, height ) )
	{
		vrapi_DestroyTextureSwapChain( chain );
		const ovrTextureFormat textureFormat = useSrgbFormat ?
												VRAPI_TEXTURE_FORMAT_8888_sRGB :
												VRAPI_TEXTURE_FORMAT_8888;
		chain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, textureFormat, width, height,
										ComputeFullMipChainNumLevels( width, height ), false );

		BackgroundPanoTexData.SetLoadTextureSwapChain( chain );
		BackgroundPanoTexData.SetSize( width, height );
	}

	glBindTexture( GL_TEXTURE_2D, vrapi_GetTextureSwapChainHandle( chain, 0 ) );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data );
	glGenerateMipmap( GL_TEXTURE_2D );
	// Because equirect panos pinch at the poles so much,
	// they would pull in mip maps so deep you would see colors
	// from the opposite half of the pano.  Clamping the level avoids this.
	// A well filtered pano shouldn't have any high frequency texels
	// that alias near the poles.
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	GL_CheckErrors( "leave LoadRgbaTexture" );
}

Matrix4f CubeMatrixForViewMatrix( const Matrix4f & viewMatrix )
{
	Matrix4f m = viewMatrix;
	// clear translation
	for ( int i = 0; i < 3; i++ )
	{
		m.M[ i ][ 3 ] = 0.0f;
	}
	return m.Inverted();
}

void Oculus360Photos::StartBackgroundPanoLoad( const char * filename )
{
	LOG( "StartBackgroundPanoLoad( %s )", filename );

	// Queue1 will determine if this is a cube map and then post a message for each
	// cube face to the other queues.

	bool isCubeMap = strstr( filename, "_nz.jpg" ) != 0;
	char const * command = isCubeMap ? "cube" : "pano";

	// Dump any load that hasn't started
	Queue1.ClearMessages();

	// Start a background load of the current pano image
	Queue1.PostPrintf( "%s %s", command, filename );
}

void Oculus360Photos::SetMenuState( const OvrMenuState state )
{
	OvrMenuState lastState = MenuState;
	MenuState = state;
	LOG( "%s to %s", MenuStateString( lastState ), MenuStateString( MenuState ) );
	switch ( MenuState )
	{
	case MENU_NONE:
		break;
	case MENU_BROWSER:
		GuiSys->CloseMenu( PanoMenu, false );
		GuiSys->OpenMenu( OvrFolderBrowser::MENU_NAME );
		BrowserOpenTime = 0.0f;
		GuiSys->GetGazeCursor().ShowCursor();
		break;
	case MENU_PANO_LOADING:
		GuiSys->CloseMenu( Browser, false );
		GuiSys->OpenMenu( OvrPanoMenu::MENU_NAME );
		CurrentFadeRate = FadeOutRate;
		Fader.StartFadeOut();
		StartBackgroundPanoLoad( ActivePano->Url.ToCStr() );

		PanoMenu->UpdateButtonsState( *GuiSys, ActivePano );
		break;
	// pano menu now to fully open
	case MENU_PANO_FADEIN:
	case MENU_PANO_REOPEN_FADEIN:
		if ( lastState != MENU_BROWSER )
		{
			GuiSys->OpenMenu( OvrPanoMenu::MENU_NAME );
			GuiSys->CloseMenu( Browser, false );
		}
		else
		{
			GuiSys->OpenMenu( OvrFolderBrowser::MENU_NAME );
			GuiSys->CloseMenu( PanoMenu, false );
		}
		GuiSys->GetGazeCursor().ShowCursor();
		Fader.Reset();
		CurrentFadeRate = FadeInRate;
		Fader.StartFadeIn();
		break;
	case MENU_PANO_FULLY_VISIBLE:
		PanoMenuTimeLeft = PanoMenuVisibleTime;
		break;
	case MENU_PANO_FADEOUT:
		PanoMenu->StartFadeOut();
		break;
	default:
		OVR_ASSERT( false );
		break;
	}
}

void Oculus360Photos::OnPanoActivated( const OvrMetaDatum * panoData )
{
	ActivePano = static_cast< const OvrPhotosMetaDatum * >( panoData );
	Browser->ReloadFavoritesBuffer( *GuiSys );
	SetMenuState( MENU_PANO_LOADING );
}

ovrFrameResult Oculus360Photos::Frame( const ovrFrameInput & vrFrame )
{
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
    for ( int i = 0; i < vrFrame.Input.NumKeyEvents; i++ )
    {
        const int keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
        const int repeatCount = vrFrame.Input.KeyEvents[i].RepeatCount;
        const KeyEventType eventType = vrFrame.Input.KeyEvents[i].EventType;

        if ( OnKeyEvent( keyCode, repeatCount, eventType ) )
        {
            continue;   // consumed the event
        }
        // If nothing consumed the key and it's a short-press of the back key, then exit the application to OculusHome.
        if ( keyCode == OVR_KEY_BACK && eventType == KEY_EVENT_SHORT_PRESS )
        {
            app->ShowSystemUI( VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
            continue;
        }
   	}
	// Process incoming messages until the queue is empty.
	for ( ; ; )
	{
		const char * msg = MessageQueue.GetNextMessage();
		if ( msg == NULL )
		{
			break;
		}
		Command( msg );
		free( (void *)msg );
	}

	FrameInput = vrFrame;

	// if just starting up, begin loading a background image
	if ( !StartupPano.IsEmpty() )
	{
		StartBackgroundPanoLoad( StartupPano.ToCStr() );
		SetMenuState( MENU_BROWSER );
		StartupPano.Clear();
	}

	// disallow player movement
	ovrFrameInput vrFrameWithoutMove = FrameInput;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 0 ] = 0.0f;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 1 ] = 0.0f;

	Scene.Frame( vrFrameWithoutMove );

	ovrFrameResult res;
	Scene.GetFrameMatrices( vrFrameWithoutMove.FovX, vrFrameWithoutMove.FovY, res.FrameMatrices );
	Scene.GenerateFrameSurfaceList( res.FrameMatrices, res.Surfaces );

	// reopen PanoMenu when in pano
	if ( ActivePano && Browser->IsClosedOrClosing() && ( MenuState != MENU_PANO_LOADING ) )
	{
		// single touch
		if ( MenuState > MENU_PANO_FULLY_VISIBLE && FrameInput.Input.buttonPressed & ( BUTTON_TOUCH_SINGLE | BUTTON_A ) )
		{
			SetMenuState( MENU_PANO_REOPEN_FADEIN );
		}

		// PanoMenu input - needs to swipe even when PanoMenu is closed and in pano
		const OvrPhotosMetaDatum * nextPano = NULL;

		if ( FrameInput.Input.buttonPressed & ( BUTTON_SWIPE_BACK | BUTTON_DPAD_LEFT | BUTTON_LSTICK_LEFT ) )
		{
			nextPano = static_cast< const OvrPhotosMetaDatum * >( Browser->NextFileInDirectory( *GuiSys, -1 ) );
		}
		else if ( FrameInput.Input.buttonPressed & ( BUTTON_SWIPE_FORWARD | BUTTON_DPAD_RIGHT | BUTTON_LSTICK_RIGHT ) )
		{
			nextPano = static_cast< const OvrPhotosMetaDatum * >( Browser->NextFileInDirectory( *GuiSys, 1 ) );
		}

		if ( nextPano && ( ActivePano != nextPano ) )
		{
			PanoMenu->RepositionMenu( app->GetLastViewMatrix() );
			SoundEffectContext->Play( "sv_release_active" );
			SetActivePano( nextPano );
			SetMenuState( MENU_PANO_LOADING );
		}
	}

	// State transitions
	if ( Fader.GetFadeState() != Fader::FADE_NONE )
	{
		Fader.Update( CurrentFadeRate, FrameInput.DeltaSeconds );
		if ( MenuState != MENU_PANO_REOPEN_FADEIN )
		{
			CurrentFadeLevel = Fader.GetFinalAlpha();
		}
	}
	else if ( ( MenuState == MENU_PANO_FADEIN || MenuState == MENU_PANO_REOPEN_FADEIN ) &&
				Fader.GetFadeAlpha() == 1.0 )
	{
		SetMenuState( MENU_PANO_FULLY_VISIBLE );
	}

	if ( MenuState == MENU_PANO_FULLY_VISIBLE )
	{
		if ( !PanoMenu->Interacting() )
		{
			if ( PanoMenuTimeLeft > 0.0f )
			{
				PanoMenuTimeLeft -= FrameInput.DeltaSeconds;
			}
			else
			{
				PanoMenuTimeLeft = 0.0f;
				SetMenuState( MENU_PANO_FADEOUT );
			}
		}
		else // Reset PanoMenuTimeLeft
		{
			PanoMenuTimeLeft = PanoMenuVisibleTime;
		}
	}

	// update gui systems after the app frame, but before rendering anything
	GuiSys->Frame( FrameInput, res.FrameMatrices.CenterView );

	//-------------------------------
	// Rendering
	//-------------------------------

	// Dim pano when browser open
	float fadeColor = CurrentFadeLevel;
	if ( Browser->IsOpenOrOpening() || MenuState == MENU_PANO_LOADING )
	{
		fadeColor *= 0.09f;
	}

	res.FrameIndex = FrameInput.FrameNumber;
	res.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2 & worldLayer = res.Layers[ res.LayerCount++ ].Projection;
	worldLayer = vrapi_DefaultLayerProjection2();

	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
	worldLayer.Header.Flags |= ( UseSrgb ? 0 : VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER );
	worldLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
	worldLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;

	worldLayer.HeadPose = FrameInput.Tracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		worldLayer.Textures[eye].ColorSwapChain = FrameInput.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = FrameInput.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = FrameInput.TexCoordsFromTanAngles;
	}

	if ( GetUseOverlay() && CurrentPanoIsCubeMap )
	{
		// Clear the eye buffers to 0 alpha so the overlay plane shows through.
		res.ClearColorBuffer = true;
		res.ClearColor = Vector4f( 0.0f, 0.0f, 0.0f, 0.0f );

		{
			worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_WRITE_ALPHA;
			worldLayer.Header.Flags &= ~VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
		}
		{
			ovrLayerProjection2 & overlayLayer = res.Layers[ res.LayerCount++ ].Projection;
			overlayLayer = vrapi_DefaultLayerProjection2();

			overlayLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
			overlayLayer.Header.Flags |= ( UseSrgb ? 0 : VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER );
			overlayLayer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_DST_ALPHA;
			overlayLayer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_DST_ALPHA;
			overlayLayer.Header.ColorScale = { fadeColor, fadeColor, fadeColor, fadeColor };

			overlayLayer.HeadPose = FrameInput.Tracking.HeadPose;
			for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
			{
				overlayLayer.Textures[eye].ColorSwapChain = BackgroundCubeTexData.GetRenderTextureSwapChain();
				overlayLayer.Textures[eye].SwapChainIndex = 0;
				overlayLayer.Textures[eye].TexCoordsFromTanAngles = CubeMatrixForViewMatrix( res.FrameMatrices.CenterView );
			}
		}
	}
	else
	{
		DoubleBufferedTextureData & db = CurrentPanoIsCubeMap ? BackgroundCubeTexData : BackgroundPanoTexData;
		ovrTextureSwapChain * chain = db.GetRenderTextureSwapChain();

		GlobeProgramTexture = GlTexture( vrapi_GetTextureSwapChainHandle( chain, 0 ),
										 CurrentPanoIsCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, 0, 0 );
		GlobeProgramColor = Vector4f( fadeColor, fadeColor, fadeColor, fadeColor );
		GlobeSurfaceDef.graphicsCommand.Program = CurrentPanoIsCubeMap ? CubeMapPanoProgram : TexturedMvpProgram;
		GlobeSurfaceDef.graphicsCommand.UniformData[0].Data = &GlobeProgramColor;
		GlobeSurfaceDef.graphicsCommand.UniformData[1].Data = &GlobeProgramTexture;

		res.Surfaces.PushBack( ovrDrawSurface( &GlobeSurfaceDef ) );

		worldLayer.Header.Flags &= ~VRAPI_FRAME_LAYER_FLAG_WRITE_ALPHA;
	}

	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );

	GL_CheckErrors( "draw" );

	return res;
}

const char * menuStateNames[] =
{
	"MENU_NONE",
	"MENU_BROWSER",
	"MENU_PANO_LOADING",
	"MENU_PANO_FADEIN",
	"MENU_PANO_REOPEN_FADEIN",
	"MENU_PANO_FULLY_VISIBLE",
	"MENU_PANO_FADEOUT",
	"NUM_MENU_STATES"
};

const char * Oculus360Photos::MenuStateString( const OvrMenuState state )
{
	OVR_ASSERT( state >= 0 && state < NUM_MENU_STATES );
	return menuStateNames[ state ];
}

int Oculus360Photos::ToggleCurrentAsFavorite()
{
	// Save MetaData -
	TagAction result = MetaData->ToggleTag( const_cast< OvrPhotosMetaDatum * >( ActivePano ), "Favorites" );

	switch ( result )
	{
	case TAG_ADDED:
		Browser->AddToFavorites( *GuiSys, ActivePano );
		break;
	case TAG_REMOVED:
		Browser->RemoveFromFavorites( *GuiSys, ActivePano );
		break;
	case TAG_ERROR:
	default:
		OVR_ASSERT( false );
		break;
	}

	return result;
}

int Oculus360Photos::GetNumPanosInActiveCategory( OvrGuiSys & guiSys ) const
{
	return Browser->GetNumPanosInActive( guiSys );
}

bool Oculus360Photos::AllowPanoInput() const
{
	return Browser->IsClosed() && MenuState == MENU_PANO_FULLY_VISIBLE;
}

} // namespace OVR
