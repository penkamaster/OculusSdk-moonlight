/************************************************************************************

Filename    :   AudioPlayer.cpp
Content     :   A Very Basic Sound Player which accounts for Rift Audio Focus.
Created     :   March 6, 2016
Authors     :   Gloria Kennickell

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "AudioPlayer.h"

#include "Kernel/OVR_LogUtils.h"
#include "OVR_FileSys.h"

#include "VrApi_Audio.h"

#include "WavReader.h"

#include <Windows.h>
#pragma comment( lib, "Winmm.lib" )	// For PlaySound

#if 0 //( _WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/ )
#include <xaudio2.h>
#pragma comment( lib,"xaudio2.lib" )
#else
// Use XAudio 2.7 API to support Win7. Win8+ will require the
// end-user 2010 dxsdk redistributable package to be installed.
#include "../../3rdParty/dxsdk/Include/XAudio2.h"
#endif

/*
TODO:
1) static sized array with max allocation allowed for sound voice pool
2) voice re-use : format tag, numchannels, bitspersample -> generate key
3) check for audioengine / mastering voice failures
4) validate memory allocation
5) validate all resources are freed on shutdown / error cases
6) if audio focus changes, recreate mastering voice
7) pause/resume
*/

namespace OVR {

class SoundCallbackHander : public IXAudio2VoiceCallback
{
public:
	SoundCallbackHander( bool * isPlayingHolder ) :
		m_isPlayingHolder( isPlayingHolder )
	{
	}

	~SoundCallbackHander()
	{
		m_isPlayingHolder = NULL;
	}

	// Voice callbacks from IXAudio2VoiceCallback
	STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 bytesRequired);
	STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS);
	STDMETHOD_(void, OnStreamEnd) (THIS);
	STDMETHOD_(void, OnBufferStart) (THIS_ void* bufferContext);
	STDMETHOD_(void, OnBufferEnd) (THIS_ void* bufferContext);
	STDMETHOD_(void, OnLoopEnd) (THIS_ void* bufferContext);
	STDMETHOD_(void, OnVoiceError) (THIS_ void* bufferContext, HRESULT error);

private:
	bool * m_isPlayingHolder;
};

//
// Callback handlers, only implement the buffer events for maintaining play state
//
void SoundCallbackHander::OnVoiceProcessingPassStart( UINT32 /*bytesRequired*/ )
{
}
void SoundCallbackHander::OnVoiceProcessingPassEnd()
{
}
void SoundCallbackHander::OnStreamEnd()
{
}
void SoundCallbackHander::OnBufferStart( void * /*bufferContext*/ )
{
	*m_isPlayingHolder = true;
}
void SoundCallbackHander::OnBufferEnd( void * /*bufferContext*/ )
{
	*m_isPlayingHolder = false;
}
void SoundCallbackHander::OnLoopEnd( void * /*bufferContext*/ )
{
}
void SoundCallbackHander::OnVoiceError( void * /*bufferContext*/, HRESULT /*error*/ )
{
}

struct voiceData_t
{
	voiceData_t() :
		audioVoice( NULL ),
		isPlaying( false ),
		callbackHandler( &isPlaying )
	{
		memset( &format, 0, sizeof( WAVEFORMATEX ) );
	}
	~voiceData_t()
	{
		if ( audioVoice != NULL )
		{
			audioVoice->DestroyVoice();
			audioVoice = NULL;
		}
	}
	IXAudio2SourceVoice *	audioVoice;
	SoundCallbackHander		callbackHandler;
	WAVEFORMATEX 			format;
	bool					isPlaying;
};

struct soundData_t
{
	soundData_t() :
		soundBuf( NULL ),
		voiceIndex( -1 )
	{
		memset( &waveFormat, 0, sizeof( OVR::ovrWaveFormat ) );
		memset( &waveData, 0, sizeof( OVR::ovrWaveData ) );
	}
	~soundData_t()
	{
		delete soundBuf;
		soundBuf = NULL;
	}

	MemBufferT< uint8_t > * soundBuf;		// only used for parallel winmm playsound implementation

	OVR::ovrWaveFormat		waveFormat;
	OVR::ovrWaveData		waveData;

	int						voiceIndex;
};

static const bool useXAudio = true;

