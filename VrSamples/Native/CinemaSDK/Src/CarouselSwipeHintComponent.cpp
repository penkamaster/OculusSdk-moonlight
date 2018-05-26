/************************************************************************************

Filename    :   CarouselSwipeHintComponent.cpp
Content     :   Menu component for the swipe hints.
Created     :   October 6, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "CarouselSwipeHintComponent.h"
#include "CarouselBrowserComponent.h"
#include "OVR_Input.h"
#include "GuiSys.h"

namespace OculusCinema {

const char * CarouselSwipeHintComponent::TYPE_NAME = "CarouselSwipeHintComponent";

bool CarouselSwipeHintComponent::ShowSwipeHints = true;

//==============================
//  CarouselSwipeHintComponent::CarouselSwipeHintComponent()
CarouselSwipeHintComponent::CarouselSwipeHintComponent( CarouselBrowserComponent *carousel, const bool isRightSwipe, const float totalTime, const float timeOffset, const float delay ) :
	VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_OPENING ),
	Carousel( carousel ),
	IsRightSwipe( isRightSwipe ),
    TotalTime( totalTime ),
    TimeOffset( timeOffset ),
    Delay( delay ),
    StartTime( 0 ),
    ShouldShow( false ),
    IgnoreDelay( false ),
    TotalAlpha()

{
}

//==============================
//  CarouselSwipeHintComponent::Reset
void CarouselSwipeHintComponent::Reset( VRMenuObject * self )
{
	IgnoreDelay = true;
	ShouldShow = false;
	const double now = vrapi_GetTimeInSeconds();
	TotalAlpha.Set( now, TotalAlpha.Value( now ), now, 0.0f );
	self->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, 0.0f ) );
}

//==============================
//  CarouselSwipeHintComponent::CanSwipe
bool CarouselSwipeHintComponent::CanSwipe() const
{
	return IsRightSwipe ? Carousel->CanSwipeForward() : Carousel->CanSwipeBack();
}

//==============================
//  CarouselSwipeHintComponent::Show
void CarouselSwipeHintComponent::Show( const double now )
{
	if ( !ShouldShow )
	{
		ShouldShow = true;
		StartTime = now + TimeOffset + ( IgnoreDelay ? 0.0f : Delay );
		TotalAlpha.Set( now, TotalAlpha.Value( now ), now + 0.5f, 1.0f );
	}
}

//==============================
//  CarouselSwipeHintComponent::Hide
void CarouselSwipeHintComponent::Hide( const double now )
{
	if ( ShouldShow )
	{
		TotalAlpha.Set( now, TotalAlpha.Value( now ), now + 0.5f, 0.0f );
		ShouldShow = false;
	}
}

//==============================
//  CarouselSwipeHintComponent::OnEvent_Impl
eMsgStatus CarouselSwipeHintComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    switch( event.EventType )
    {
    	case VRMENU_EVENT_OPENING :
    		return Opening( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FRAME_UPDATE :
        	return Frame( guiSys, vrFrame, self, event );
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  CarouselSwipeHintComponent::Opening
eMsgStatus CarouselSwipeHintComponent::Opening( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	Reset( self );
	return MSG_STATUS_ALIVE;
}

//==============================
//  CarouselSwipeHintComponent::Frame
eMsgStatus CarouselSwipeHintComponent::Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	if ( ShowSwipeHints && Carousel->HasSelection() && CanSwipe() )
	{
		Show( vrFrame.PredictedDisplayTimeInSeconds );
	}
	else
	{
		Hide( vrFrame.PredictedDisplayTimeInSeconds );
	}

	IgnoreDelay = false;

	float alpha = static_cast<float>( TotalAlpha.Value( vrFrame.PredictedDisplayTimeInSeconds ) );
	if ( alpha > 0.0f )
	{
		double time = vrFrame.PredictedDisplayTimeInSeconds - StartTime;
		if ( time < 0.0f )
		{
			alpha = 0.0f;
		}
		else
		{
			float normTime = static_cast<float>( time / TotalTime );
			alpha *= sinf( MATH_FLOAT_PI * 2.0f * normTime );
			alpha = OVR::Alg::Max( alpha, 0.0f );
		}
	}

	self->SetColor( Vector4f( 1.0f, 1.0f, 1.0f, alpha ) );

	return MSG_STATUS_ALIVE;
}

} // namespace OculusCinema
