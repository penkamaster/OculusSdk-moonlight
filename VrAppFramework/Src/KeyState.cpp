/************************************************************************************

Filename    :   KeyState.h
Content     :   Tracking of short-press, long-press and double-tapping of keys
Created     :   June 18, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "KeyState.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_LogUtils.h"
#include "SystemClock.h"

namespace OVR {

char const * KeyEventNames[KEY_EVENT_MAX] =
{
	"KEY_EVENT_NONE",
	"KEY_EVENT_SHORT_PRESS",
	"KEY_EVENT_DOWN",
	"KEY_EVENT_UP",
};

//==========================
// KeyState::KeyState
KeyState::KeyState( float const shortPressTime ) :
	NumEvents( 0 ),
	ShortPressTime( shortPressTime ),
	Down( false )
{
	Reset();
}

//==========================
// KeyState::HandleEvent
void KeyState::HandleEvent( double const time, bool const down, int const repeatCount )
{
	LOG_WITH_TAG( "KeyState", "(%.4f) HandleEvent: %s, NumEvents %i, RepeatCount %i",
		SystemClock::GetTimeInSeconds(), down ? "DOWN" : "UP", NumEvents, repeatCount );

	bool wasDown = this->Down;
	this->Down = down;

	if ( NumEvents <= 0 && !down )
	{
		// we ignore up events if we aren't currently tracking from a down -- this let's us exclude the up
		// event after a long press because we Reset() as soon as we fire the long-press event.
		PendingEvent = KEY_EVENT_NONE;
		return;
	}

	if ( repeatCount > 0 )
	{
		ASSERT_WITH_TAG( down == true, "KeyState" );	// only a hold should have a repeat count
		// key is held
		PendingEvent = KEY_EVENT_NONE;
		return;
	}

	if ( wasDown == down )
	{
		LOG_WITH_TAG( "KeyState", "wasDown != down" );	// this should always be a toggle unless we've missed an event, right?
		PendingEvent = KEY_EVENT_NONE;
		return;
	}

	// record the event times
	if ( NumEvents < MAX_EVENTS )
	{
		EventTimes[NumEvents++] = time;
	}

	// key going down
	if ( down &&  NumEvents == 1 && repeatCount == 0 )
	{
		PendingEvent = KEY_EVENT_DOWN;	// initial down event
		return;
	}

	PendingEvent = KEY_EVENT_NONE;
}

//==========================
// KeyState::Update
KeyEventType KeyState::Update( double const time )
{
	//LOG_WITH_TAG( "KeyState", "Update: NumEvents %i", NumEvents );
	if ( NumEvents > 0 )
	{
		// if we exceed the max time for key events we care about, reset.
		if ( NumEvents != 2 && time - EventTimes[0] >= 0.75f )
		{
			Reset();
			LOG_WITH_TAG( "KeyState", "(%.4f) Update() - discarding events (%i) after %.2f seconds.",
				SystemClock::GetTimeInSeconds(), NumEvents, time - EventTimes[0] );
			return KEY_EVENT_NONE;
		}
		if ( NumEvents == 2 )
		{

			// Only send a short press if the button went down once and up in less than the short press time.
			if ( EventTimes[1] - EventTimes[0] < ShortPressTime )
			{
				// the HMT button always releases a hold at 0.8 seconds right now :(
				LOG_WITH_TAG( "KeyState", "(%.4f) Update() - press released after %.2f seconds.",
					SystemClock::GetTimeInSeconds(), time - EventTimes[0] );

				Reset();
				return KEY_EVENT_SHORT_PRESS;
			}
			else
			{
				LOG_WITH_TAG( "KeyState", "(%.4f) Update() - discarding short-press after %.2f seconds.",
					SystemClock::GetTimeInSeconds(), time - EventTimes[0] );

				Reset();
				return KEY_EVENT_UP;
			}
		}
	}

	KeyEventType outEvent = PendingEvent;
	PendingEvent = KEY_EVENT_NONE;
	if ( outEvent != KEY_EVENT_NONE )
	{
		LOG_WITH_TAG( "KeyState", "outEvent %s", KeyEventNames[ outEvent ] );
	}
	return outEvent;
}

//==========================
// KeyState::Reset
void KeyState::Reset()
{
	Down = false;
	NumEvents = 0;
	for ( int i = 0; i < MAX_EVENTS; i++ )
	{
		EventTimes[i] = -1.0;
	}	PendingEvent = KEY_EVENT_NONE;
}

} // namespace OVR