ovrAudioPlayer::ovrAudioPlayer() :
	FileSys( NULL ),
	AudioEngine( NULL ),
	AudioMasteringVoice( NULL )
{
	CoInitializeEx( NULL, COINIT_MULTITHREADED );

	// Create the XAudio2 Engine.
	UINT32 flags = 0;
	HRESULT hr = XAudio2Create( &AudioEngine, flags );
	if ( SUCCEEDED( hr ) )
	{
#if 0 // ( _WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/ ) && defined( _DEBUG )
		XAUDIO2_DEBUG_CONFIGURATION debug ={ 0 };
		debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
		debug.BreakMask = XAUDIO2_LOG_ERRORS;
		AudioEngine->SetDebugConfiguration( &debug, 0 );
#endif

		// Set Audio Focus to the preferred VR audio output device.
		wchar_t deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
		vrapi_GetAudioDeviceOutGuidStr( deviceOutStrBuffer );

#if 0 // ( _WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/ )
		LPWSTR  deviceId = GetAudioOutDeviceId( deviceOutStrBuffer );
#else
		UINT32 deviceCount = 0;
		AudioEngine->GetDeviceCount( &deviceCount );

		UINT32 deviceId = 0;
		for ( UINT32 i = 0; i < deviceCount; i++ )
		{
			XAUDIO2_DEVICE_DETAILS deviceDetails;
			hr = AudioEngine->GetDeviceDetails( i, &deviceDetails );
			if ( SUCCEEDED( hr ) )
			{
				if ( wcscmp( deviceDetails.DeviceID, deviceOutStrBuffer ) == 0 )
				{
					deviceId = i;
					break;
				}
			}
		}
#endif

		// Create the mastering voice.
		hr = AudioEngine->CreateMasteringVoice( &AudioMasteringVoice, 
											XAUDIO2_DEFAULT_CHANNELS, 
											XAUDIO2_DEFAULT_SAMPLERATE,
											0,
											deviceId );
		if ( FAILED( hr ) )
		{
			WARN( "Failed to create XAudio2 Mastering Voice %d", hr );

			if ( AudioEngine != NULL )
			{
				AudioEngine->Release();
				AudioEngine = NULL;
			}
		}
	}
	else
	{
		WARN( "Failed to create XAudio2 Engine %d", hr );
		if ( hr == REGDB_E_CLASSNOTREG  )
		{
			WARN( "Verify the dxsdk 2010 redistributable is installed" );
		}
	}
}

ovrAudioPlayer::~ovrAudioPlayer()
{
	if ( !useXAudio )
	{
		// Stop any winmm sounds from playing before freeing memory.
		::PlaySound( NULL, NULL, 0 );
	}

	for ( int i = 0; i < SoundVoicePool.GetSizeI(); i++ )
	{
		if ( SoundVoicePool[i] != NULL )
		{
			delete SoundVoicePool[i];
			SoundVoicePool[i] = NULL;
		}
	}
	SoundVoicePool.Clear();

	for ( SoundCacheMap::Iterator i = SoundCache.Begin(); i != SoundCache.End(); ++i )
	{
		delete i->Second;
	}
	SoundCache.Clear();

	if ( AudioMasteringVoice != NULL )
	{
		AudioMasteringVoice->DestroyVoice();
		AudioMasteringVoice = NULL;
	}

	if ( AudioEngine != NULL )
	{
		AudioEngine->Release();
		AudioEngine = NULL;
	}

	CoUninitialize();
}

void ovrAudioPlayer::Initialize( ovrFileSys * fileSys )
{
	FileSys = fileSys;
}

soundData_t * ovrAudioPlayer::CacheSound( const char * soundName, MemBufferT< uint8_t > * soundBuf )
{
	soundData_t * soundData = new soundData_t();
	OVR_ASSERT( soundData != NULL );
	soundData->soundBuf = soundBuf;

	bool success = OVR::ParseWavDataInMemory( *(const MemBufferT<uint8_t> *)soundBuf, 
											 soundData->waveFormat,
											 soundData->waveData );
	if ( !success )
	{
		WARN( "Faild to load wav data for %s", soundName );
		delete soundData;
		return NULL;
	}

	voiceData_t * voiceData = new voiceData_t();
	voiceData->format.wFormatTag = soundData->waveFormat.Format;
	voiceData->format.nChannels = soundData->waveFormat.NumChannels;
	voiceData->format.nSamplesPerSec = soundData->waveFormat.SampleRate;
	voiceData->format.nAvgBytesPerSec = soundData->waveFormat.BytesPerSec;
	voiceData->format.nBlockAlign = soundData->waveFormat.BlockAlign;
	voiceData->format.wBitsPerSample = soundData->waveFormat.BitsPerSample;
	voiceData->format.cbSize = soundData->waveFormat.ExtraSize;

	// Create the source voice.
	if ( AudioEngine != NULL )
	{
		HRESULT hr = AudioEngine->CreateSourceVoice( &voiceData->audioVoice,
												 &voiceData->format,
												 0,
												 XAUDIO2_DEFAULT_FREQ_RATIO,
												 reinterpret_cast<IXAudio2VoiceCallback *>( &voiceData->callbackHandler ), 
												 NULL,
												 NULL );

		if ( FAILED( hr ) )
		{
			WARN( "Failed to create sound voice for %s", soundName );

			delete soundData;
			delete voiceData;

			return NULL;
		}
	}

	SoundVoicePool.PushBack( voiceData );
	soundData->voiceIndex = static_cast<int>( SoundVoicePool.GetSize() ) - 1;

	SoundCache.SetCaseInsensitive( soundName, soundData );

	return soundData;
}

