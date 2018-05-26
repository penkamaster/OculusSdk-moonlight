/************************************************************************************

Filename    :   OVR_PerfTimer.cpp
Content     :   A simple RAII class for timing blocks of code.
Created     :   1/22/2016
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_LogTimer.h"
#include "OVR_GlUtils.h"

// GPU Timer queries cause instability on current
// Adreno drivers. Disable by default, but allow
// enabling via the local prefs file.
// TODO: Test this on new Qualcomm driver under Lollipop.
static int AllowGpuTimerQueries = 0;	// 0 = off, 1 = glBeginQuery/glEndQuery, 2 = glQueryCounter

void SetAllowGpuTimerQueries( int enable )
{
	LOG( "SetAllowGpuTimerQueries( %d )", enable );
	AllowGpuTimerQueries = enable;
}

template< int NumTimers, int NumFrames >
LogGpuTime<NumTimers,NumFrames>::LogGpuTime() :
	UseTimerQuery( false ),
	UseQueryCounter( false ),
	TimerQuery(),
	BeginTimestamp(),
	DisjointOccurred(),
	TimeResultIndex(),
	TimeResultMilliseconds(),
	LastIndex( -1 )
{
}

template< int NumTimers, int NumFrames >
LogGpuTime<NumTimers,NumFrames>::~LogGpuTime()
{
#if defined( OVR_OS_ANDROID )
	for ( int i = 0; i < NumTimers; i++ )
	{
		if ( TimerQuery[i] != 0 )
		{
			glDeleteQueriesEXT_( 1, &TimerQuery[i] );
		}
	}
#endif
}

template< int NumTimers, int NumFrames >
bool LogGpuTime<NumTimers,NumFrames>::IsEnabled()
{
#if defined( OVR_OS_ANDROID )
	return UseTimerQuery && extensionsOpenGL.EXT_disjoint_timer_query;
#else
	return false;
#endif
}

template< int NumTimers, int NumFrames >
void LogGpuTime<NumTimers,NumFrames>::Begin( int index )
{
	// don't enable by default on Mali because this issues a glFinish() to work around a driver bug
	UseTimerQuery = ( AllowGpuTimerQueries != 0 );
	// use glQueryCounterEXT on Mali to time GPU rendering to a non-default FBO
	UseQueryCounter = ( AllowGpuTimerQueries == 2 );

#if defined( OVR_OS_ANDROID )
	if ( UseTimerQuery && extensionsOpenGL.EXT_disjoint_timer_query )
	{
		OVR_ASSERT( index >= 0 && index < NumTimers );
		OVR_ASSERT( LastIndex == -1 );
		LastIndex = index;

		if ( TimerQuery[index] )
		{
			for ( GLint available = 0; available == 0; )
			{
				glGetQueryObjectivEXT_( TimerQuery[index], GL_QUERY_RESULT_AVAILABLE, &available );
			}

			glGetIntegerv( GL_GPU_DISJOINT_EXT, &DisjointOccurred[index] );

			GLuint64 elapsedGpuTime = 0;
			glGetQueryObjectui64vEXT_( TimerQuery[index], GL_QUERY_RESULT_EXT, &elapsedGpuTime );

			TimeResultMilliseconds[index][TimeResultIndex[index]] = ( elapsedGpuTime - (GLuint64)BeginTimestamp[index] ) * 0.000001;
			TimeResultIndex[index] = ( TimeResultIndex[index] + 1 ) % NumFrames;
		}
		else
		{
			glGenQueriesEXT_( 1, &TimerQuery[index] );
		}
		if ( !UseQueryCounter )
		{
			BeginTimestamp[index] = 0;
			glBeginQueryEXT_( GL_TIME_ELAPSED_EXT, TimerQuery[index] );
		}
		else
		{
			glGetInteger64v_( GL_TIMESTAMP_EXT, &BeginTimestamp[index] );
		}
	}
#endif
}

template< int NumTimers, int NumFrames >
void LogGpuTime<NumTimers,NumFrames>::End( int index )
{
#if defined( OVR_OS_ANDROID )
	if ( UseTimerQuery && extensionsOpenGL.EXT_disjoint_timer_query )
	{
		OVR_ASSERT( index == LastIndex );
		LastIndex = -1;

		if ( !UseQueryCounter )
		{
			glEndQueryEXT_( GL_TIME_ELAPSED_EXT );
		}
		else
		{
			glQueryCounterEXT_( TimerQuery[index], GL_TIMESTAMP_EXT );
			// Mali workaround: check for availability once to make sure all the pending flushes are resolved
			GLint available = 0;
			glGetQueryObjectivEXT_( TimerQuery[index], GL_QUERY_RESULT_AVAILABLE, &available );
			// Mali workaround: need glFinish() when timing rendering to non-default FBO
			//glFinish();
		}
	}
#endif
}

template< int NumTimers, int NumFrames >
void LogGpuTime<NumTimers,NumFrames>::PrintTime( int index, const char * label ) const
{
#if defined( OVR_OS_ANDROID )
	if ( UseTimerQuery && extensionsOpenGL.EXT_disjoint_timer_query )
	{
//		double averageTime = 0.0;
//		for ( int i = 0; i < NumFrames; i++ )
//		{
//			averageTime += TimeResultMilliseconds[index][i];
//		}
//		averageTime *= ( 1.0 / NumFrames );
//		LOG( "%s %i: %3.1f %s", label, index, averageTime, DisjointOccurred[index] ? "DISJOINT" : "" );
	}
#endif
}

template< int NumTimers, int NumFrames >
double LogGpuTime<NumTimers,NumFrames>::GetTime( int index ) const
{
	double averageTime = 0;
	for ( int i = 0; i < NumFrames; i++ )
	{
		averageTime += TimeResultMilliseconds[index][i];
	}
	averageTime *= ( 1.0 / NumFrames );
	return averageTime;
}

template< int NumTimers, int NumFrames >
double LogGpuTime<NumTimers,NumFrames>::GetTotalTime() const
{
	double totalTime = 0;
	for ( int j = 0; j < NumTimers; j++ )
	{
		for ( int i = 0; i < NumFrames; i++ )
		{
			totalTime += TimeResultMilliseconds[j][i];
		}
	}
	totalTime *= ( 1.0 / NumFrames );
	return totalTime;
}

template class LogGpuTime<2,10>;
template class LogGpuTime<8,10>;
