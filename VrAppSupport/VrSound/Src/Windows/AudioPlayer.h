/************************************************************************************

Filename    :   AudioPlayer.h
Content     :   A Very Basic Sound Player which accounts for Rift Audio Focus.
Created     :   March 6, 2016
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef __OvrAudioPlayer_h__
#define __OvrAudioPlayer_h__

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_StringHash.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Kernel/OVR_Array.h"

struct IXAudio2;
struct IXAudio2MasteringVoice;

namespace OVR {

struct soundData_t;
struct voiceData_t;

class ovrAudioPlayer
{
public:
					ovrAudioPlayer();
					~ovrAudioPlayer();

	void			Initialize( class ovrFileSys * fileSys );

	bool			PlaySound( const char * soundName );
	bool			StopSound( const char * soundName );

private:
	soundData_t *	CacheSound( const char * soundName, MemBufferT< uint8_t > * soundBuf );
	bool			PlaySound_Internal( const char * soundName, const soundData_t * sound );

private:

	class ovrFileSys *				FileSys;

	IXAudio2 *						AudioEngine;
	IXAudio2MasteringVoice *		AudioMasteringVoice;

	typedef StringHash<soundData_t *> SoundCacheMap;
	SoundCacheMap					SoundCache;

	Array<voiceData_t *>			SoundVoicePool;
};

} // namespace OVR

#endif // __OvrAudioPlayer_h__
