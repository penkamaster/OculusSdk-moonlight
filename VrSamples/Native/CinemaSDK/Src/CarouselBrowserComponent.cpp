/************************************************************************************

Filename    :   CarouselBrowserComponent.h
Content     :   A menu for browsing a a group of items on a carousel in front of the user
Created     :   July 25, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "CarouselBrowserComponent.h"
#include "App.h"
#include "GuiSys.h"
#include "VRMenuMgr.h"

using namespace OVR;

namespace OculusCinema {

//==============================================================
// CarouselBrowserComponent
CarouselBrowserComponent::CarouselBrowserComponent( const Array<CarouselItem *> &items, const Array<PanelPose> &panelPoses ) :
	VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | 	VRMENU_EVENT_TOUCH_DOWN | 
		VRMENU_EVENT_SWIPE_FORWARD | VRMENU_EVENT_SWIPE_BACK | VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_OPENED | VRMENU_EVENT_CLOSED ),
		SelectPressed( false ), PositionScale( 1.0f ), Position( 0.0f ), TouchDownTime( -1.0 ),
		Items(), MenuObjs(), MenuComps(), PanelPoses( panelPoses ),
		StartTime( 0.0 ), EndTime( 0.0 ), PrevPosition( 0.0f ), NextPosition( 0.0f ), Swiping( false ), PanelsNeedUpdate( false )

{
	SetItems( items );
}

eMsgStatus CarouselBrowserComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
	VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_ASSERT( HandlesEvent( VRMenuEventFlags_t( event.EventType ) ) );

	switch( event.EventType )
	{
		case VRMENU_EVENT_FRAME_UPDATE:
			return Frame( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_TOUCH_DOWN:
			return TouchDown( guiSys, vrFrame, self, event);
		case VRMENU_EVENT_TOUCH_UP:
			return TouchUp( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_OPENED:
			return Opened( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_CLOSED:
			return Closed( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_SWIPE_FORWARD:
			return SwipeForward( guiSys, vrFrame, self );
		case VRMENU_EVENT_SWIPE_BACK:
			return SwipeBack( guiSys, vrFrame, self );
		default:
			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
			return MSG_STATUS_ALIVE;
	}
}

void CarouselBrowserComponent::SetPanelPoses( OvrVRMenuMgr & menuMgr, VRMenuObject * self, const Array<PanelPose> &panelPoses )
{
	PanelPoses = panelPoses;
	UpdatePanels( menuMgr, self );
}

void CarouselBrowserComponent::SetMenuObjects( const Array<VRMenuObject *> &menuObjs, const Array<CarouselItemComponent *> &menuComps )
{
	MenuObjs = menuObjs;
	MenuComps = menuComps;

	OVR_ASSERT( MenuObjs.GetSizeI() == MenuObjs.GetSizeI() );
}

PanelPose CarouselBrowserComponent::GetPosition( const float t )
{
	int index = ( int )floor( t );
	float frac = t - ( float )index;

	PanelPose pose;

	if ( index < 0 )
	{
		pose = PanelPoses[ 0 ];
	}
	else if ( ( index == PanelPoses.GetSizeI() - 1 ) && ( fabs( frac ) <= 0.00001f ) )
	{
		pose = PanelPoses[ PanelPoses.GetSizeI() - 1 ];
	}
	else if ( index >= PanelPoses.GetSizeI() - 1 )
	{
		pose.Orientation = Quatf();
		pose.Position = Vector3f( 0.0f, 0.0f, 0.0f );
		pose.Color = Vector4f( 0.0f, 0.0f, 0.0f, 0.0f );
	}
	else
	{
		pose.Orientation = PanelPoses[ index + 1 ].Orientation.Nlerp( PanelPoses[ index ].Orientation, frac ); // NLerp has the frac inverted
		pose.Position = PanelPoses[ index ].Position.Lerp( PanelPoses[ index + 1 ].Position, frac );
		pose.Color = PanelPoses[ index ].Color * ( 1.0f - frac ) + PanelPoses[ index + 1 ].Color * frac;
	}

	pose.Position = pose.Position * PositionScale;

	return pose;
}

void CarouselBrowserComponent::SetSelectionIndex( const int selectedIndex )
{
	if ( ( selectedIndex >= 0 ) && ( selectedIndex < Items.GetSizeI() ) )
	{
		Position = static_cast<float>( selectedIndex );
	}
	else
	{
		Position = 0.0f;
	}

	NextPosition = Position;
	Swiping = false;
	PanelsNeedUpdate = true;
}

int CarouselBrowserComponent::GetSelection() const
{
	int itemIndex = static_cast<int>( floor( Position + 0.5f ) );
	if ( ( itemIndex >= 0 ) && ( itemIndex < Items.GetSizeI() ) )
	{
		return itemIndex;
	}

	return -1;
}

bool CarouselBrowserComponent::HasSelection() const
{
	if ( Items.GetSize() == 0 )
	{
		return false;
	}

	return !Swiping;
}

bool CarouselBrowserComponent::CanSwipeBack() const
{
	float nextPos = floorf( Position ) - 1.0f;
	return ( nextPos >= 0.0f );
}

bool CarouselBrowserComponent::CanSwipeForward() const
{
	float nextPos = floorf( Position ) + 1.0f;
	return ( nextPos < Items.GetSizeI() );
}

void CarouselBrowserComponent::UpdatePanels( OvrVRMenuMgr & menuMgr, VRMenuObject * self )
{
	OVR_UNUSED( menuMgr );
	OVR_UNUSED( self );

	int centerIndex = static_cast<int>( floor( Position ) );
	float offset = centerIndex - Position;
	int leftIndex = centerIndex - PanelPoses.GetSizeI() / 2;

	int itemIndex = leftIndex;
	for ( int i = 0; i < MenuObjs.GetSizeI(); i++, itemIndex++ )
	{
		PanelPose pose = GetPosition( ( float )i + offset );
		if ( ( itemIndex < 0 ) || ( itemIndex >= Items.GetSizeI() ) || ( ( offset < 0.0f ) && ( i == 0 ) ) )
		{
			MenuComps[ i ]->SetItem( MenuObjs[ i ], NULL, pose );
		}
		else
		{
			MenuComps[ i ]->SetItem( MenuObjs[ i ], Items[ itemIndex ], pose );
		}
	}

	PanelsNeedUpdate = false;
}

void CarouselBrowserComponent::CheckGamepad( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self )
{
	if ( Swiping )
	{
		return;
	}

	if ( CanSwipeBack() && ( ( vrFrame.Input.buttonState & BUTTON_DPAD_LEFT ) || ( vrFrame.Input.sticks[0][0] < -0.5f ) ) )
	{
		SwipeBack( guiSys, vrFrame, self );
		return;
	}

	if ( CanSwipeForward() && ( ( vrFrame.Input.buttonState & BUTTON_DPAD_RIGHT ) || ( vrFrame.Input.sticks[0][0] > 0.5f ) ) )
	{
		SwipeForward( guiSys, vrFrame, self );
		return;
	}
}

eMsgStatus CarouselBrowserComponent::Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( event );

	if ( Swiping )
	{
		float frac = static_cast<float>( ( vrFrame.PredictedDisplayTimeInSeconds - StartTime ) / ( EndTime - StartTime ) );
		if ( frac >= 1.0f )
		{
			frac = 1.0f;
			Swiping = false;
		}

		float easeOutQuad = -1.0f * frac * ( frac - 2.0f );

		Position = PrevPosition * ( 1.0f - easeOutQuad ) + NextPosition * easeOutQuad;

		PanelsNeedUpdate = true;
	}

	if ( PanelsNeedUpdate )
	{
		UpdatePanels( guiSys.GetVRMenuMgr(), self );
	}

	return MSG_STATUS_ALIVE;
}

eMsgStatus CarouselBrowserComponent::SwipeForward( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self )
{
	if ( !Swiping )
	{
		float nextPos = floorf( Position ) + 1.0f;
		if ( nextPos < Items.GetSizeI() )
		{
			guiSys.GetSoundEffectPlayer().Play( "carousel_move" );
			PrevPosition = Position;
			StartTime = vrFrame.PredictedDisplayTimeInSeconds;
			EndTime = StartTime + 0.25;
			NextPosition = nextPos;
			Swiping = true;
		}
	}

	return MSG_STATUS_CONSUMED;
}

eMsgStatus CarouselBrowserComponent::SwipeBack( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self )
{
	if ( !Swiping )
	{
		float nextPos = floorf( Position ) - 1.0f;
		if ( nextPos >= 0.0f )
		{
			guiSys.GetSoundEffectPlayer().Play( "carousel_move" );
			PrevPosition = Position;
			StartTime = vrFrame.PredictedDisplayTimeInSeconds;
			EndTime = StartTime + 0.25;
			NextPosition = nextPos;
			Swiping = true;
		}
	}

	return MSG_STATUS_CONSUMED;
}

eMsgStatus CarouselBrowserComponent::TouchDown( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	//LOG( "TouchDown" );

	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );

	TouchDownTime = vrapi_GetTimeInSeconds();

	if ( Swiping )
	{
		return MSG_STATUS_CONSUMED;
	}

	return MSG_STATUS_ALIVE;	// don't consume -- we're just listening
}

eMsgStatus CarouselBrowserComponent::TouchUp( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	//LOG( "TouchUp" );

	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );

	float const timeTouchHasBeenDown = (float)( vrapi_GetTimeInSeconds() - TouchDownTime );
	TouchDownTime = -1.0;

	float dist = event.FloatValue.LengthSq();
	if ( !Swiping && ( dist < 20.0f ) && ( timeTouchHasBeenDown < 1.0f ) )
	{
		LOG( "Selectmovie" );
		SelectPressed = true;
	}
	else if ( Swiping )
	{
		return MSG_STATUS_CONSUMED;
	}

	//LOG( "Ignore: %f, %f", RotationalVelocity, ( float )timeTouchHasBeenDown );
	return MSG_STATUS_ALIVE; // don't consume -- we are just listening
}

eMsgStatus CarouselBrowserComponent::Opened( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );
	OVR_UNUSED( event );

	Swiping = false;
	Position = floorf( Position );
	SelectPressed = false;
	return MSG_STATUS_ALIVE;
}

eMsgStatus CarouselBrowserComponent::Closed( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );
	OVR_UNUSED( event );

	SelectPressed = false;
	return MSG_STATUS_ALIVE;
}

void CarouselBrowserComponent::SetItems( const Array<CarouselItem *> &items )
{
	Items = items;
	SelectPressed = false;
	Position = 0.0f;
	TouchDownTime = -1.0;
	StartTime = 0.0;
	EndTime = 0.0;
	PrevPosition = 0.0f;
	NextPosition = 0.0f;
	PanelsNeedUpdate = true;
}

} // namespace OculusCinema
