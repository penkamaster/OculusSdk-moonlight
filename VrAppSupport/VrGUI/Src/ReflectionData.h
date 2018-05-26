/************************************************************************************

Filename    :   ReflectionData.h
Content     :   Data for introspection and reflection of C++ objects.
Created     :   11/16/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_ReflectionData_h )
#define OVR_ReflectionData_h

#include "Reflection.h"

#define MEMBER_SIZE( type_, member_ ) sizeof( ((type_*)0)->member_ )

namespace OVR {

//=============================================================================================
// Reflection Data
//=============================================================================================

extern ovrTypeInfo TypeInfoList[];

} // namespace OVR

#endif // OVR_ReflectionData_h
