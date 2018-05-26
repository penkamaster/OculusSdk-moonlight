/************************************************************************************

Filename    :   Log.cpp
Content     :   Macros and helpers for Android logging.
Created     :   4/15/2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_LogUtils.h"

#if defined( WIN32 ) || defined( WIN64 ) || defined( _WIN32 ) || defined( _WIN64 )
#define NOMINMAX	// stop Windows.h from redefining min and max and breaking std::min / std::max
#include <windows.h>		// OutputDebugString
#endif

#include <stdarg.h>
#include <string.h>
#include <assert.h>

// Log with an explicit tag
void LogWithTag( const int prio, const char * tag, const char * fmt, ... )
{
#if defined( OVR_OS_ANDROID )
	va_list ap;
	va_start( ap, fmt );
	__android_log_vprint( prio, tag, fmt, ap );
	va_end( ap );
#elif defined( OVR_OS_WIN32 )
	OVR_UNUSED( tag );
	OVR_UNUSED( prio );

	va_list args;
	va_start( args, fmt );

	char buffer[4096];
	vsnprintf_s( buffer, 4096, _TRUNCATE, fmt, args );
	va_end( args );

	OutputDebugString( buffer );
#else
#error "Not implemented"
#endif
}

void FilePathToTag( const char * filePath, char * strippedTag, size_t const strippedTagSize )
{
	// scan backwards from the end to the first slash
	const int len = static_cast< int >( strlen( filePath ) );
	int	slash;
	for ( slash = len - 1; slash > 0 && filePath[slash] != '/' && filePath[slash] != '\\'; slash-- )
	{
	}
	if ( filePath[slash] == '/' || filePath[slash] == '\\' )
	{
		slash++;
	}
	// copy forward until a dot or 0
	size_t i;
	for ( i = 0; i < strippedTagSize - 1; i++ )
	{
		const char c = filePath[slash+i];
		if ( c == '.' || c == 0 )
		{
			break;
		}
		strippedTag[i] = c;
	}
	strippedTag[i] = 0;
}

void LogWithFileTag( const int prio, const char * fileTag, const char * fmt, ... )
{
#if defined( OVR_OS_ANDROID )
	va_list ap, ap2;

	// fileTag will be something like "jni/App.cpp", which we
	// want to strip down to just "App"
	char strippedTag[128];

	FilePathToTag( fileTag, strippedTag, sizeof( strippedTag ) );

	va_start( ap, fmt );

	// Calculate the length of the log message... if its too long __android_log_vprint() will clip it!
	va_copy( ap2, ap );
	const int loglen = vsnprintf( NULL, 0, fmt, ap2 );
	va_end( ap2 );

	if ( prio == ANDROID_LOG_ERROR )
	{
		// For FAIL messages which are longer than 512, truncate at 512.
		// We do not know the max size of abort message that will be taken by SIGABRT. 512 has been verified to work
		char *formattedMsg = ( char * )malloc( 512 );
		vsnprintf( formattedMsg, 512U, fmt, ap2 );
		__android_log_assert( "FAIL", strippedTag, "%s", formattedMsg );
		free( formattedMsg );
	}
	if ( loglen < 512 )
	{
		// For short messages just use android's default formatting path (which has a fixed size buffer on the stack).
		__android_log_vprint( prio, strippedTag, fmt, ap );
	}
	else
	{
		// For long messages allocate off the heap to avoid blowing the stack...
		char *formattedMsg = ( char * )malloc( loglen + 1 );
		vsnprintf( formattedMsg, ( size_t ) ( loglen + 1 ), fmt, ap2 );
		__android_log_write( prio, strippedTag, formattedMsg );
		free( formattedMsg );
	}

	va_end( ap );
#elif defined( OVR_OS_WIN32 )
	OVR_UNUSED( fileTag );
	OVR_UNUSED( prio );

	va_list args;
	va_start( args, fmt );

	char buffer[4096];
	vsnprintf_s( buffer, 4096, _TRUNCATE, fmt, args );
	va_end( args );

	OutputDebugString( buffer );
	OutputDebugString( "\n" );
#else
#error "Not implemented"
#endif
}
