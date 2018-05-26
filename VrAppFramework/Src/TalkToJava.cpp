/************************************************************************************

Filename    :   TalkToJava.cpp
Content     :   Thread and JNI management for making java calls in the background
Created     :   February 26, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "TalkToJava.h"

#include <stdlib.h>
#include <string.h>

#include "Android/JniUtils.h"
#include "Kernel/OVR_LogUtils.h"

#if defined( OVR_OS_ANDROID )
namespace OVR
{

void TalkToJava::Init( JavaVM & javaVM_, TalkToJavaInterface & javaIF_ )
{
	Jvm = &javaVM_;
	Interface = &javaIF_;

	// spawn the VR thread
	if ( TtjThread.Start() == false )
	{
		FAIL( "TtjThread start failed." );
	}
}

TalkToJava::~TalkToJava()
{
	if ( TtjThread.GetThreadState() != Thread::NotRunning )
	{
		// Get the background thread to kill itself.
		LOG( "TtjMessageQueue.PostPrintf( quit )" );
		TtjMessageQueue.PostPrintf( "quit" );
		if ( TtjThread.Join() == false )
		{
			WARN( "failed to join TtjThread" );
		}
	}
}

// Shim to call a C++ object from an OVR::Thread::Start.
threadReturn_t TalkToJava::ThreadStarter( Thread * thread, void * parm )
{
	thread->SetThreadName( "OVR::TalkToJava" );

	((TalkToJava *)parm)->TtjThreadFunction();

	return NULL;
}

/*
 * TtjThreadFunction
 *
 * Continuously waits for command messages and processes them.
 */
void TalkToJava::TtjThreadFunction()
{
	// The Java VM needs to be attached on each thread that will use it.
	LOG( "TalkToJava: Jvm->AttachCurrentThread" );
	ovr_AttachCurrentThread( Jvm, &Jni, NULL );

	// Process all queued messages
	for ( ; ; )
	{
		const char * msg = TtjMessageQueue.GetNextMessage();
		if ( !msg )
		{
			// Go dormant until something else arrives.
			TtjMessageQueue.SleepUntilMessage();
			continue;
		}

		if ( strcmp( msg, "quit" ) == 0 )
		{
			free( (void *)msg );
			break;
		}

		// Set up a local frame with room for at least 100
		// local references that will be auto-freed.
		Jni->PushLocalFrame( 100 );

		// Let whoever initialized us do what they want.
		Interface->TtjCommand( *Jni, msg );

		// If we don't clean up exceptions now, later
		// calls may fail.
		if ( Jni->ExceptionOccurred() )
		{
			Jni->ExceptionClear();
			LOG( "JNI exception after: %s", msg );
		}

		// Free any local references
		Jni->PopLocalFrame( NULL );

		free( (void *)msg );
	}

	LOG( "TalkToJava: Jvm->DetachCurrentThread" );
	ovr_DetachCurrentThread( Jvm );
}

}
#endif	// OVR_OS_ANDROID
