/************************************************************************************

Filename    :   MovieSelectionComponent.cpp
Content     :   Menu component for the movie selection menu.
Created     :   October 8, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "MovieSelectionComponent.h"
#include "OVR_Input.h"
#include "SelectionView.h"
#include "GuiSys.h"

namespace OculusCinema {

//==============================
//  MovieSelectionComponent::
MovieSelectionComponent::MovieSelectionComponent( SelectionView *view ) :
	VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) |
		VRMENU_EVENT_TOUCH_DOWN |
		VRMENU_EVENT_TOUCH_UP |
        VRMENU_EVENT_FOCUS_GAINED |
        VRMENU_EVENT_FOCUS_LOST ),
    CallbackView( view )

{
}

//==============================
//  MovieSelectionComponent::OnEvent_Impl
eMsgStatus MovieSelectionComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
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
       		Sound.PlaySoundEffect( guiSys, "touch_down", 0.1 );
       		return MSG_STATUS_CONSUMED;
        case VRMENU_EVENT_TOUCH_UP:
        	if ( !( vrFrame.Input.buttonState & BUTTON_TOUCH_WAS_SWIPE ) )
			{
        		Sound.PlaySoundEffect( guiSys, "touch_up", 0.1 );
        		CallbackView->Select();
        		return MSG_STATUS_CONSUMED;
        	}
            return MSG_STATUS_ALIVE;
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  MovieSelectionComponent::Frame
eMsgStatus MovieSelectionComponent::Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	CallbackView->SelectionHighlighted( self->IsHilighted() );

#if 0
	if ( self->IsHilighted() )
	{
		if ( vrFrame.Input.buttonPressed & BUTTON_A )
		{
			Sound.PlaySoundEffect( guiSys, "touch_down", 0.1 );
		}
		if ( vrFrame.Input.buttonReleased & BUTTON_A )
		{
			Sound.PlaySoundEffect( guiSys, "touch_up", 0.1 );
			CallbackView->SelectMovie();
		}
	}
#endif

    return MSG_STATUS_ALIVE;
}

//==============================
//  MovieSelectionComponent::FocusGained
eMsgStatus MovieSelectionComponent::FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	LOG( "FocusGained" );

	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

    // set the hilight flag
    self->SetHilighted( true );
    CallbackView->SelectionHighlighted( true );

    Sound.PlaySoundEffect( guiSys, "gaze_on", 0.1 );
	
    return MSG_STATUS_ALIVE;
}

//==============================
//  MovieSelectionComponent::FocusLost
eMsgStatus MovieSelectionComponent::FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	LOG( "FocusLost" );

	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

    // clear the hilight flag
    self->SetHilighted( false );
    CallbackView->SelectionHighlighted( false );

   	Sound.PlaySoundEffect( guiSys, "gaze_off", 0.1 );

    return MSG_STATUS_ALIVE;
}

} // namespace OculusCinema
