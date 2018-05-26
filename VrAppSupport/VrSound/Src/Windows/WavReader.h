/************************************************************************************

Filename    :   WaveReader.h
Content     :	Wave data parsing.
Created     :	March 6, 2016
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

*************************************************************************************/

#ifndef OVR_WAV_READER_H
#define OVR_WAV_READER_H

#include "Kernel/OVR_MemBuffer.h"

namespace OVR
{
	struct ovrWaveFormat
	{
		unsigned short	Format;			// Format Type
		unsigned short	NumChannels;	// Number of Channels
		unsigned int	SampleRate;		// Sample Rate
		unsigned int	BytesPerSec;	// Bytes per second (buffer estimation)
		unsigned short	BlockAlign;		// Block size of data
		unsigned short	BitsPerSample;	// Bits per sample of mono data
		unsigned short	ExtraSize;		// Size of extra information (in bytes)
	};

	struct ovrWaveData
	{
		const uint8_t * data;			// Wave data
		uint32_t		dataSize;		// Wave data size
	};

	bool ParseWavDataInMemory( const MemBufferT<uint8_t> & inWaveBuff,
							   ovrWaveFormat & outWaveFormat,
							   ovrWaveData & outWaveData );
}

#endif	// OVR_WAV_READER_H
