/************************************************************************************

Filename    :   SoundLimiter.cpp
Content     :   Utility class for limiting how often sounds play.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "SoundLimiter.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_BitFlags.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_LogUtils.h"

#include "OVR_Input.h"
#include "VrCommon.h"
#include "GuiSys.h"
#include "SystemClock.h"

namespace OVR {

//==============================
// ovrSoundLimiter::PlaySound
void ovrSoundLimiter::PlaySoundEffect( OvrGuiSys & guiSys, char const * soundName, double const limitSeconds )
{
	double curTime = SystemClock::GetTimeInSeconds();
	double t = curTime - LastPlayTime;
	//LOG_WITH_TAG( "VrMenu", "PlaySoundEffect( '%s', %.2f ) - t == %.2f : %s", soundName, limitSeconds, t, t >= limitSeconds ? "PLAYING" : "SKIPPING" );
	if ( t >= limitSeconds )
	{
		guiSys.GetSoundEffectPlayer().Play( soundName );
		LastPlayTime = curTime;
	}
}

void ovrSoundLimiter::PlayMenuSound( OvrGuiSys & guiSys, char const * appendKey, char const * soundName, double const limitSeconds )
{
	char overrideSound[ 1024 ];
	OVR_sprintf( overrideSound, 1024, "%s_%s", appendKey, soundName );

	if ( guiSys.GetSoundEffectPlayer().Has( overrideSound ) )
	{
		PlaySoundEffect( guiSys, overrideSound, limitSeconds );
	}
	else
	{
		PlaySoundEffect( guiSys, soundName, limitSeconds );
	}
}

} // namespace OVR
