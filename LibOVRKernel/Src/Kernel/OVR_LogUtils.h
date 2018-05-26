/************************************************************************************

Filename    :   Log.h
Content     :   Macros and helpers for Android logging.
Created     :   4/15/2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVRLib_Log_h )
#define OVRLib_Log_h

#include "OVR_Types.h"
#include "OVR_Std.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <time.h>
#include <stdint.h>

void FilePathToTag( const char * filePath, char * strippedTag, size_t const strippedTagSize );

#if defined( OVR_OS_WIN32 )		// allow this file to be included in PC projects

// stub common functions for non-Android platforms

// Log with an explicit tag
void LogWithTag( const int prio, const char * tag, const char * fmt, ... );

// Strips the directory and extension from fileTag to give a concise log tag
void LogWithFileTag( const int prio, const char * fileTag, const char * fmt, ... );

#define LOG( ... ) 	LogWithFileTag( 0, __FILE__, __VA_ARGS__ )
#define WARN( ... ) LogWithFileTag( 0, __FILE__, __VA_ARGS__ )
#define FAIL( ... ) {LogWithFileTag( 0, __FILE__, __VA_ARGS__ );exit(0);}
#define LOG_WITH_TAG( __tag__, ... ) LogWithTag( 0, __FILE__, __VA_ARGS__ )
#define ASSERT_WITH_TAG( __expr__, __tag__ )

#elif defined( OVR_OS_ANDROID )

#include <android/log.h>
#include <jni.h>

// Log with an explicit tag
void LogWithTag( const int prio, const char * tag, const char * fmt, ... )
	__attribute__((__format__(printf, 3, 4)))
	__attribute__((__nonnull__(3)));

// Strips the directory and extension from fileTag to give a concise log tag
void LogWithFileTag( const int prio, const char * fileTag, const char * fmt, ... )
	__attribute__((__format__(printf, 3, 4)))
	__attribute__((__nonnull__(3)));

// Our standard logging (and far too much of our debugging) involves writing
// to the system log for viewing with logcat.  Previously we defined separate
// LOG() macros in each file to give them file-specific tags for filtering;
// now we use this helper function to use a LOG_TAG define (via cflags or
// #define LOG_TAG in source file) when available. Fallback to using a massaged
// __FILE__ macro turning the file base in to a tag -- jni/App.cpp becomes the
// tag "App".
#ifdef LOG_TAG
#define LOG(...) ( (void)LogWithTag( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__) )
#define WARN(...) ( (void)LogWithTag( ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__) )
#define FAIL(...) { (void)LogWithTag( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); abort(); }
#else
#define LOG( ... ) LogWithFileTag( ANDROID_LOG_INFO, __FILE__, __VA_ARGS__ )
#define WARN( ... ) LogWithFileTag( ANDROID_LOG_WARN, __FILE__, __VA_ARGS__ )
#define FAIL( ... ) {LogWithFileTag( ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__ );abort();}
#endif

#define LOG_WITH_TAG( __tag__, ...) ( (void)LogWithTag( ANDROID_LOG_INFO, __tag__, __VA_ARGS__) )
#define WARN_WITH_TAG( __tag__, ...) ( (void)LogWithTag( ANDROID_LOG_WARN, __tag__, __VA_ARGS__) )
#define FAIL_WITH_TAG( __tag__, ... ) { (void)LogWithTag( ANDROID_LOG_ERROR, __tag__, __VA_ARGS__); abort(); }


// LOG (usually defined on a per-file basis to write to a specific tag) is for logging that can be checked in 
// enabled and generally only prints once or infrequently.
// SPAM is for logging you want to see every frame but should never be checked in
#if defined( OVR_BUILD_DEBUG )
// you should always comment this out before checkin
//#define ALLOW_LOG_SPAM
#endif

#if defined( ALLOW_LOG_SPAM )
#define SPAM(...) LogWithTag( ANDROID_LOG_VERBOSE, "Spam", __VA_ARGS__ )
#else
#define SPAM(...) { }
#endif

// TODO: we need a define for internal builds that will compile in assertion messages but not debug breaks
// and we need a define for external builds that will do nothing when an assert is hit.
#if !defined( OVR_BUILD_DEBUG )
#define ASSERT_WITH_TAG( __expr__, __tag__ ) { if ( !( __expr__ ) ) { WARN_WITH_TAG( __tag__, "ASSERTION FAILED: %s", #__expr__ ); } }
#else
#define ASSERT_WITH_TAG( __expr__, __tag__ ) { if ( !( __expr__ ) ) { WARN_WITH_TAG( __tag__, "ASSERTION FAILED: %s", #__expr__ ); OVR_DEBUG_BREAK; } }
#endif

#else
#error "unknown platform"
#endif	

#define LOGD LOG
#define LOGI LOG
#define LOGW WARN
#define LOGE WARN

#endif // OVRLib_Log_h
