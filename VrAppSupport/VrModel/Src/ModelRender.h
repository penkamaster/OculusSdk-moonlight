/************************************************************************************

Filename    :   ModelRender.h
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_ModelRender_h
#define OVR_ModelRender_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"

#include "OVR_GlUtils.h"
#include "SurfaceRender.h"
#include "ModelFile.h"

namespace OVR
{
// The model surfaces are culled and added to the sorted surface list.
// Application specific surfaces from the emit list are also added to the sorted surface list.
// The surface list is sorted such that opaque surfaces come first, sorted front-to-back,
// and transparent surfaces come last, sorted back-to-front.
void BuildModelSurfaceList(	Array<ovrDrawSurface> & surfaceList,
							const Array<ModelNodeState *> & emitNodes,
							const Array<ovrDrawSurface> & emitSurfaces,
							const Matrix4f & viewMatrix,
							const Matrix4f & projectionMatrix );

} // namespace OVR

#endif	// OVR_ModelRender_h
