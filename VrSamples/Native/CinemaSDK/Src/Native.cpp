/************************************************************************************

Filename    :   Native.cpp
Content     :
Created     :	6/20/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include <VrApi_Types.h>
#include "CinemaApp.h"
#include "Native.h"
#include "Android/JniUtils.h"

#if defined( OVR_OS_ANDROID )
extern "C" {

long Java_com_oculus_cinemasdk_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	LOG( "nativeSetAppInterface" );
	return (new OculusCinema::CinemaApp())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

void Java_com_oculus_cinemasdk_MainActivity_nativeSetVideoSize( JNIEnv *jni, jclass clazz, jlong interfacePtr, int width, int height, int rotation, int duration )
{
	LOG( "nativeSetVideoSizes: width=%i height=%i rotation=%i duration=%i", width, height, rotation, duration );

	OculusCinema::CinemaApp * cinema = static_cast< OculusCinema::CinemaApp * >( ( (App *)interfacePtr )->GetAppInterface() );
	cinema->GetMessageQueue().PostPrintf( "video %i %i %i %i", width, height, rotation, duration );
}

jobject Java_com_oculus_cinemasdk_MainActivity_nativePrepareNewVideo( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{
	OculusCinema::CinemaApp * cinema = static_cast< OculusCinema::CinemaApp * >( ( (App *)interfacePtr )->GetAppInterface() );

	// set up a message queue to get the return message
	// TODO: make a class that encapsulates this work
	ovrMessageQueue result( 1 );
	cinema->GetMessageQueue().PostPrintf( "newVideo %p", &result );

	result.SleepUntilMessage();
	const char * msg = result.GetNextMessage();
	jobject	texobj;
	sscanf( msg, "surfaceTexture %p", &texobj );
	free( (void *)msg );

	return texobj;
}
void Java_com_oculus_cinemasdk_MainActivity_nativeDisplayMessage( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring text, int time, bool isError ) {}
void Java_com_oculus_cinemasdk_MainActivity_nativeAddPc( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring name, jstring uuid, int psi, jstring binding,int width,int height)
{
    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    JavaUTFChars utfName( jni, name );
    JavaUTFChars utfUUID( jni, uuid );
    JavaUTFChars utfBind( jni, binding );

    OculusCinema::Native::PairState ps = (OculusCinema::Native::PairState) psi;
    cinema->PcMgr.AddPc(utfName.ToStr(), utfUUID.ToStr(), ps, utfBind.ToStr(), width, height);
}
void Java_com_oculus_cinemasdk_MainActivity_nativeRemovePc( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring name)
{
    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    JavaUTFChars utfName( jni, name );
    cinema->PcMgr.RemovePc(utfName.ToStr());
}
void Java_com_oculus_cinemasdk_MainActivity_nativeAddApp( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring name, jstring posterfilename, int id)
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    JavaUTFChars utfName( jni, name );
    JavaUTFChars utfPosterFileName( jni, posterfilename );
    cinema->AppMgr.AddApp(utfName.ToStr(), utfPosterFileName.ToStr(), id);

}
void Java_com_oculus_cinemasdk_MainActivity_nativeRemoveApp( JNIEnv *jni, jclass clazz, jlong interfacePtr, int id)
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    cinema->AppMgr.RemoveApp( id);

}


void Java_com_oculus_cinemasdk_MainActivity_nativeShowPair( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring message )
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    JavaUTFChars utfMessage( jni, message );
    cinema->ShowPair(utfMessage.ToStr());

}
void Java_com_oculus_cinemasdk_MainActivity_nativePairSuccess( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    cinema->PairSuccess();

}
void Java_com_oculus_cinemasdk_MainActivity_nativeShowError( JNIEnv *jni, jclass clazz, jlong interfacePtr, jstring message )
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    JavaUTFChars utfMessage( jni, message );
    cinema->ShowError(utfMessage.ToStr());

}
void Java_com_oculus_cinemasdk_MainActivity_nativeClearError( JNIEnv *jni, jclass clazz, jlong interfacePtr )
{

    OculusCinema::CinemaApp *cinema = ( OculusCinema::CinemaApp * )( ( (App *)interfacePtr )->GetAppInterface() );
    cinema->ClearError();

}

}	// extern "C"
#endif

//==============================================================

namespace OculusCinema
{

// Java method ids
static jmethodID 	getExternalCacheDirectoryMethodId = NULL;
static jmethodID	createVideoThumbnailMethodId = NULL;
static jmethodID 	isPlayingMethodId = NULL;
static jmethodID 	isPlaybackFinishedMethodId = NULL;
static jmethodID 	hadPlaybackErrorMethodId = NULL;
static jmethodID 	startMovieMethodId = NULL;
static jmethodID 	stopMovieMethodId = NULL;
static jmethodID     initPcSelectorMethodId = NULL;
static jmethodID     pairPcMethodId = NULL;
static jmethodID     getPcPairStateMethodId = NULL;
static jmethodID     getPcStateMethodId = NULL;
static jmethodID     getPcReachabilityMethodId = NULL;
static jmethodID    addPCbyIPMethodId = NULL;
static jmethodID     initAppSelectorMethodId = NULL;
static jmethodID    mouseMoveMethodId = NULL;
static jmethodID    mouseClickMethodId = NULL;
static jmethodID    mouseScrollMethodId = NULL;



#if defined( OVR_OS_ANDROID )
// Error checks and exits on failure
static jmethodID GetMethodID( App * app, jclass cls, const char * name, const char * signature )
{
	jmethodID mid = app->GetJava()->Env->GetMethodID( cls, name, signature );
	if ( !mid )
	{
    	FAIL( "Couldn't find %s methodID", name );
    }

	return mid;
}
#endif

void Native::OneTimeInit( App *app, jclass mainActivityClass )
{
	LOG( "Native::OneTimeInit" );

	const double start = SystemClock::GetTimeInSeconds();

#if defined( OVR_OS_ANDROID )
	getExternalCacheDirectoryMethodId 	= GetMethodID( app, mainActivityClass, "getExternalCacheDirectory", "()Ljava/lang/String;" );
	createVideoThumbnailMethodId        = GetMethodID( app, mainActivityClass, "createVideoThumbnail", "(Ljava/lang/String;ILjava/lang/String;II)Z" );
	isPlayingMethodId 					= GetMethodID( app, mainActivityClass, "isPlaying", "()Z" );
	isPlaybackFinishedMethodId			= GetMethodID( app, mainActivityClass, "isPlaybackFinished", "()Z" );
	hadPlaybackErrorMethodId			= GetMethodID( app, mainActivityClass, "hadPlaybackError", "()Z" );
	startMovieMethodId                     = GetMethodID( app, mainActivityClass, "startMovie", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;IIIZ)V" );
	stopMovieMethodId 					= GetMethodID( app, mainActivityClass, "stopMovie", "()V" );
	initPcSelectorMethodId                 = GetMethodID( app, mainActivityClass, "initPcSelector", "()V" );
    pairPcMethodId                         = GetMethodID( app, mainActivityClass, "pairPc", "(Ljava/lang/String;)V" );
    getPcPairStateMethodId                 = GetMethodID( app, mainActivityClass, "getPcPairState", "(Ljava/lang/String;)I" );
    getPcStateMethodId                     = GetMethodID( app, mainActivityClass, "getPcState", "(Ljava/lang/String;)I" );
    getPcReachabilityMethodId             = GetMethodID( app, mainActivityClass, "getPcReachability", "(Ljava/lang/String;)I" );
	addPCbyIPMethodId                    = GetMethodID( app, mainActivityClass, "addPCbyIP", "(Ljava/lang/String;)I" );
	initAppSelectorMethodId             = GetMethodID( app, mainActivityClass, "initAppSelector", "(Ljava/lang/String;)V" );
    mouseMoveMethodId                     = GetMethodID( app, mainActivityClass, "mouseMove", "(II)V" );
    mouseClickMethodId                     = GetMethodID( app, mainActivityClass, "mouseClick", "(IZ)V" );
    mouseScrollMethodId                 = GetMethodID( app, mainActivityClass, "mouseScroll", "(B)V" );



#endif
	LOG( "Native::OneTimeInit: %3.1f seconds", SystemClock::GetTimeInSeconds() - start );
}

void Native::OneTimeShutdown()
{
	LOG( "Native::OneTimeShutdown" );
}

String Native::GetExternalCacheDirectory( App * app )
{
#if defined( OVR_OS_ANDROID )
	jstring externalCacheDirectoryString = (jstring)app->GetJava()->Env->CallObjectMethod( app->GetJava()->ActivityObject, getExternalCacheDirectoryMethodId );

	const char *externalCacheDirectoryStringUTFChars = app->GetJava()->Env->GetStringUTFChars( externalCacheDirectoryString, NULL );
	String externalCacheDirectory = externalCacheDirectoryStringUTFChars;

	app->GetJava()->Env->ReleaseStringUTFChars( externalCacheDirectoryString, externalCacheDirectoryStringUTFChars );
	app->GetJava()->Env->DeleteLocalRef( externalCacheDirectoryString );

	return externalCacheDirectory;
#else
	return String();
#endif
}

bool Native::CreateVideoThumbnail( App *app, const char *uuid, int appId, const char *outputFilePath, const int width, const int height )
{
	LOG( "CreateVideoThumbnail( %s, %i, %s )", uuid, appId, outputFilePath );
#if defined( OVR_OS_ANDROID )
	jstring jstrUUID = app->GetJava()->Env->NewStringUTF( uuid );

	jstring jstrOutputFilePath = app->GetJava()->Env->NewStringUTF( outputFilePath );


    //todo rafa

    jboolean result = app->GetJava()->Env->CallBooleanMethod( app->GetJava()->ActivityObject, createVideoThumbnailMethodId, jstrUUID, appId, jstrOutputFilePath, width, height );
    LOG( "Done creating thumbnail!");
    app->GetJava()->Env->DeleteLocalRef( jstrUUID );

	app->GetJava()->Env->DeleteLocalRef( jstrOutputFilePath );

	//LOG( "CreateVideoThumbnail( %s, %s )", videoFilePath, outputFilePath );

	return result;
#else
	return false;
#endif
}

bool Native::IsPlaying( App * app )
{
	LOG( "IsPlaying()" );
#if defined( OVR_OS_ANDROID )
	return app->GetJava()->Env->CallBooleanMethod( app->GetJava()->ActivityObject, isPlayingMethodId );
#else
	return false;
#endif
}

bool Native::IsPlaybackFinished( App * app )
{
#if defined( OVR_OS_ANDROID )
	jboolean result = app->GetJava()->Env->CallBooleanMethod( app->GetJava()->ActivityObject, isPlaybackFinishedMethodId );
	return ( result != 0 );
#else
	return false;
#endif
}

bool Native::HadPlaybackError( App * app )
{
#if defined( OVR_OS_ANDROID )
	jboolean result = app->GetJava()->Env->CallBooleanMethod( app->GetJava()->ActivityObject, hadPlaybackErrorMethodId );
	return ( result != 0 );
#else
	return false;
#endif
}

void Native::StartMovie( App *app, const char * uuid, const char * appName, int id, const char * binder, int width, int height, int fps, bool hostAudio )

{
	LOG( "StartMovie( %s )", appName );

	jstring jstrUUID = app->GetJava()->Env->NewStringUTF( uuid );
	jstring jstrAppName = app->GetJava()->Env->NewStringUTF( appName );
	jstring jstrBinder = app->GetJava()->Env->NewStringUTF( binder );


	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, startMovieMethodId, jstrUUID, jstrAppName, id, jstrBinder, width, height, fps, hostAudio );

	app->GetJava()->Env->DeleteLocalRef( jstrUUID );
	app->GetJava()->Env->DeleteLocalRef( jstrAppName );
	app->GetJava()->Env->DeleteLocalRef( jstrBinder );
}

void Native::StopMovie( App *app )
{
	LOG( "StopMovie()" );
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, stopMovieMethodId );
}

void Native::InitPcSelector( App *app )
{
	LOG( "InitPcSelector()" );
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, initPcSelectorMethodId );
}

void Native::InitAppSelector( App *app, const char* uuid)
{
	LOG( "InitAppSelector()" );

	jstring jstrUUID = app->GetJava()->Env->NewStringUTF( uuid );
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, initAppSelectorMethodId, jstrUUID );
}

Native::PairState Native::GetPairState( App *app, const char* uuid)
{
	LOG( "GetPairState()" );

	jstring jstrUUID = app->GetJava()->Env->NewStringUTF( uuid );
	return (PairState)app->GetJava()->Env->CallIntMethod( app->GetJava()->ActivityObject, getPcPairStateMethodId, jstrUUID );
}

void Native::Pair( App *app, const char* uuid)
{
	LOG( "Pair()" );

	jstring jstrUUID = app->GetJava()->Env->NewStringUTF( uuid );
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, pairPcMethodId, jstrUUID );
}

void Native::MouseMove(App *app, int deltaX, int deltaY)
{
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, mouseMoveMethodId, deltaX, deltaY );
}

void Native::MouseClick(App *app, int buttonId, bool down)
{
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, mouseClickMethodId, buttonId, down );
}

void Native::MouseScroll(App *app, signed char amount)
{
	app->GetJava()->Env->CallVoidMethod( app->GetJava()->ActivityObject, mouseScrollMethodId, amount );
}

int Native::addPCbyIP(App *app, const char* ip)
{
	jstring jstrIP = app->GetJava()->Env->NewStringUTF( ip );
	int result = app->GetJava()->Env->CallIntMethod( app->GetJava()->ActivityObject, addPCbyIPMethodId, jstrIP );
	app->GetJava()->Env->DeleteLocalRef( jstrIP );
	return result;
}



} // namespace OculusCinema
