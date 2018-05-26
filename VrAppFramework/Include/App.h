/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
                Applications that use this framework should NOT include any VrApi
                headers other than VrApi_Types.h and VrApi_Helpers.h included here.
                The framework should provide a complete superset of the VrApi.
                Avoid including this header file as much as possible in the
                framework, so individual components are not tied to the native
                application framework, and can be reused more easily by Unity
                or other hosting applications.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_App_h
#define OVR_App_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi_Types.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "OVR_GlUtils.h"
#include "GlProgram.h"
#include "GlTexture.h"
#include "GlGeometry.h"
#include "SurfaceRender.h"
#include "SurfaceTexture.h"
#include "EyeBuffers.h"
#include "VrCommon.h"
#include "OVR_Input.h"
#include "TalkToJava.h"
#include "Console.h"

namespace OVR
{

//==============================================================
// forward declarations
class OvrDebugLines;
class App;
class OvrStoragePaths;
class ovrFileSys;
class ovrTextureManager;

enum ovrIntentType
{
	INTENT_LAUNCH,
	INTENT_NEW,
	INTENT_OLD
};

enum ovrRenderMode
{
	RENDERMODE_TYPE_MONO_MASK			= 0x0200,	// DO NOTE USE: Mask-only.

	// These Render Modes assume the application is rendering via the
	// surface list generation in Frame() path which is required for 
	// Multiview support.
	RENDERMODE_TYPE_FRAME_SURFACES_MASK	= 0x1000,	// DO NOT USE: Mask-only
	RENDERMODE_STEREO					= 0x1100,	// Render both eyes views.
	RENDERMODE_MONO						= 0x1200,	// Render a single eye view and use the same image for both Timewarp eyes.
	RENDERMODE_MULTIVIEW				= 0x1400,	// Render both eye views simultaneously.
};

struct ovrFence
{
	EGLDisplay	Display;
	// Note: These sync objects must be EGLSyncKHR because the VrApi still supports OpenGL ES 2.0.
	EGLSyncKHR	Sync;
};

#if defined( OVR_OS_WIN32 )
struct ovrWindowCreationParms
{
	unsigned int	IconResourceId;
	String			Title;
};
#endif

// Passed to an application to configure various VR settings.
struct ovrSettings
{
	bool				ShowLoadingIcon;
	bool				UseSrgbFramebuffer;			// EGL_GL_COLORSPACE_KHR,  EGL_GL_COLORSPACE_SRGB_KHR
	bool				UseProtectedFramebuffer;	// EGL_PROTECTED_CONTENT_EXT, EGL_TRUE
	bool				Use16BitFramebuffer;		// Instead of 32 bit, can only be changed on first launch
	int					SwapInterval;
	ovrModeParms		ModeParms;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	ovrTrackingTransform TrackingTransform;			// Default is VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_FLOOR_LEVEL
	ovrEyeBufferParms	EyeBufferParms;
	ovrRenderMode		RenderMode;					// Default is RENDERMODE_STEREO.
#if defined( OVR_OS_WIN32 )
	ovrWindowCreationParms	WindowParms;
#endif
};

struct ovrFrameMatrices
{
	Matrix4f	CenterView;			// the view transform for the point between the eyes
	Matrix4f	EyeView[2];			// the view transforms for each of the eyes
	Matrix4f	EyeProjection[2];	// the projection transforms for each of the eyes
};

class ovrFrameResult
{
public:
	ovrFrameResult()
		: Layers()
		, LayerCount( 0 )
		, FrameFlags( 0 )
		, FrameIndex( 0 )
		, DisplayTime( 0 )
		, SwapInterval( 1 )
		, FrameMatrices()
		, ClearColorBuffer( false )
		, ClearDepthBuffer( false )
		, ClearColor( 1.0f, 0.0f, 1.0f, 1.0f )
		, TexRectLayer( -1 )
	{
	}

	ovrLayer_Union2				Layers[ovrMaxLayerCount];
	int							LayerCount;
	int							FrameFlags;
	long long					FrameIndex;
	double						DisplayTime;
	int							SwapInterval;
	ovrFrameMatrices			FrameMatrices;		// view and projection transforms
	Array< ovrDrawSurface > 	Surfaces;			// list of surfaces to render
	bool						ClearColorBuffer;	// true if the app wants to color buffer cleared
	bool						ClearDepthBuffer;	// true if the app wants the depth buffer cleared
	Vector4f					ClearColor;			// color to clear the depth buffer to
	int							TexRectLayer;		// if non-negative, the layer for which a texRect
													// covering the surface list is calculated.
};

/*

VrAppInterface
==============

The VrAppInterface class implements the application life cycle. A class that implements
an application is derived from VrAppInterface. The application then receives life cycle
events by implementing the virtual functions from the VrAppInterface class. The virtual
functions of the VrAppInterface will be called by the application framework. An application
should never call these virtual functions itself.

All of the VrAppInterface virtual functions will be called from the VR application thread.
All functions except for VrAppInterface::Configure() will be called while the application
is in VR mode. An applications that does not care about a particular virtual function,
is not required to implement it.

VrAppInterface life cycle
-------------------------

                                  <--------+
    1.  Configure(settings)                |
    2.  EnteredVrMode()                    |
                                    <--+   |
    3.  while(keyEvent) OnKeyEvent()   |   |
    4.  Frame()                        |   |
                                    ---+   |
    5.  LeavingVrMode()                    |
                                  ---------+

*/
class VrAppInterface
{
public:
							VrAppInterface();
	virtual					~VrAppInterface();

