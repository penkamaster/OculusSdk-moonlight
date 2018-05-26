/************************************************************************************

Filename    :   OVR_LogTimer.h
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_LogTimer )
#define OVR_LogTimer

#include "Kernel/OVR_LogUtils.h"
#include "SystemClock.h"

// Declaring a variable with this class will report the time elapsed when it
// goes out of scope.
class LogCpuTime
{
public:

	LogCpuTime( const char * fmt, ... )
	{
		va_list ap;
		va_start( ap, fmt );
#if defined( OVR_MSVC_SAFESTRING )
		vsnprintf_s( Label, sizeof( Label ), _TRUNCATE, fmt, ap );
#else
		vsnprintf( Label, sizeof( Label ), fmt, ap );
#endif
		va_end( ap );
		StartTimeNanoSec = SystemClock::GetTimeInNanoSeconds();
	}
	~LogCpuTime()
	{
		const double endTimeNanoSec = SystemClock::GetTimeInNanoSeconds();
		LOG( "%s took %6.4f seconds", Label, ( endTimeNanoSec - StartTimeNanoSec ) * 1e-9 );
	}

private:
	char			Label[1024];
	double			StartTimeNanoSec;
};

#define LOGCPUTIME( ... ) const LogCpuTime logCpuTimeObject( __VA_ARGS__ )

// Call LogGpuTime::Begin() and LogGpuTime::End() to log the GPU rendering time between begin and end.
// Note that begin-end blocks cannot overlap.
// This seems to cause some stability problems, so don't do it automatically.
template< int NumTimers, int NumFrames = 10 >
class LogGpuTime
{
public:
					LogGpuTime();
					~LogGpuTime();

	bool			IsEnabled();
	void			Begin( int index );
	void			End( int index );
	void			PrintTime( int index, const char * label ) const;
	double			GetTime( int index ) const;
	double			GetTotalTime() const;

private:
	bool			UseTimerQuery;
	bool			UseQueryCounter;
	uint32_t		TimerQuery[NumTimers];
	int64_t			BeginTimestamp[NumTimers];
	int32_t			DisjointOccurred[NumTimers];
	int32_t			TimeResultIndex[NumTimers];
	double			TimeResultMilliseconds[NumTimers][NumFrames];
	int				LastIndex;
};

// Allow GPU Timer Queries - NOTE: GPU Timer queries
// can cause instability on current Adreno driver.
void SetAllowGpuTimerQueries( int enable );

#endif // OVR_LogTimer
