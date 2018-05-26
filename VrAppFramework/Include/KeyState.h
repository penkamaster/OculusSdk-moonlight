/************************************************************************************

Filename    :   KeyState.h
Content     :   Tracking of short-press, long-press
Created     :   June 18, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef Ovr_KeyState_h
#define Ovr_KeyState_h

#include "OVR_Input.h"

namespace OVR {

//==============================================================
// KeyState
//
// Single-press / Short-press
// |-------------------------|
// down ---> down_time ---> up ---> up_time .........
//
// Because we're detecting long-presses, we don't know at the point of a down
// whether this is going to be a short- or long-press.  We must wait for one
// of the two possible state changes, which is an up event (signaling a short-
// press), or the expiration of the long-press time (signaling a long press).
// In the case of a long-press, we want the long-press to signal as soon as
// the down time has exceeded the long-press time. If we didn't do this, the
// user would not have any indication of when they exceeded the long-press
// time (unless we add a sound - which doesn't work if the device is muted -
// or something like a cursor change). Because we are acting on the time 
// threshold and not a button up, the up will come after the long-press is 
// released and we need to ignore that button up to avoid it being treated
// as if it were the up from a short-press


class KeyState
{
public:
	static const int MAX_EVENTS = 2;

					KeyState( float const doubleTapTime );

	void			HandleEvent( double const time, bool const down, int const repeatCount );

	KeyEventType	Update( double const time );

	void			Reset();

	bool			IsDown() const { return Down; }

	float			GetShortPressTime() const { return ShortPressTime; }
	void			SetShortPressTime( const float shortPressTime ) { ShortPressTime = shortPressTime; }

private:
	int				NumEvents;				// number of events that have occurred
	double			EventTimes[MAX_EVENTS];	// times for first down, up, second down
	float			ShortPressTime;			// down up must occur in this time for a short press to occur
	bool			Down;					// true if the key is down
	KeyEventType	PendingEvent;			// next pending event== stored so that the client only has to handle events in one place
};

} // namespace OVR

#endif // Ovr_KeyState_h
