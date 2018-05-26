/************************************************************************************

Filename    :   OVR_PerfTimer.cpp
Content     :   Simple RAII class for timing blocks of code.
Created     :   1/22/2016
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_PerfTimer_h )
#define OVR_PerfTimer_h

#include "Kernel/OVR_LogUtils.h"
#include "SystemClock.h"

namespace OVR
{

class ovrPerfTimerAccumulator
{
public:
	ovrPerfTimerAccumulator( char const * const message )
		: Message( message )
		, TotalTime( 0.0 )
	{
	}

	~ovrPerfTimerAccumulator()
	{
		if ( TotalTime > 0.0 )
		{
			LOG_WITH_TAG( "OVRPerf", "%s : ~Total %f seconds\n", Message, TotalTime );
			TotalTime = 0.0;
		}
	}

	void	Accumulate( double const t )
	{
		TotalTime += t;
	}

	void	Report( char const * extraMsg ) const
	{
		if ( extraMsg == nullptr )
		{
			LOG_WITH_TAG( "OVRPerf", "%s : Total %f seconds\n", Message, TotalTime );
		}
		else
		{
			LOG_WITH_TAG( "OVRPerf", "%s( %s ) : Total %f seconds\n", Message, extraMsg, TotalTime );
		}
		TotalTime = 0.0;
	}

	double	GetTotalTime() const { return TotalTime; }

private:
	char const * const	Message;
	mutable double		TotalTime;

private:
	ovrPerfTimerAccumulator( ovrPerfTimerAccumulator const & ) = delete;
	ovrPerfTimerAccumulator operator = ( ovrPerfTimerAccumulator const & ) = delete;
};

class ovrPerfTimer
{
public:
	friend class ovrPerfTimerAccumulator;

	ovrPerfTimer( char const * const message, class ovrPerfTimerAccumulator * accumulator )
		: Message( message )
		, StartTime( -1.0 )
		, Accumulator( accumulator )
	{
		StartTime = SystemClock::GetTimeInSeconds();
	}

	~ovrPerfTimer()
	{
		Stop( nullptr, true );
	}

	double				Stop( char const * extraMsg, bool const report )
	{
		if ( StartTime < 0.0 )
		{
			return 0.0;
		}

		double const endTime = SystemClock::GetTimeInSeconds();
		double const totalTime = endTime - StartTime;
		StartTime = -1.0;
		if ( Accumulator != nullptr )	// never report ourselves if we have an accumulator
		{
			Accumulator->Accumulate( totalTime );
		}
		else if ( report )
		{
			if ( extraMsg != nullptr )
			{
				LOG_WITH_TAG( "OVRPerf", "%s( %s ): %f seconds\n", Message, extraMsg, totalTime );
			}
			else
			{
				LOG_WITH_TAG( "OVRPerf", "%s : %f seconds\n", Message, totalTime );
			}
		}
		return totalTime;
	}

private:
	ovrPerfTimer( ovrPerfTimer const & ) = delete;
	ovrPerfTimer operator = ( ovrPerfTimer const & ) = delete;

	char const * const				Message;
	double							StartTime;
	class ovrPerfTimerAccumulator *	Accumulator;
};

// To time code, define OVR_USE_PERF_TIMER, then include OVR_PerfTimer.h:
//
// #define OVR_USE_PERF_TIMER
// #include "OVR_PerfTimer.h"
//
// Then use the OVR_PTIMER macro in some scope to time that scope:
//
// if ( checkValid )
// {
//     OVR_PTIMER( "CheckValid" );
//     [... code to time...]
// }
//
// On exiting the scope, ovrPerfTimer will deconstruct and output the
// time spent in the scope.

#if defined( OVR_USE_PERF_TIMER )
#	define OVR_PERF_TIMER( name_ )	ovrPerfTimer name_##_Timer( #name_, nullptr )
#else
#	define OVR_PERF_TIMER( name_ ) 
#endif // OVR_USE_PERF_TIMER

// To accumulate a timer over multiple passes use OVR_PERF_ACCUMULATOR() with the same
// name token as the timer you want to accumulate.
// Call OVR_PERF_ACCUMULATE() with that same name token BEFORE the timer deconstructs.
// When you want to output the total accumulated time, use OVR_PERF_REPORT().
// If you need to reference the timer in a compilation unit other than where it is
// defined, use OVR_PERF_ACCUMULATOR() at file scope, then in the other compilation
// unit, use OVR_PERF_ACCUMULATR_EXTERN() to declare the extern definition of the
// accumulator.
#if defined( OVR_USE_PERF_TIMER )
#	define OVR_PERF_ACCUMULATOR( name_ ) ovrPerfTimerAccumulator name_##_Accumulator( #name_ )
#	define OVR_PERF_ACCUMULATE( name_ ) ovrPerfTimer name_##_Timer( #name_, & name_##_Accumulator )
#	define OVR_PERF_REPORT( name_ ) name_##_Accumulator.Report( nullptr )
#	define OVR_PERF_REPORT_MSG( name_, msg_ ) name_##_Accumulator.Report( msg_ )
#	define OVR_PERF_ACCUMULATOR_EXTERN( name_ ) extern ovrPerfTimerAccumulator name_##_Accumulator
#	define OVR_PERF_TIMER_STOP( name_ ) name_##_Timer.Stop( nullptr, true )
#	define OVR_PERF_TIMER_STOP_MSG( name_, msg_ ) name_##_Timer.Stop( msg_, true )
#else
#	define OVR_PERF_ACCUMULATOR( name_ ) 
#	define OVR_PERF_ACCUMULATE( name_ ) 
#	define OVR_PERF_REPORT( name_ )
#	define OVR_PERF_REPORT_MSG( name_, msg_ )
#	define OVR_PERF_ACCUMULATOR_EXTERN( name_ ) 
#	define OVR_PERF_TIMER_STOP( name_ )
#	define OVR_PERF_TIMER_STOP_MSG( name_, msg_ )
#endif // OVR_USE_PERF_TIMER

} // namespace OVR

#endif // OVR_PerfTimer_h
