/************************************************************************************

Filename    :   OVR_Signal.cpp
Content     :   Signal for thread synchronization, similar to a Windows event object.
Created     :   June 6, 2014
Authors     :   J.M.P van Waveren and Jonathan E. Wright
Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_Signal_h )
#define OVR_Signal_h

namespace OVR {

//==============================================================
// ovrSignal
// Abstract class for a a synchronization object
class ovrSignal
{
public:
	virtual ~ovrSignal() { }

	static ovrSignal *	Create( bool const autoReset );
	static void			Destroy( ovrSignal * & signal );

	// Waits for the object to enter the signalled state and returns true if this state is reached within the time-out period.
	// If 'autoReset' is true then the first thread that reaches the signalled state within the time-out period will clear the signalled state.
	// If 'timeOutMilliseconds' is negative then this will wait indefinitely until the signalled state is reached.
	// Returns true if the thread was released because the object entered the signalled state, returns false if the time-out is reached first.
	virtual	bool	Wait( const int timeOutMilliseconds ) = 0;
	// Enter the signalled state.
	// Note that if 'autoReset' is true then this will only release a single thread.
	virtual	void	Raise() = 0;
	// Clear the signalled state.
	// Should not be needed for auto-reset signals (autoReset == true).
	virtual	void	Clear() = 0;
};

} // namespace OVR

#endif
