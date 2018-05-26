/************************************************************************************

Filename    :   MemBuffer.cpp
Content     :	Memory buffer.
Created     :	May 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_MemBuffer.h"
#include "OVR_Types.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>

#if defined( OVR_OS_ANDROID )
#include <unistd.h>
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wconversion"
#include <sys/stat.h>
#pragma GCC diagnostic pop 
#else
#include <sys/stat.h>
#endif

#include "OVR_LogUtils.h"

namespace OVR
{

bool MemBuffer::WriteToFile( const char * filename )
{
	LOG( "Writing %i bytes to %s\n", Length, filename );
	FILE * f = fopen( filename, "wb" );
	if ( f != NULL )
	{
		fwrite( Buffer, Length, 1, f );
		fclose( f );
		return true;
	}
	else
	{
		LOG( "MemBuffer::WriteToFile failed to write to %s\n", filename );
	}
	return false;
}

MemBuffer::MemBuffer( int length ) : Buffer( malloc( length ) ), Length( length )
{
}

MemBuffer::~MemBuffer()
{
}

void MemBuffer::FreeData()
{
	if ( Buffer != NULL )
	{
		free( (void *)Buffer );
		Buffer = NULL;
	}
	Length = 0;
}

MemBufferFile::MemBufferFile( const char * filename )
{
	LoadFile( filename );
}

bool MemBufferFile::LoadFile( const char * filename )
{
	FreeData();
#if !defined( OVR_OS_ANDROID )
	FILE * f = fopen( filename, "rb" );
	if ( !f )
	{
		LOG( "Couldn't open %s\n", filename );
		return false;
	}
	fseek( f, 0, SEEK_END );
	Length = ftell( f );
	fseek( f, 0, SEEK_SET );
	Buffer = malloc( Length );
	const size_t readRet = fread( (unsigned char *)Buffer, Length, 1, f );
	fclose( f );
	if ( readRet != 1 )
	{
		LOG( "Only read %zu of %i bytes in %s\n", readRet, Length, filename );
		FreeData();
		return false;
	}
	return true;
#else
	// Using direct IO gives read speeds of 200 - 290 MB/s,
	// versus 130 - 170 MB/s with buffered stdio on a background thread.
	const int fd = open( filename, O_RDONLY, 0 );
	if ( fd < 0 )
	{
		LOG( "Couldn't open %s\n", filename );
		return false;
	}
	struct stat buf;
	if ( -1 == fstat( fd, &buf ) )
	{
		close( fd );
		LOG( "Couldn't fstat %s\n", filename );
		return false;
	}
	Length = (int)buf.st_size;
	Buffer = malloc( Length );
	const size_t readRet = read( fd, (unsigned char *)Buffer, Length );
	close( fd );
	if ( readRet != (size_t)Length )
	{
		LOG( "Only read %zu of %i bytes in %s\n", readRet, Length, filename );
		FreeData();
		return false;
	}
	return true;
#endif
}

MemBufferFile::MemBufferFile( eNoInit const noInit )
{
	OVR_UNUSED( noInit );
}

MemBuffer MemBufferFile::ToMemBuffer()
{
	MemBuffer	mb( Buffer, Length );
	Buffer = NULL;
	Length = 0;
	return mb;
}

MemBufferFile::~MemBufferFile()
{
	FreeData();
}

}	// namespace OVR
