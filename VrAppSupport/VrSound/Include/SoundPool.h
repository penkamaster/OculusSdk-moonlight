/************************************************************************************

Filename    :   SoundPool.h
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_SoundPool_h )
#define OVR_SoundPool_h

#include "Kernel/OVR_Types.h"

namespace OVR {

// Pooled sound player for playing sounds from the APK. Must be
// created/destroyed from the same thread.
class ovrSoundPool
{
public:
			ovrSoundPool( JNIEnv & jni_, jobject activity_ );
			~ovrSoundPool();

	void	Initialize( class ovrFileSys * fileSys );

	// Not thread safe on Android.
	void	Play( JNIEnv & env, const char * soundName );
	void	Stop( JNIEnv & env, const char * soundName );
	void   	LoadSoundAsset( JNIEnv & env, const char * soundName );

private:
	// private assignment operator to prevent copying
	ovrSoundPool &	operator = ( ovrSoundPool & );

private:
	JNIEnv &		jni;
	jobject			pooler;
	jmethodID		playMethod;
	jmethodID		stopMethod;
	jmethodID		preloadMethod;
	jmethodID		releaseMethod;

#if defined( OVR_OS_WIN32 )
	class ovrAudioPlayer * AudioPlayer;
#endif
};

}

#endif // OVR_SoundPool_h
