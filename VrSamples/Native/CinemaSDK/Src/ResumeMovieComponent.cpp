/************************************************************************************

Filename    :   ResumeMovieComponent.cpp
Content     :
Created     :   September 4, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "ResumeMovieComponent.h"
#include "ResumeMovieView.h"
#include "CinemaApp.h"
#include "GuiSys.h"

namespace OculusCinema {

const Vector4f ResumeMovieComponent::FocusColor( 1.0f, 1.0f, 1.0f, 1.0f );
const Vector4f ResumeMovieComponent::HighlightColor( 1.0f, 1.0f, 1.0f, 1.0f );
const Vector4f ResumeMovieComponent::NormalColor( 82.0f / 255.0f, 101.0f / 255.0f, 120.0f / 255.0f, 255.0f / 255.0f );

//==============================
//  ResumeMovieComponent::
ResumeMovieComponent::ResumeMovieComponent( ResumeMovieView * view, int itemNum ) :
    VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) | 
            VRMENU_EVENT_TOUCH_UP | 
            VRMENU_EVENT_FOCUS_GAINED | 
            VRMENU_EVENT_FOCUS_LOST |
            VRMENU_EVENT_FRAME_UPDATE ),

    Icon( NULL ),
    Sound(),
	HasFocus( false ),
    ItemNum( itemNum ),
    CallbackView( view )
{
}

//==============================
//  ResumeMovieComponent::UpdateColor
void ResumeMovieComponent::UpdateColor( VRMenuObject * self )
{
	self->SetTextColor( HasFocus ? FocusColor : ( self->IsHilighted() ? HighlightColor : NormalColor ) );
	if ( Icon != NULL )
	{
		Icon->SetColor( HasFocus ? FocusColor : ( self->IsHilighted() ? HighlightColor : NormalColor ) );
	}
	self->SetColor( self->IsHilighted() ? Vector4f( 1.0f ) : Vector4f( 0.0f ) );
}

//==============================
//  ResumeMovieComponent::OnEvent_Impl
eMsgStatus ResumeMovieComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    switch( event.EventType )
    {
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FOCUS_GAINED:
            return FocusGained( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FOCUS_LOST:
            return FocusLost( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_TOUCH_DOWN:
        	if ( CallbackView != NULL )
        	{
        		Sound.PlaySoundEffect( guiSys, "touch_down", 0.1 );
        		return MSG_STATUS_CONSUMED;
        	}
        	return MSG_STATUS_ALIVE;
        case VRMENU_EVENT_TOUCH_UP:
        	if ( !( vrFrame.Input.buttonState & BUTTON_TOUCH_WAS_SWIPE ) && ( CallbackView != NULL ) )
        	{
                Sound.PlaySoundEffect( guiSys, "touch_up", 0.1 );
               	CallbackView->ResumeChoice( ItemNum );
        		return MSG_STATUS_CONSUMED;
        	}
            return MSG_STATUS_ALIVE;
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  ResumeMovieComponent::Frame
eMsgStatus ResumeMovieComponent::Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	UpdateColor( self );

    return MSG_STATUS_ALIVE;
}

//==============================
//  ResumeMovieComponent::FocusGained
eMsgStatus ResumeMovieComponent::FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	//LOG( "FocusGained" );

	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	HasFocus = true;
	Sound.PlaySoundEffect( guiSys, "gaze_on", 0.1 );

	self->SetHilighted( true );
	self->SetTextColor( HighlightColor );
	if ( Icon != NULL )
	{
		Icon->SetColor( HighlightColor );
	}

	return MSG_STATUS_ALIVE;
}

//==============================
//  ResumeMovieComponent::FocusLost
eMsgStatus ResumeMovieComponent::FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	//LOG( "FocusLost" );

	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	HasFocus = false;
	Sound.PlaySoundEffect( guiSys, "gaze_off", 0.1 );

	self->SetHilighted( false );
	self->SetTextColor( self->IsHilighted() ? HighlightColor : NormalColor );
	if ( Icon != NULL )
	{
		Icon->SetColor( self->IsHilighted() ? HighlightColor : NormalColor );
	}

	return MSG_STATUS_ALIVE;
}

} // namespace OculusCinema
