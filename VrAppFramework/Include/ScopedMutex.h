/************************************************************************************

Filename    :   ScopedMutex.h
Content     :   RAII class for locking / unlocking a mutex.
Created     :   12/15/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_ScopedMutex_h )
#define OVR_ScopedMutex_h

#include "Kernel/OVR_Threads.h"

namespace OVR {

//==============================================================
// ovrScopedMutex
class ovrScopedMutex
{
public:
	ovrScopedMutex( OVR::Mutex & mutex )
		: ThisMutex( mutex )
	{
		ThisMutex.DoLock();
	}
	~ovrScopedMutex()
	{
		ThisMutex.Unlock();
	}

	ovrScopedMutex &	operator = ( ovrScopedMutex & rhs ) = delete;

private:
	OVR::Mutex &	ThisMutex;
};

} // namespace OVR

#endif	// OVR_ScopedMutex_h
