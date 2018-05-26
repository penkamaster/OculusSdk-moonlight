/*
================================================================================================================================

Signal for thread synchronization, similar to a Windows event object.

Windows event objects come in two types: auto-reset events and manual-reset events. A Windows event object
can be signalled by calling either SetEvent or PulseEvent.

When a manual-reset event is signaled by calling SetEvent, it sets the event into the signaled state and
wakes up all threads waiting on the event. The manual-reset event remains in the signalled state until
the event is manually reset. When an auto-reset event is signaled by calling SetEvent and there are any
threads waiting, it wakes up only one thread and resets the event to the non-signaled state. If there are
no threads waiting for an auto-reset event, then the event remains signaled until a single waiting thread
waits on it and is released.

When a manual-reset event is signaled by calling PulseEvent, it wakes up all waiting threads and atomically
resets the event. When an auto-reset event is signaled by calling PulseEvent, and there are any threads
waiting, it wakes up only one thread and resets the event to the non-signaled state. If there are no threads
waiting, then no threads are released and the event is set to the non-signaled state.

Compared to a POSIX condition variable, a Windows event object is limited in functionality. Unlike a
Windows event object, the expression waited upon by a POSIX condition variable can be arbitrarily complex.
Furthermore, there is no way to release just one waiting thread with a manual-reset Windows event object.
Similarly there is no way to release all waiting threads with an auto-reset event. These limitations make
it difficult to simulate a POSIX condition variable using Windows event objects.

Windows Vista and later implement PCONDITION_VARIABLE, but as Douglas C. Schmidt and Irfan Pyarali point
out in:

	1. "Strategies for Implementing POSIX Condition Variables on Win32"
	   http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
	2. "Patterns for POSIX Condition Variables on Win32"
	   http://www.cs.wustl.edu/~schmidt/win32-cv-2.html
	
it is complicated to simulate a POSIX condition variable on prior versions of Windows without causing
unfair or even incorrect behavior. Even using SignalObjectAndWait is not safe because as per the Microsoft
documentation: "Note that the 'signal' and 'wait' are not guaranteed to be performed as an atomic operation.
Threads executing on other processors can observe the signaled state of the first object before the thread
calling SignalObjectAndWait begins its wait on the second object."

Simulating a Windows event object using a POSIX condition variable is fairly straight forward, which
is done here. However, this implementation does not support the equivalent of PulseEvent, because
PulseEvent is unreliable. On Windows, a thread waiting on an event object can be momentarily removed
from the wait state by a kernel-mode Asynchronous Procedure Call (APC), and then returned to the wait
state after the APC is complete. If a call to PulseEvent occurs during the time when the thread has
been temporarily removed from the wait state, then the thread will not be released, because PulseEvent
releases only those threads that are in the wait state at the moment PulseEvent is called.

Signal_t

static void Signal_Create( Signal_t * signal, const bool autoReset );
static void Signal_Destroy( Signal_t * signal );
static bool Signal_Wait( Signal_t * signal, const int timeOutMilliseconds );
static void Signal_Raise( Signal_t * signal );
static void Signal_Clear( Signal_t * signal );

================================================================================================================================
*/

#include "OVR_Signal.h"
#include "OVR_Types.h"

// unfortunate these need to be in the header file right now
#if defined( OVR_OS_WIN32 )
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#else
#	include <errno.h>
#	include <pthread.h>
#endif