bool ovrAudioPlayer::PlaySound_Internal( const char * soundName, const soundData_t * soundData )
{
	if ( useXAudio )
	{
		if ( soundData != NULL )
		{
			XAUDIO2_BUFFER playBuffer = { 0 };
			playBuffer.AudioBytes = soundData->waveData.dataSize;
			playBuffer.pAudioData = soundData->waveData.data;
			playBuffer.Flags = XAUDIO2_END_OF_STREAM;

			voiceData_t * voiceData = SoundVoicePool.ValueAt( soundData->voiceIndex );
			if ( voiceData == NULL || voiceData->audioVoice == NULL )
			{
				return false;
			}

			// Stop if already playing
			HRESULT hr = voiceData->audioVoice->Stop();
			if ( SUCCEEDED( hr ) )
			{
				hr = voiceData->audioVoice->FlushSourceBuffers();
			}

			// Submit the sound buffer and start
			hr = voiceData->audioVoice->SubmitSourceBuffer( &playBuffer );
			if ( SUCCEEDED( hr ) )
			{
				hr = voiceData->audioVoice->Start( 0, XAUDIO2_COMMIT_NOW );
			}

			return SUCCEEDED( hr );
		}
	}
	else
	{
		// Async PlaySound() takes about 60us in quick test.
		if ( soundData != NULL && soundData->soundBuf != NULL )
		{
			::PlaySound( (LPCSTR)(const uint8_t *)*soundData->soundBuf, NULL, SND_MEMORY | SND_ASYNC );
		}
		else
		{
			::PlaySound( soundName, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT );
		}
	}

	return true;
}

bool ovrAudioPlayer::PlaySound( const char * soundName )
{
	soundData_t * soundData = NULL;
	if ( FileSys != NULL )
	{
		// Takes 20-30us on Cache hit and ~500us on cache miss.
		MemBufferT< uint8_t > * soundBuf = NULL;
		SoundCacheMap::Iterator found = SoundCache.FindCaseInsensitive( soundName );
		if ( found != SoundCache.End() )
		{
			soundData = found->Second;
		}
		else
		{
			soundBuf = new MemBufferT< uint8_t >;
			if ( FileSys->ReadFile( soundName, *soundBuf ) )
			{
				soundData = CacheSound( soundName, soundBuf );
			}
			else
			{
				delete soundBuf;
				soundBuf = NULL;
			}
		}
	}

	return PlaySound_Internal( soundName, soundData );
}

bool ovrAudioPlayer::StopSound( const char * soundName )
{
	if ( useXAudio )
	{
		soundData_t * soundData = NULL;
		SoundCacheMap::Iterator found = SoundCache.FindCaseInsensitive( soundName );
		if ( found != SoundCache.End() )
		{
			soundData = found->Second;
		}
		else
		{
			return false;
		}

		if ( soundData == NULL )
		{
			return false;
		}

		voiceData_t * voiceData = SoundVoicePool.ValueAt( soundData->voiceIndex );
		if ( voiceData == NULL || voiceData->audioVoice == NULL )
		{
			return false;
		}

		HRESULT hr = voiceData->audioVoice->Stop();
		if ( SUCCEEDED( hr ) )
		{
			hr = voiceData->audioVoice->FlushSourceBuffers();
		}

		return SUCCEEDED( hr );
	}
	else
	{
		::PlaySound( NULL, NULL, 0 );
		return true;
	}
}

} // namespace OVR
