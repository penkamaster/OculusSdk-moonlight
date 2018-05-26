/************************************************************************************

Filename    :   App.cpp
Content     :   Native counterpart to VrActivity and VrApp
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "App.h"
#include "AppLocal.h"

#if defined( OVR_OS_WIN32 )

#include "Android/JniUtils.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_System.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_JSON.h"

#include "VrApi.h"

namespace OVR
{
long long VrAppInterface::SetActivity(	JNIEnv * jni, jclass clazz, jobject activity, 
										jstring javaFromPackageNameString,
										jstring javaCommandString,
										jstring javaUriString )
{
	OVR_UNUSED( jni );
	OVR_UNUSED( clazz );
	OVR_UNUSED( activity );
	OVR_UNUSED( javaFromPackageNameString );
	OVR_UNUSED( javaCommandString );
	OVR_UNUSED( javaUriString );

	int exitCode = 0;

	ovrJava java;
	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult == VRAPI_INITIALIZE_SUCCESS )
	{
		AppLocal * appLocal = static_cast< AppLocal * >( app );
		if ( appLocal == NULL )
		{
			appLocal = new AppLocal( *jni, activity, *this );
			app = appLocal;

			// TODO: Review re-working framework handling so the pc port can be single-threaded.
			// Start the VrThread and wait for it to initialize.
			appLocal->StartVrThread();

			// TODO: Better way to map lifecycle
			// We could simulate android lifecycle events by :
			// (1) Check Session Status for VR Focus
			// (2) Check Mount/Unmount Status and pause after 12s similar to Gear.
			appLocal->GetMessageQueue().SendPrintf( "resume " );
			appLocal->GetMessageQueue().SendPrintf( "surfaceCreated " );

			exitCode = *static_cast< int32_t* >( appLocal->JoinVrThread() );

			delete appLocal;
			appLocal = NULL;
		}
	}
	else
	{
		exitCode = initResult;
		LOG( "vrapi_Initialize Error: %d", initResult );
	}

	vrapi_Shutdown();

	return exitCode;
}

void AppLocal::SetActivity( JNIEnv * jni, jobject activity )
{
	OVR_UNUSED( jni );
	OVR_UNUSED( activity );
}

void AppLocal::HandleVrModeChanges()
{
	if ( Resumed != false )
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

void AppLocal::TtjCommand( JNIEnv & jni, const char * commandString )
{
	OVR_UNUSED( commandString );
}

} // OVR

#endif	// OVR_OS_WIN32
