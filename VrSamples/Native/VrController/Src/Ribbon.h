/************************************************************************************

Filename    :   Ribbon.h
Content     :   Class that renders connected polygon strips from a list of points
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#pragma once

#include "PointList.h"
#include "SurfaceRender.h"

namespace OVR {

//==============================================================
// ovrRibbon
class ovrRibbon
{
public:
	ovrRibbon( const ovrPointList & pointList, const float width, const Vector4f & color );
	~ovrRibbon();

	void AddPoint( ovrPointList & pointList, const ovrVector3f & point );
	void Update( const ovrPointList & pointList, const ovrMatrix4f & centerViewMatrix, const bool invertAlpha );
	void GenerateSurfaceList( Array< ovrDrawSurface > & surfaceList ) const;

private:
	float 							HalfWidth;	
	Vector4f 						Color;
	ovrSurfaceDef					Surface;
	GlTexture						Texture;
};

} // namespace OVR
