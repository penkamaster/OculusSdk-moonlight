/************************************************************************************

Filename    :   App.h
Content     :   Native counterpart to VrActivity
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_AppLocal_h
#define OVR_AppLocal_h

#include "App.h"
#include "GlSetup.h"
#include "PointTracker.h"
#include "VrFrameBuilder.h"
#include "Kernel/OVR_Threads.h"

namespace OVR {

//==============================================================
// AppLocal
//
// NOTE: do not define any of the functions in here inline (inside of the class 
// definition).  When AppLocal.h is included in multiple files (App.cpp and AppRender.cpp)
// this causes bugs with accessor functions.
class AppLocal : public App
{
public:
								AppLocal( JNIEnv & jni_,
										  jobject activityObject_,
										  VrAppInterface & interface_ );
	virtual						~AppLocal();

	virtual VrAppInterface *	GetAppInterface();

	virtual void				RecenterYaw( const bool showBlack );
	virtual void				SetRecenterYawFrameStart( const long long frameNumber );
	virtual long long			GetRecenterYawFrameStart() const;

	virtual void				SendIntent( const char * actionName, const char * toPackageName,
											const char * toClassName, const char * command, const char * uri );
	virtual void				SendLaunchIntent( const char * toPackageName, const char * command, const char * uri,
												  const char * action );

	virtual void				FinishActivity( const ovrAppFinishType type );

	virtual bool				ShowSystemUI( const ovrSystemUIType type );
	virtual void				FatalError( const ovrAppFatalError error, const char * fileName, const unsigned int lineNumber,
											const char * messageFormat, ... );
	virtual void				ShowDependencyError();

	//-----------------------------------------------------------------
	// interfaces
	//-----------------------------------------------------------------

	virtual OvrDebugLines &     	GetDebugLines();
	virtual const OvrStoragePaths & GetStoragePaths();

	//-----------------------------------------------------------------
	// system settings
	//-----------------------------------------------------------------

	virtual int						GetSystemProperty( const ovrSystemProperty propType );
	virtual int						GetSystemStatus( const ovrSystemStatus statusType );
	virtual const VrDeviceStatus &	GetDeviceStatus() const;

	//-----------------------------------------------------------------
	// accessors
	//-----------------------------------------------------------------

	virtual	const ovrEyeBufferParms &	GetEyeBufferParms() const;
	virtual void						SetEyeBufferParms( const ovrEyeBufferParms & parms );

	virtual int							GetSwapInterval() const;
	virtual void						SetSwapInterval( const int swapInterval );

	virtual	bool						GetFramebufferIsSrgb() const;
	virtual bool						GetFramebufferIsProtected() const;

	virtual Matrix4f const &			GetLastViewMatrix() const;
	virtual void						SetLastViewMatrix( Matrix4f const & m );
	virtual void						RecenterLastViewMatrix();

	virtual ovrMobile *					GetOvrMobile();
	virtual ovrFileSys &				GetFileSys();
	virtual	ovrTextureManager *			GetTextureManager();

	//-----------------------------------------------------------------
	// Localization
	//-----------------------------------------------------------------

	virtual const char *				GetPackageName() const;
	// Returns the path to an installed package in outPackagePath. If the call fails or
	// the outPackagePath buffer is too small to hold the package path, false is returned.
	virtual bool						GetInstalledPackagePath( char const * packageName, 
												char * outPackagePath, size_t const outMaxSize ) const;

	//-----------------------------------------------------------------
	// Java accessors
	//-----------------------------------------------------------------

	virtual const ovrJava *				GetJava() const;
	virtual jclass	&					GetVrActivityClass();

	//-----------------------------------------------------------------
	// debugging
	//-----------------------------------------------------------------

	virtual void						RegisterConsoleFunction( char const * name, consoleFn_t function );

	// Public functions and variables used by native function calls from Java.
public:
	ovrMessageQueue &	GetMessageQueue();
	void				SetActivity( JNIEnv * jni, jobject activity );
	void				StartVrThread();
	void				StopVrThread();
	void *				JoinVrThread(); // the return value is the void * returned from VrThreadFunction

	// Primary apps will exit(0) when they get an onDestroy() so we
	// never leave any cpu-sucking process running, but some apps
	// need to just return to the primary activity.
	volatile bool			ExitOnDestroy;
	volatile ovrIntentType	IntentType;

	ANativeWindow *		pendingNativeWindow;

private:
	bool				VrThreadSynced;
	bool				ReadyToExit;				// start exit procedure
	bool				Resumed;

	VrAppInterface *	appInterface;

	// Most calls from java should communicate with the VrThread through this message queue.
	ovrMessageQueue		MessageQueue;

	// gl setup information
	glSetup_t			glSetup;

	// window surface
	ANativeWindow *		nativeWindow;
	bool				FramebufferIsSrgb;			// requires KHR_gl_colorspace
	bool				FramebufferIsProtected;		// requires GPU trust zone extension

	bool				UseMultiview;

	bool				GraphicsObjectsInitialized;

	// From vrapi_EnterVrMode, used for vrapi_SubmitFrame and vrapi_LeaveVrMode
	ovrMobile *			OvrMobile;

	// Handles creating, destroying, and re-configuring the buffers
	// for drawing the eye views, which might be in different texture
	// configurations for CPU warping, etc.
	ovrEyeBuffers *		EyeBuffers;

	static const int MAX_FENCES = 4;
	ovrFence *			CompletionFences;
	int					CompletionFenceIndex;

	ovrJava				Java;
	jclass				VrActivityClass;

	jmethodID			finishActivityMethodId;

	String				IntentURI;					// URI app was launched with
	String				IntentJSON;					// extra JSON data app was launched with
	String				IntentFromPackage;			// package that sent us the launch intent

	String				PackageName;				// package name 

	Matrix4f			LastViewMatrix;

	float				SuggestedEyeFovDegreesX;
	float				SuggestedEyeFovDegreesY;

	ovrInputEvents		InputEvents;
	VrFrameBuilder		TheVrFrame;					// passed to VrAppInterface::Frame()
	long long			EnteredVrModeFrame;			// frame number when VR mode was last entered

	ovrSettings			VrSettings;					// passed to VrAppInterface::Configure()

	ovrSurfaceRender	SurfaceRender;

	Thread				VrThread;					// thread
	int32_t				ExitCode;					// returned from JoinVrThread

#if defined( OVR_OS_ANDROID )
	// For running java commands on another thread to
	// avoid hitches.
	TalkToJava			Ttj;
#endif

	long long 			RecenterYawFrameStart;	// Allows apps to reorient without having invalid orientation information for that frame.

	OvrDebugLines *		DebugLines;
	OvrStoragePaths *	StoragePaths;

	ovrTextureSwapChain *	LoadingIconTextureChain;
	ovrTextureSwapChain *	ErrorTextureSwapChain;
	int						ErrorTextureSize;
	double					ErrorMessageEndTime;

	ovrFileSys *		FileSys;
	ovrTextureManager *	TextureManager;

	//-----------------------------------------------------------------

	// Read a file from the apk zip file.  Free buffer with free() when done.
	// Files put in the eclipse res/raw directory will be found as "res/raw/<NAME>"
	// Files put in the eclipse assets directory will be found as "assets/<name>"
	// The filename comparison is case insensitive.
	void 				ReadFileFromApplicationPackage( const char * nameInZip, int &length, void * & buffer );

	//-----------------------------------------------------------------

	void				FrameworkInputProcessing( const VrInput & input );

	// One time init of GL objects.
	void				InitGlObjects();
	void				ShutdownGlObjects();

	void 				DrawEyeViews( ovrFrameResult & res );
	void				DrawBlackFrame( const int frameFlags = 0 );
	void				DrawLoadingIcon( ovrTextureSwapChain * swapChain, const float spinSpeed = 1.0f, const float spinScale = 16.0f );

	void				CreateFence( ovrFence * fence );
	void				DestroyFence( ovrFence * fence );
	void				InsertFence( ovrFence * fence );

	static threadReturn_t ThreadStarter( Thread *, void * parm );
	void *				VrThreadFunction();

	// Process commands forwarded from other threads.
	// Commands can be processed even when the window surfaces
	// are not setup.
	//
	// The msg string will be freed by the framework after
	// command processing.
	void    			Command( const char * msg );

	// Android Activity/Surface life cycle handling.
	void				Configure();
	void				EnterVrMode();
	void				LeaveVrMode();
	void				HandleVrModeChanges();
	
	// TalkToJavaInterface
	virtual void		TtjCommand( JNIEnv & jni, const char * commandString );

	void				BuildVrFrame();
};

} // namespace OVR

#endif // OVR_AppLocal_h
