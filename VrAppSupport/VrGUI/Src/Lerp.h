/************************************************************************************

Filename    :   Lerp.h
Content     :	Simple floating point interpolation.
Created     :	8/25/2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( Lerp_h )
#define Lerp_h

#include "Kernel/OVR_Alg.h"

namespace OVR {

class Lerp
{
public:
	Lerp() : startDomain( 0.0 )
			, endDomain( 0.0 )
			, startValue( 0.0 )
			, endValue( 0.0 )
	{}
		
	void	Set( double startDomain_, double startValue_, double endDomain_, double endValue_ )
	{
		startDomain = startDomain_;
		endDomain = endDomain_;
		startValue = startValue_;
		endValue = endValue_;
	}

	double	Value( double domain ) const
	{
		const double f = OVR::Alg::Clamp( ( domain - startDomain ) / ( endDomain - startDomain ), 0.0, 1.0 );
		return startValue * ( 1.0 - f ) + endValue * f;
	}

	double	startDomain;
	double	endDomain;
	double	startValue;
	double	endValue;
};

} // namespace OVR

#endif // Lerp_h
