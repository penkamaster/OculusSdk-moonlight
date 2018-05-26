/************************************************************************************

Filename    :   WavReader.h
Content     :	Wave data parsing.
Created     :	March 6, 2016
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

*************************************************************************************/

#include "WavReader.h"

#include "Kernel/OVR_LogUtils.h"

namespace OVR
{

static const uint32_t WAVE_RIFF_TAG		= 'FFIR';
static const uint32_t WAVE_FILE_TAG		= 'EVAW';
static const uint32_t WAVE_FORMAT_TAG	= ' tmf';
static const uint32_t WAVE_DATA_TAG		= 'atad';

static const uint32_t WAVE_FORMAT_PCM	= 0x01;

struct RiffChunk
{
	uint32_t	ChunkId;
	uint32_t	ChunkSize;
};

struct RiffChunkHeader
{
	uint32_t	ChunkId;
	uint32_t	ChunkSize;
	uint32_t	RiffChunk;
};

struct PcmWaveFormat
{
	unsigned short	Format;
	unsigned short	NumChannels;
	unsigned int	SampleRate;
	unsigned int	ByteRate;
	unsigned short	BlockAlign;
	unsigned short	BitsPerSample;
};

static const RiffChunk * GetChunkFromStream( const uint8_t * data, const size_t dataSize, const uint32_t chunkId )
{
	if ( data == NULL )
	{
		return NULL;
	}

	const uint8_t * bufferStart = data;
	const uint8_t * bufferEnd = data + dataSize;

	while ( bufferEnd > ( bufferStart + sizeof( RiffChunk )  ) )
	{
		const RiffChunk * chunkHeader = reinterpret_cast<const RiffChunk *>( bufferStart );
		if ( chunkHeader->ChunkId == chunkId )
		{
			return chunkHeader;
		}

		ptrdiff_t chunkOffset = chunkHeader->ChunkSize + sizeof( RiffChunk );
		bufferStart += chunkOffset;
	}

	return NULL;
}

bool ParseWavDataInMemory( const MemBufferT<uint8_t> & inBuff,
						   ovrWaveFormat & outWaveFormat,
						   ovrWaveData & outWaveData )
{
	memset( &outWaveFormat, 0, sizeof( ovrWaveFormat ) );
	memset( &outWaveData, 0, sizeof( ovrWaveData ) );

	const uint8_t * waveBuffer = ((const uint8_t *)inBuff );
	const size_t waveBufferSize = inBuff.GetSize();

	if ( waveBuffer == NULL )
	{
		return false;
	}

	if ( waveBufferSize < ( sizeof( RiffChunk ) * 2 + sizeof( uint32_t ) + ( sizeof( PcmWaveFormat ) - 2 /*waveformat*/) ) )
	{
		WARN( "Invalid Wave buffer size %d", waveBufferSize );
		return false;
	}

	const uint8_t * waveBufferEnd = waveBuffer + waveBufferSize;

	// Find the 'RIFF' chunk
	const RiffChunk * riffWaveChunk = GetChunkFromStream( waveBuffer, waveBufferSize, WAVE_RIFF_TAG );
	if ( riffWaveChunk == NULL || riffWaveChunk->ChunkSize < 4 )
	{
		WARN( "Invalid RIFF chunk size %d", riffWaveChunk->ChunkSize );
		return false;
	}

	// Verify the format.
	const RiffChunkHeader * riffWaveHeader = reinterpret_cast<const RiffChunkHeader *>( riffWaveChunk );
	if ( riffWaveHeader->RiffChunk != WAVE_FILE_TAG )
	{
		WARN( "Unsupported file type %d", riffWaveHeader->RiffChunk );
		return false;
	}

	const uint8_t * ptr = reinterpret_cast<const uint8_t *>( riffWaveHeader ) + sizeof( RiffChunkHeader );
	if ( ( ptr + sizeof( RiffChunk ) ) > waveBufferEnd )
	{
		WARN( "End of file when looking for FMT chunk" );
		return false;
	}

	// Find the 'FMT' chunk
	const RiffChunk * riffFmtChunk = GetChunkFromStream( ptr, riffWaveHeader->ChunkSize, WAVE_FORMAT_TAG );
	if ( riffFmtChunk == NULL || riffFmtChunk->ChunkSize < sizeof( PcmWaveFormat ) )
	{
		WARN( "Invalid Riff Fmt Chunk" );
		return false;
	}

	ptr = reinterpret_cast<const uint8_t *>( riffFmtChunk ) + sizeof( RiffChunk );
	if ( ( ptr + riffFmtChunk->ChunkSize ) > waveBufferEnd )
	{
		return false;
	}

	const PcmWaveFormat * wf = reinterpret_cast< const PcmWaveFormat *>( ptr );

	// Verify the format data
	OVR_ASSERT( wf->Format == WAVE_FORMAT_PCM );
	OVR_ASSERT( wf->NumChannels >= 1 && wf->NumChannels <= 8 );

	outWaveFormat.Format = wf->Format;
	outWaveFormat.NumChannels = wf->NumChannels;
	outWaveFormat.SampleRate = wf->SampleRate;
	outWaveFormat.BytesPerSec = wf->ByteRate;
	outWaveFormat.BlockAlign = wf->BlockAlign;
	outWaveFormat.BitsPerSample = wf->BitsPerSample;
	outWaveFormat.ExtraSize = 0;

	// Find the wave data
	ptr = reinterpret_cast<const uint8_t *>( riffWaveHeader ) + sizeof( RiffChunkHeader );
	if ( ( ptr + sizeof( RiffChunk ) ) > waveBufferEnd )
	{
		WARN( "End of file when looking for wave data" );
		return false;
	}

	const RiffChunk * riffDataChunk = GetChunkFromStream( ptr, riffWaveChunk->ChunkSize, WAVE_DATA_TAG );
	if ( riffDataChunk == NULL || riffDataChunk->ChunkSize == 0 )
	{
		WARN( "Invalid Riff Data Chunk" );
		return false;
	}

	ptr = reinterpret_cast<const uint8_t *>( riffDataChunk ) + sizeof( RiffChunk );
	if ( ptr + riffDataChunk->ChunkSize > waveBufferEnd )
	{
		WARN( "End of file when looking for Riff data" );
		return false;
	}

	outWaveData.data = ptr;
	outWaveData.dataSize = riffDataChunk->ChunkSize;

	return true;
}

}
