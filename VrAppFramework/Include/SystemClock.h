/************************************************************************************

Filename    :   SystemClock.h
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2018 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#if !defined( OVR_SystemClock )
#define OVR_SystemClock

class SystemClock
{
public:
	static double GetTimeInNanoSeconds();
	static double GetTimeInSeconds();
	static double GetTimeOfDaySeconds();
	static double DeltaTimeInSeconds( double startTime );
	static double DeltaTimeInSeconds( double startTime, double endTime );
};

#endif // OVR_SystemClock
