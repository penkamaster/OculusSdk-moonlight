/************************************************************************************

Filename    :   OVR_TurboJpeg.h
Content     :	Utility functions for libjpeg-turbo
Created     :   February 25, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "Kernel/OVR_Types.h"
#include "OVR_TurboJpeg.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "turbojpeg.h"

#if defined( OVR_OS_ANDROID )
#include <unistd.h>
#include <sys/mman.h>
#endif

#if defined( OVR_OS_ANDROID )
#include <android/log.h>
const int ANDROID_LOG_WARN_TJ = 5;
#define LOG_TJ( ... ) __android_log_print( ANDROID_LOG_WARN_TJ, __FILE__, __VA_ARGS__ );printf("\n")
#else
#define LOG_TJ( ... ) printf( __VA_ARGS__ );printf("\n")
#endif

namespace OVR {

bool WriteJpeg( const char * destinationFile, const unsigned char * rgbxBuffer, int width, int height )
{
#if defined( OVR_OS_ANDROID )
	tjhandle tj = tjInitCompress();

	unsigned char * jpegBuf = NULL;
	unsigned long jpegSize = 0;

	const int r = tjCompress2( tj, ( unsigned char * )rgbxBuffer,	// TJ isn't const correct...
		width, width * 4, height, TJPF_RGBX, &jpegBuf,
		&jpegSize, TJSAMP_444 /* TJSAMP_422 */, 90 /* jpegQual */, 0 /* flags */ );
	if ( r != 0 )
	{
		LOG_TJ( "tjCompress2 returned %s for %s", tjGetErrorStr(), destinationFile );
		return false;
	}

	FILE * f = fopen( destinationFile, "wb" );
	if ( f != NULL )
	{
		fwrite( jpegBuf, jpegSize, 1, f );
		fclose( f );
	}
	else
	{
		LOG_TJ( "WriteJpeg failed to write to %s", destinationFile );
		return false;
	}

	tjFree( jpegBuf );

	tjDestroy( tj );

	return true;
#else
	OVR_ASSERT( 0 );

	OVR_UNUSED2( destinationFile, rgbxBuffer );
	OVR_UNUSED2( width, height );

	return false;
#endif
}

// Drop-in replacement for stbi_load_from_memory(), but without component specification.
// Often 2x - 3x faster.
unsigned char * TurboJpegLoadFromMemory( const unsigned char * jpg, const int length, int * width, int * height )
{
#if defined( OVR_OS_ANDROID )
	tjhandle tj = tjInitDecompress();
	int	jpegWidth;
	int	jpegHeight;
	int jpegSubsamp;
	int jpegColorspace;
	const int headerRet = tjDecompressHeader3( tj,
		( unsigned char * )jpg /* tj isn't const correct */, length, &jpegWidth, &jpegHeight,
		&jpegSubsamp, &jpegColorspace );
	if ( headerRet )
	{
		LOG_TJ( "TurboJpegLoadFromMemory: header: %s", tjGetErrorStr() );
		tjDestroy( tj );
		return NULL;
	}
	const int bufLen = jpegWidth * jpegHeight * 4;
	unsigned char * buffer = ( unsigned char * )malloc( bufLen );
	if ( buffer != NULL )
	{
		const int decompRet = tjDecompress2( tj,
			( unsigned char * )jpg, length, buffer,
			jpegWidth, jpegWidth * 4, jpegHeight, TJPF_RGBX, 0 /* flags */ );
		if ( decompRet )
		{
			LOG_TJ( "TurboJpegLoadFromMemory: decompress: %s", tjGetErrorStr() );
			tjDestroy( tj );
			free( buffer );
			return NULL;
		}

		tjDestroy( tj );

		*width = jpegWidth;
		*height = jpegHeight;

	}

	return buffer;
#else
	OVR_ASSERT( 0 );

	OVR_UNUSED( jpg );
	OVR_UNUSED3( length, width, height );

	return NULL;
#endif
}


unsigned char * TurboJpegLoadFromFile( const char * filename, int * width, int * height )
{
#if defined( OVR_OS_ANDROID )
	const int fd = open( filename, O_RDONLY );
	if ( fd <= 0 )
	{
		return NULL;
	}
	struct stat	st = {};
	if ( fstat( fd, &st ) == -1 )
	{
		LOG_TJ( "fstat failed on %s", filename );
		close( fd );
		return NULL;
	}

	const unsigned char * jpg = ( unsigned char * )mmap(
		NULL, ( size_t )st.st_size, PROT_READ, MAP_SHARED, fd, 0 );
	if ( jpg == MAP_FAILED )
	{
		LOG_TJ( "Failed to mmap %s, len %i: %s", filename, ( int )st.st_size,
			strerror( errno ) );
		close( fd );
		return NULL;
	}
	LOG_TJ( "mmap %s, %i bytes at %p", filename, ( int )st.st_size, jpg );

	unsigned char * ret = TurboJpegLoadFromMemory( jpg, ( int )st.st_size, width, height );
	close( fd );
	munmap( ( void * )jpg, ( size_t )st.st_size );
	return ret;
#else
	OVR_ASSERT( 0 );

	OVR_UNUSED( filename );
	OVR_UNUSED2( width, height );

	return NULL;
#endif
}
}
