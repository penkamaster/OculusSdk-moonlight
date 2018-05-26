/************************************************************************************

Filename    :   OVR_Lockless.h
Content     :   Lock-less classes for producer/consumer communication
Created     :   November 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014-2016 Oculus VR, LLC All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Lockless_h
#define OVR_Lockless_h

#include <atomic>

#if defined( OVR_OS_WIN32 )
#define NOMINMAX    // stop Windows.h from redefining min and max and breaking std::min / std::max
#include <windows.h>        // for MemoryBarrier
#endif

// Define this to compile-in Lockless test logic
//#define OVR_LOCKLESS_TEST

namespace OVR {


// ***** LocklessUpdater

// For single producer cases where you only care about the most recent update, not
// necessarily getting every one that happens (vsync timing, SensorFusion updates).
//
// This is multiple consumer safe.
//
// TODO: This is Android specific

template<class T>
class LocklessUpdater
{
public:
	LocklessUpdater() : UpdateBegin( 0 ), UpdateEnd( 0 ) {}

	LocklessUpdater( const LocklessUpdater & other ) : LocklessUpdater()
	{
		SetState( other.GetState() );
	}

	LocklessUpdater & operator = ( const LocklessUpdater & other )
	{
		if ( this != &other )
		{
			this->SetState( other.GetState() );
		}
		return *this;
	}

	T GetState() const
	{
		// This is the original code path with the copy on return, which exists for backwards compatibility
		// To avoid this copy, use GetState below
		T state;
		GetState( state );
		return state;
	}

	void GetState( T & state ) const
	{
		// Copy the state out, then retry with the alternate slot
		// if we determine that our copy may have been partially
		// stepped on by a new update.
		int	begin, end, final;

		for(;;)
		{
			end   = UpdateEnd.load(std::memory_order_acquire);
			state = Slots[ end & 1 ];
			// Manually insert an memory barrier here in order to ensure that
			// memory access between Slots[] and UpdateBegin are properly ordered
			#if defined(OVR_CC_MSVC)
				MemoryBarrier();
			#else
				__sync_synchronize();
			#endif
			begin = UpdateBegin.load(std::memory_order_acquire);
			if ( begin == end ) {
				return;
			}

			// The producer is potentially blocked while only having partially
			// written the update, so copy out the other slot.
			state = Slots[ (begin & 1) ^ 1 ];
			#if defined(OVR_CC_MSVC)
				MemoryBarrier();
			#else
				__sync_synchronize();
			#endif
			final = UpdateBegin.load(std::memory_order_acquire);
			if ( final == begin ) {
				return;
			}

			// The producer completed the last update and started a new one before
			// we got it copied out, so try fetching the current buffer again.
		}
	}

	void	SetState( const T & state )
	{
		const int slot = UpdateBegin.fetch_add(1, std::memory_order_seq_cst) & 1;
		// Write to (slot ^ 1) because fetch_add returns 'previous' value before add.
		Slots[slot ^ 1] = state;
		UpdateEnd.fetch_add(1, std::memory_order_seq_cst);
	}

	std::atomic<int> UpdateBegin;
	std::atomic<int> UpdateEnd;
	T		 Slots[2];
};


#ifdef OVR_LOCKLESS_TEST
void StartLocklessTest();
#endif


} // namespace OVR

#endif // OVR_Lockless_h