namespace OVR {

typedef struct
{
#if defined( OVR_OS_WIN32 )
	HANDLE			handle;
#else
	pthread_mutex_t	mutex;
	pthread_cond_t	cond;
	int				waitCount;		// number of threads waiting on the signal
	bool			autoReset;		// automatically clear the signalled state when a single thread is released
	bool			signaled;		// in the signalled state if true
#endif
} Signal_t;

static void Signal_Create( Signal_t * signal, const bool autoReset )
{
#if defined( OVR_OS_WIN32 )
	signal->handle = CreateEvent( NULL, !autoReset, FALSE, NULL );
#else
	pthread_mutex_init( &signal->mutex, NULL );
	pthread_cond_init( &signal->cond, NULL );
	signal->waitCount = 0;
	signal->autoReset = autoReset;
	signal->signaled = false;
#endif
}

static void Signal_Destroy( Signal_t * signal )
{
#if defined( OVR_OS_WIN32 )
	CloseHandle( signal->handle );
#else
	pthread_cond_destroy( &signal->cond );
	pthread_mutex_destroy( &signal->mutex );
#endif
}

// Waits for the object to enter the signalled state and returns true if this state is reached within the time-out period.
// If 'autoReset' is true then the first thread that reaches the signalled state within the time-out period will clear the signalled state.
// If 'timeOutMilliseconds' is negative then this will wait indefinitely until the signalled state is reached.
// Returns true if the thread was released because the object entered the signalled state, returns false if the time-out is reached first.
bool Signal_Wait( Signal_t * signal, const int timeOutMilliseconds )
{
#if defined( OVR_OS_WIN32 )
	DWORD result = WaitForSingleObject( signal->handle, timeOutMilliseconds < 0 ? INFINITE : timeOutMilliseconds );
	OVR_ASSERT( result == WAIT_OBJECT_0 || ( timeOutMilliseconds >= 0 && result == WAIT_TIMEOUT ) );
	return ( result == WAIT_OBJECT_0 );
#else
	bool released = false;
	pthread_mutex_lock( &signal->mutex );
	if ( signal->signaled )
	{
		released = true;
	}
	else
	{
		signal->waitCount++;
		if ( timeOutMilliseconds < 0 )
		{
			do
			{
				pthread_cond_wait( &signal->cond, &signal->mutex );
				// Must re-check condition because pthread_cond_wait may spuriously wake up.
			} while ( signal->signaled == false );
		}
		else
		{
			struct timeval tp;
			gettimeofday( &tp, NULL );
			struct timespec ts;
			ts.tv_sec = tp.tv_sec + timeOutMilliseconds / 1000;
			ts.tv_nsec = tp.tv_usec * 1000 + ( timeOutMilliseconds % 1000 ) * 1000000;

			// NOTE: tv_nsec is only valid if >= 0 and < number of nanoseconds in a second.
			if ( ts.tv_nsec > 999999999 )
			{
				ts.tv_sec++;
				ts.tv_nsec -= 1000000000;
			}

			do
			{
				if ( pthread_cond_timedwait( &signal->cond, &signal->mutex, &ts ) == ETIMEDOUT )
				{
					break;
				}
				// Must re-check condition because pthread_cond_timedwait may spuriously wake up.
			} while ( signal->signaled == false );
		}
		released = signal->signaled;
		signal->waitCount--;
	}
	if ( released && signal->autoReset )
	{
		signal->signaled = false;
	}
	pthread_mutex_unlock( &signal->mutex );
	return released;
#endif
}

// Enter the signalled state.
// Note that if 'autoReset' is true then this will only release a single thread.
void Signal_Raise( Signal_t * signal )
{
#if defined( OVR_OS_WIN32 )
	SetEvent( signal->handle );
#else
	pthread_mutex_lock( &signal->mutex );
	signal->signaled = true;
	if ( signal->waitCount > 0 )
	{
		pthread_cond_broadcast( &signal->cond );
	}
	pthread_mutex_unlock( &signal->mutex );
#endif
}

// Clear the signalled state.
// Should not be needed for auto-reset signals (autoReset == true).
void Signal_Clear( Signal_t * signal )
{
#if defined( OVR_OS_WIN32 )
	ResetEvent( signal->handle );
#else
	pthread_mutex_lock( & signal->mutex );
	signal->signaled = false;
	pthread_mutex_unlock( & signal->mutex );
#endif
}

//==============================================================
// ovrSignalImpl
//
class ovrSignalImpl : public ovrSignal
{
public:
	ovrSignalImpl( const bool autoReset );
	virtual	~ovrSignalImpl();

	virtual	bool	Wait( const int timeOutMilliseconds );
	virtual	void	Raise();
	virtual	void	Clear();

private:
	Signal_t	Signal;
};

ovrSignalImpl::ovrSignalImpl( const bool autoReset )
{
	Signal_Create( &Signal, autoReset );
}

ovrSignalImpl::~ovrSignalImpl()
{
	Signal_Destroy( &Signal );
}

bool ovrSignalImpl::Wait( const int timeOutMilliseconds )
{
	return Signal_Wait( &Signal, timeOutMilliseconds );
}

void ovrSignalImpl::Raise()
{
	Signal_Raise( &Signal );
}

void ovrSignalImpl::Clear()
{
	Signal_Clear( &Signal );
}

//==============================================================================================
// public static functions for creating and destroying ovrSignal objects
//==============================================================================================
ovrSignal *	ovrSignal::Create( bool const autoReset )
{
	return new ovrSignalImpl( autoReset );
}

void ovrSignal::Destroy( ovrSignal * & signal )
{
	delete signal;
	signal = nullptr;
}

} // namespace OVR
