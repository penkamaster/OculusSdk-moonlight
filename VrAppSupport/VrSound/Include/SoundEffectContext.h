/************************************************************************************

Filename    :   SoundContext.h
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_SoundContext_h )
#define OVR_SoundContext_h

#include "SoundAssetMapping.h"
#include "SoundPool.h"
#include "TalkToJava.h"

namespace OVR {

// Context for playing sound effects from the APK. Must be
// created/destroyed from the same thread.
class ovrSoundEffectContext : private TalkToJavaInterface
{
public:
			ovrSoundEffectContext( JNIEnv & jni_, jobject activity_ );
	virtual	~ovrSoundEffectContext();

	void 	Initialize( class ovrFileSys * fileSys = nullptr );

	const ovrSoundAssetMapping & GetMapping()
	{
		return SoundAssetMapping;
	}

	void	Play( const char * name );
	void	Stop( const char * name );
	void	LoadSoundAsset( const char * name );

private:
	ovrSoundEffectContext &	operator = ( ovrSoundEffectContext & );

	void 			PlayInternal( JNIEnv & env, const char * name );
	void 			StopInternal( JNIEnv & env, const char * name );
	void 			LoadSoundAssetInternal( JNIEnv & env, const char * name );
	virtual void 	TtjCommand( JNIEnv & jni, const char * commandString ) OVR_OVERRIDE;

private:
#if defined( OVR_OS_ANDROID )
	TalkToJava				Ttj;
#endif
	ovrSoundPool			SoundPool;
	ovrSoundAssetMapping	SoundAssetMapping;
};

}

#endif	// OVR_SoundContext_h
