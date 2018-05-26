/************************************************************************************

Filename    :   SystemClock.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2018 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include "Kernel/OVR_Types.h"
#include "SystemClock.h"
#include <time.h>

double SystemClock::GetTimeInNanoSeconds()
{
#if defined( OVR_OS_ANDROID )
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return (double)now.tv_sec * 1e9 + now.tv_nsec;
#else
	return 0.0;
#endif
}

double SystemClock::GetTimeInSeconds()
{
#if defined( OVR_OS_ANDROID )
	return GetTimeInNanoSeconds() * 0.000000001;
#else
	return 0.0;
#endif
}

double SystemClock::GetTimeOfDaySeconds()
{
#if defined( OVR_OS_ANDROID )
	struct timeval now;
	gettimeofday( &now, 0 );
	const uint64_t result = (uint64_t)now.tv_sec * (uint64_t)( 1000 * 1000 ) + uint64_t( now.tv_usec );

	return ( result * 0.000001 );
#else
	return 0.0;
#endif
}

double SystemClock::DeltaTimeInSeconds( double startTime )
{
	return SystemClock::GetTimeInSeconds() - startTime;
}

double SystemClock::DeltaTimeInSeconds( double startTime, double endTime )
{
	return endTime - startTime;
}
