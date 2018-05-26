/************************************************************************************

Filename    :   App_Android.cpp
Content     :   Native counterpart to VrActivity and VrApp
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"
#include "AppLocal.h"

#if defined( OVR_OS_ANDROID )

#include "Android/JniUtils.h"

#include "VrApi.h"
#include "VrApi_Helpers.h"

#include <android/native_window_jni.h>	// for native window JNI
#include "OVR_Input.h"

// do not queue input events if less than this number of slots are available in the
// message queue. This prevents input from overflowing the queue if the message
// queue is not be serviced by VrThreadFunction.
static const int	MIN_SLOTS_AVAILABLE_FOR_INPUT = 12;

void ComposeIntentMessage( char const * packageName, char const * uri, char const * jsonText, 
		char * out, size_t outSize );

extern "C"
{

// The JNIEXPORT macro prevents the functions from ever being stripped out of the library.

void Java_com_oculus_vrappframework_VrApp_nativeOnCreate( JNIEnv *jni, jclass clazz, jobject activity )
{
	ovrJava java;
	jni->GetJavaVM( &java.Vm );
	java.Env = jni;
	java.ActivityObject = activity;

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		LOG( "vrapi_Initialize failed" );
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}
}

void Java_com_oculus_vrappframework_VrApp_nativeOnPause( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p nativePause", (void *)appPtr );
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	appLocal->GetMessageQueue().SendPrintf( "pause " );
}

void Java_com_oculus_vrappframework_VrApp_nativeOnResume( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p nativeResume", (void *)appPtr );
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	appLocal->GetMessageQueue().SendPrintf( "resume " );
}

void Java_com_oculus_vrappframework_VrApp_nativeOnDestroy( JNIEnv *jni, jclass clazz,
		jlong appPtr )
{
	LOG( "%p nativeDestroy", (void *)appPtr );

	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	const bool exitOnDestroy = appLocal->ExitOnDestroy;

	appLocal->StopVrThread();
	appLocal->SetActivity( jni, NULL );
	delete appLocal;

	vrapi_Shutdown();

	if ( exitOnDestroy )
	{
		LOG( "ExitOnDestroy is true, exiting" );
		exit( 0 );	// FIXME: is this still needed?
	}
	else
	{
		LOG( "ExitOnDestroy was false, returning normally." );
	}
}

void Java_com_oculus_vrappframework_VrApp_nativeSurfaceCreated( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceCreated( %p )", (void *)appPtr, surface );

	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( jni, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		WARN( "        Surface not in landscape mode!" );
	}

	LOG( "    pendingNativeWindow = ANativeWindow_fromSurface( jni, surface )" );
	appLocal->pendingNativeWindow = newNativeWindow;
	appLocal->GetMessageQueue().SendPrintf( "surfaceCreated " );
}

void Java_com_oculus_vrappframework_VrApp_nativeSurfaceChanged( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceChanged( %p )", (void *)appPtr, surface );

	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( jni, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		WARN( "        Surface not in landscape mode!" );
	}

	if ( newNativeWindow != appLocal->pendingNativeWindow )
	{
		if ( appLocal->pendingNativeWindow != NULL )
		{
			appLocal->GetMessageQueue().SendPrintf( "surfaceDestroyed " );
			LOG( "    ANativeWindow_release( pendingNativeWindow )" );
			ANativeWindow_release( appLocal->pendingNativeWindow );
			appLocal->pendingNativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			LOG( "    pendingNativeWindow = ANativeWindow_fromSurface( jni, surface )" );
			appLocal->pendingNativeWindow = newNativeWindow;
			appLocal->GetMessageQueue().SendPrintf( "surfaceCreated " );
		}
	}
	else if ( newNativeWindow != NULL )
	{
		ANativeWindow_release( newNativeWindow );
	}
}

void Java_com_oculus_vrappframework_VrApp_nativeSurfaceDestroyed( JNIEnv *jni, jclass clazz,
		jlong appPtr, jobject surface )
{
	LOG( "%p nativeSurfaceDestroyed()", (void *)appPtr );

	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;

	appLocal->GetMessageQueue().SendPrintf( "surfaceDestroyed " );
	LOG( "    ANativeWindow_release( %p )", appLocal->pendingNativeWindow );
	ANativeWindow_release( appLocal->pendingNativeWindow );
	appLocal->pendingNativeWindow = NULL;
}

void Java_com_oculus_vrappframework_VrActivity_nativeJoypadAxis( JNIEnv *jni, jclass clazz,
		jlong appPtr, jfloat lx, jfloat ly, jfloat rx, jfloat ry )
{
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	// Suspend input until EnteredVrMode( INTENT_LAUNCH ) has finished to avoid overflowing the message queue on long loads.
	if ( appLocal->IntentType != OVR::INTENT_LAUNCH )
	{
		appLocal->GetMessageQueue().PostPrintfIfSpaceAvailable( MIN_SLOTS_AVAILABLE_FOR_INPUT, "joy %f %f %f %f", lx, ly, rx, ry );
	}
}

void Java_com_oculus_vrappframework_VrActivity_nativeTouch( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint action, jfloat x, jfloat y )
{
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	// Suspend input until EnteredVrMode( INTENT_LAUNCH ) has finished to avoid overflowing the message queue on long loads.
	if ( appLocal->IntentType != OVR::INTENT_LAUNCH )
	{
		appLocal->GetMessageQueue().PostPrintfIfSpaceAvailable( MIN_SLOTS_AVAILABLE_FOR_INPUT, "touch %i %f %f", action, x, y );
	}
}

void Java_com_oculus_vrappframework_VrActivity_nativeKeyEvent( JNIEnv *jni, jclass clazz,
		jlong appPtr, jint key, jboolean down, jint repeatCount )
{
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	// Suspend input until EnteredVrMode( INTENT_LAUNCH ) has finished to avoid overflowing the message queue on long loads.
	if ( appLocal->IntentType != OVR::INTENT_LAUNCH )
	{
		OVR::ovrKeyCode keyCode = OVR::OSKeyToKeyCode( key );
		//LOG( "nativeKeyEvent: key = %i, keyCode = %i, down = %s, repeatCount = %i", key, keyCode, down ? "true" : "false", repeatCount );
		appLocal->GetMessageQueue().PostPrintfIfSpaceAvailable( MIN_SLOTS_AVAILABLE_FOR_INPUT, "key %i %i %i", keyCode, down, repeatCount );
	}
}

void Java_com_oculus_vrappframework_VrActivity_nativeNewIntent( JNIEnv *jni, jclass clazz,
		jlong appPtr, jstring fromPackageName, jstring command, jstring uriString )
{
	LOG( "%p nativeNewIntent", (void *)appPtr );
	JavaUTFChars utfPackageName( jni, fromPackageName );
	JavaUTFChars utfUri( jni, uriString );
	JavaUTFChars utfJson( jni, command );

	char intentMessage[4096];
	ComposeIntentMessage( utfPackageName.ToStr(), utfUri.ToStr(), utfJson.ToStr(), 
			intentMessage, sizeof( intentMessage ) );
	LOG( "nativeNewIntent: %s", intentMessage );
	OVR::AppLocal * appLocal = (OVR::AppLocal *)appPtr;
	appLocal->GetMessageQueue().PostString( intentMessage );
}

}	// extern "C"

namespace OVR
{
//=======================================================================================
// VrAppInterface
//=======================================================================================

jlong VrAppInterface::SetActivity( JNIEnv * jni, jclass clazz, jobject activity, jstring javaFromPackageNameString,
		jstring javaCommandString, jstring javaUriString )
{
	// Make a permanent global reference for the class
	if ( ActivityClass != NULL )
	{
		jni->DeleteGlobalRef( ActivityClass );
	}
	ActivityClass = (jclass)jni->NewGlobalRef( clazz );

	JavaUTFChars utfFromPackageString( jni, javaFromPackageNameString );
	JavaUTFChars utfJsonString( jni, javaCommandString );
	JavaUTFChars utfUriString( jni, javaUriString );
	LOG( "VrAppInterface::SetActivity: %s %s %s", utfFromPackageString.ToStr(), utfJsonString.ToStr(), utfUriString.ToStr() );

	AppLocal * appLocal = static_cast< AppLocal * >( app );

	if ( appLocal == NULL )
	{	// First time initialization
		// This will set the VrAppInterface app pointer directly,
		// so it is set when OneTimeInit is called.
		LOG( "new AppLocal( %p %p %p )", jni, activity, this );
		appLocal = new AppLocal( *jni, activity, *this );
		app = appLocal;

		// Start the VrThread and wait for it to have initialized.
		appLocal->StartVrThread();
	}
	else
	{	// Just update the activity object.
		LOG( "Update AppLocal( %p %p %p )", jni, activity, this );
		appLocal->SetActivity( jni, activity );
	}

	// Send the launch intent.
	char intentMessage[4096];
	ComposeIntentMessage( utfFromPackageString.ToStr(), utfUriString.ToStr(), utfJsonString.ToStr(), intentMessage, sizeof( intentMessage ) );
	appLocal->GetMessageQueue().PostString( intentMessage );

	return (jlong)app;
}

//=======================================================================================
// AppLocal
//=======================================================================================

void AppLocal::SetActivity( JNIEnv * jni, jobject activity )
{
	if ( Java.ActivityObject != NULL )
	{
		jni->DeleteGlobalRef( Java.ActivityObject );
		Java.ActivityObject = NULL;
		VrSettings.ModeParms.Java.ActivityObject = NULL;
	}
	if ( activity != NULL )
	{
		Java.ActivityObject = jni->NewGlobalRef( activity );
		VrSettings.ModeParms.Java.ActivityObject = Java.ActivityObject;
	}
}

void AppLocal::HandleVrModeChanges()
{
	if ( Resumed != false && nativeWindow != NULL )
	{
		if ( OvrMobile == NULL )
		{
			Configure();
			EnterVrMode();
		}
	}
	else
	{
		if ( OvrMobile != NULL )
		{
			LeaveVrMode();
		}
	}
}

// This callback happens from the java thread, after a string has been
// pulled off the message queue
void AppLocal::TtjCommand( JNIEnv & jni, const char * commandString )
{
	if ( MatchesHead( "finish ", commandString ) )
	{
		jni.CallVoidMethod( Java.ActivityObject, finishActivityMethodId );
	}
}

}	// namespace OVR

#else  // OVR_OS_ANDROID

// add a single symbol to avoid LNK4221 when there are no symbols defined
namespace { char App_Android_EmptyFile; }

#endif