	// Each onCreate in java will allocate a new java object.
	jlong SetActivity( JNIEnv * jni, jclass clazz, jobject activity, 
					   jstring javaFromPackageNameString,
					   jstring javaCommandString, 
					   jstring javaUriString );

	// All subclasses communicate with App through this member.
	App *	app;				// Allocated in the first call to SetActivity()
	jclass	ActivityClass;		// global reference to clazz passed to SetActivity

	// This is called on each resume, before entering VR Mode, to allow
	// the application to make changes.
	virtual void Configure( ovrSettings & settings );

	// This will be called right after entering VR mode.
	// The intent is either a launch intent, a new intent, or a repeat of the last intent.
	virtual void EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI );

	// This will be called right before leaving VR mode.
	virtual void LeavingVrMode();

	// Frame will only be called if the window surfaces have been created.
	//
	// Any GPU operations that are relevant for both eye views, like
	// buffer updates or dynamic texture rendering, should be done first.
	//
	// NOTE: Be sure not to free any resources associated with surfaces
	// appended to the FrameResult Surfaces list while a Frame render
	// operation is in flight.
	virtual ovrFrameResult Frame( const ovrFrameInput & vrFrame );
};

//==============================================================
// App
class App : public TalkToJavaInterface
{
public:
	enum ovrAppFatalError
	{
		FATAL_ERROR_OUT_OF_MEMORY,
		FATAL_ERROR_OUT_OF_STORAGE,
		FATAL_ERROR_MISC,
		FATAL_ERROR_MAX
	};

	enum ovrAppFinishType
	{
		FINISH_NORMAL,		// This will finish the current activity.
		FINISH_AFFINITY		// This will finish all activities on the stack.
	};

	virtual						~App();

	// When VrAppInterface::SetActivity is called, the App member is
	// allocated and hooked up to the VrAppInterface.
	virtual VrAppInterface *	GetAppInterface() = 0;

	// Reorients while optionally pushing a black frame. Additionally, recenters
	// the LastViewMatrix yaw.
	virtual void				RecenterYaw( const bool showBlack ) = 0;
	// Enables reorient before sensor data is read.
	// Allows apps to reorient without having invalid orientation information for that frame.
	virtual void				SetRecenterYawFrameStart( const long long frameNumber ) = 0;
	virtual long long			GetRecenterYawFrameStart() const = 0;

	// Send an intent to another activity.
	virtual void				SendIntent( const char * actionName, const char * toPackageName,
											const char * toClassName, const char * command, const char * uri ) = 0;
	// Launch another package.
	// The destination package will be queried for its launch intent. If action is not NULL, then
	// the intent's action will be set to the string pointed to by action.
	virtual void				SendLaunchIntent( const char * toPackageName, const char * command, const char * uri,
												  const char * action ) = 0;

	// End Activity.
	virtual void				FinishActivity( const ovrAppFinishType type ) = 0;

	// Switch to System UI, display Fatal Errors.
	virtual bool				ShowSystemUI( const ovrSystemUIType type ) = 0;
	virtual void				FatalError( const ovrAppFatalError error, const char * fileName, const unsigned int lineNumber,
											const char * messageFormat, ... ) = 0;
	virtual void				ShowDependencyError() = 0;

	//-----------------------------------------------------------------
	// interfaces
	//-----------------------------------------------------------------
	virtual OvrDebugLines &     	GetDebugLines() = 0;
	virtual const OvrStoragePaths &	GetStoragePaths() = 0;

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------

	virtual int						GetSystemProperty( const ovrSystemProperty propType ) = 0;
	virtual int						GetSystemStatus( const ovrSystemStatus statusType ) = 0;
	virtual const VrDeviceStatus &	GetDeviceStatus() const = 0;

	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	// The parms will be initialized to sensible values on startup, and
	// can be freely changed at any time with minimal overhead.
	virtual	const ovrEyeBufferParms &	GetEyeBufferParms() const = 0;
	virtual void						SetEyeBufferParms( const ovrEyeBufferParms & parms ) = 0;

	virtual int							GetSwapInterval() const = 0;
	virtual void						SetSwapInterval( const int swapInterval ) = 0;

	// The framebuffer may not be setup the way it was requested through VrAppInterface::Configure().
	virtual bool						GetFramebufferIsSrgb() const = 0;
	virtual bool						GetFramebufferIsProtected() const = 0;
	
	virtual Matrix4f const &			GetLastViewMatrix() const = 0;
	virtual void						SetLastViewMatrix( Matrix4f const & m ) = 0;
	virtual void						RecenterLastViewMatrix() = 0;

	virtual ovrMobile *					GetOvrMobile() = 0;
	virtual ovrFileSys &				GetFileSys() = 0;
	// it's possible that this could return NULL if it's called before InitGLObjects()
	virtual	ovrTextureManager *			GetTextureManager() = 0;

	//-----------------------------------------------------------------
	// Localization
	//-----------------------------------------------------------------
	virtual const char *				GetPackageName() const = 0;
	// Returns the path to an installed package in outPackagePath. If the call fails or
	// the outPackagePath buffer is too small to hold the package path, false is returned.
	virtual bool						GetInstalledPackagePath( char const * packageName, 
												char * outPackagePath, size_t const outMaxSize ) const = 0;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual const ovrJava *				GetJava() const = 0;
	virtual jclass	&					GetVrActivityClass() = 0;

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------

	virtual void						RegisterConsoleFunction( char const * name, consoleFn_t function ) = 0;
};

}	// namespace OVR

#endif	// OVR_App
