/************************************************************************************

Filename    :   SurfaceTexture.cpp
Content     :   Interface to Android SurfaceTexture objects
Created     :   September 17, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "SurfaceTexture.h"

#include <stdlib.h>

#include "Kernel/OVR_LogUtils.h"
#include "VrApi.h"				// for swapchain interface

#include "OVR_GlUtils.h"
#include "GlTexture.h"

#if !defined( OVR_OS_ANDROID )
#define GL_TEXTURE_EXTERNAL_OES GL_TEXTURE_2D
#endif

namespace OVR {

SurfaceTexture::SurfaceTexture( JNIEnv * jni_ ) :
	textureSwapChain( NULL ),
	javaObject( NULL ),
	jni( NULL ),
	nanoTimeStamp( 0 ),
	updateTexImageMethodId( NULL ),
	getTimestampMethodId( NULL ),
	setDefaultBufferSizeMethodId( NULL )
{
	jni = jni_;

	// Gen a gl texture id for the java SurfaceTexture to use.
	textureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D_EXTERNAL, VRAPI_TEXTURE_FORMAT_8888, 0, 0, 1, false );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, GetTextureId() );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameterf( GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );

#if defined( OVR_OS_ANDROID )
	static const char * className = "android/graphics/SurfaceTexture";
	const jclass surfaceTextureClass = jni->FindClass( className );
	if ( surfaceTextureClass == 0 )
	{
		FAIL( "FindClass( %s ) failed", className );
	}

	// find the constructor that takes an int
	const jmethodID constructor = jni->GetMethodID( surfaceTextureClass, "<init>", "(I)V" );
	if ( constructor == 0 )
	{
		FAIL( "GetMethodID( <init> ) failed" );
	}

	jobject obj = jni->NewObject( surfaceTextureClass, constructor, GetTextureId() );
	if ( obj == 0 )
	{
		FAIL( "NewObject() failed" );
	}

	javaObject = jni->NewGlobalRef( obj );
	if ( javaObject == 0 )
	{
		FAIL( "NewGlobalRef() failed" );
	}

	// Now that we have a globalRef, we can free the localRef
	jni->DeleteLocalRef( obj );

	updateTexImageMethodId = jni->GetMethodID( surfaceTextureClass, "updateTexImage", "()V" );
	if ( !updateTexImageMethodId )
	{
		FAIL( "couldn't get updateTexImageMethodId" );
	}

	getTimestampMethodId = jni->GetMethodID( surfaceTextureClass, "getTimestamp", "()J" );
	if ( !getTimestampMethodId )
	{
		FAIL( "couldn't get getTimestampMethodId" );
	}

	setDefaultBufferSizeMethodId = jni->GetMethodID( surfaceTextureClass, "setDefaultBufferSize", "(II)V" );
	if ( !setDefaultBufferSizeMethodId )
	{
		FAIL( "couldn't get setDefaultBufferSize" );
	}

	// jclass objects are localRefs that need to be freed
	jni->DeleteLocalRef( surfaceTextureClass );
#else
    BufferHeight = 0;
    BufferWidth = 0;
#endif
}

SurfaceTexture::~SurfaceTexture()
{
#if defined( OVR_OS_ANDROID )
	if ( textureSwapChain )
	{
		vrapi_DestroyTextureSwapChain( textureSwapChain );
		textureSwapChain = NULL;
	}
	if ( javaObject )
	{
		jni->DeleteGlobalRef( javaObject );
		javaObject = 0;
	}
#else
	delete Buffer;
#endif
}

void SurfaceTexture::SetDefaultBufferSize( const int width, const int height )
{
#if defined( OVR_OS_ANDROID )
	jni->CallVoidMethod( javaObject, setDefaultBufferSizeMethodId, width, height );
#else
	OVR_UNUSED( width );
	OVR_UNUSED( height );
#endif
}

void SurfaceTexture::Update()
{
#if defined( OVR_OS_ANDROID )
    // latch the latest movie frame to the texture
	if ( !javaObject )
	{
		return;
	}

	jni->CallVoidMethod( javaObject, updateTexImageMethodId );
	nanoTimeStamp = jni->CallLongMethod( javaObject, getTimestampMethodId );
#else
    glBindTexture( GL_TEXTURE_2D, GetTextureId() );

    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, BufferWidth, BufferHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, Buffer );
#endif
}

unsigned int SurfaceTexture::GetTextureId()
{
	return ( textureSwapChain != NULL ) ? vrapi_GetTextureSwapChainHandle( textureSwapChain, 0 ) : 0;
}

ovrTextureSwapChain * SurfaceTexture::GetTextureSwapChain()
{
	return textureSwapChain;
}

jobject SurfaceTexture::GetJavaObject()
{
	return javaObject;
}

long long SurfaceTexture::GetNanoTimeStamp()
{
	return nanoTimeStamp;
}

#if !defined( OVR_OS_ANDROID )

void * SurfaceTexture::GetBuffer()
{
    nanoTimeStamp++;
    return Buffer;
}

int SurfaceTexture::GetBufferSize() const
{
    return BufferWidth * BufferHeight * 4;
}

int SurfaceTexture::GetBufferWidth() const
{
    return BufferWidth;
}

int SurfaceTexture::GetBufferHeight() const
{
    return BufferHeight;
}

void SurfaceTexture::SetBufferDimensions( int width, int height )
{
    BufferWidth = width;
    BufferHeight = height;
    delete Buffer;
    Buffer = new uint8_t[BufferWidth * BufferHeight * 4];
}

#endif

}	// namespace OVR
