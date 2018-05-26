/************************************************************************************

Filename    :   TheaterSelectionComponent.cpp
Content     :   Menu component for the movie theater selection menu.
Created     :   August 15, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "TheaterSelectionComponent.h"
#include "TheaterSelectionView.h"
#include "OVR_Input.h"

namespace OculusCinema {

//==============================
//  TheaterSelectionComponent::
TheaterSelectionComponent::TheaterSelectionComponent( TheaterSelectionView *view ) :
		CarouselItemComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) |
            VRMENU_EVENT_TOUCH_UP |
            VRMENU_EVENT_FOCUS_GAINED |
            VRMENU_EVENT_FOCUS_LOST |
            VRMENU_EVENT_FRAME_UPDATE ),
	CallbackView( view ),
	HilightFader( 0.0f ),
	StartFadeInTime( 0.0 ),
	StartFadeOutTime( 0.0 ),
	HilightScale( 1.05f ),
	FadeDuration( 0.25f )
{
}

//==============================
//  TheaterSelectionComponent::SetItem
void TheaterSelectionComponent::SetItem( VRMenuObject * self, const CarouselItem * item, const PanelPose &pose )
{
	self->SetLocalPosition( pose.Position );
	self->SetLocalRotation( pose.Orientation );

	if ( item != NULL )
	{
		self->SetText( item->Name.ToCStr() );
		self->SetSurfaceTexture( 0, 0, SURFACE_TEXTURE_DIFFUSE,
			item->Texture, item->TextureWidth, item->TextureHeight );
		self->SetColor( pose.Color );
		self->SetTextColor( pose.Color );
	}
	else
	{
		Vector4f color( 0.0f );
		self->SetColor( color );
		self->SetTextColor( color );
	}
}

//==============================
//  TheaterSelectionComponent::OnEvent_Impl
eMsgStatus TheaterSelectionComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    switch( event.EventType )
    {
        case VRMENU_EVENT_FOCUS_GAINED:
            return FocusGained( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FOCUS_LOST:
            return FocusLost( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FRAME_UPDATE:
            return Frame( guiSys, vrFrame, self, event );
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
                CallbackView->SelectPressed();
        		return MSG_STATUS_CONSUMED;
        	}
            return MSG_STATUS_ALIVE;
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  TheaterSelectionComponent::FocusGained
eMsgStatus TheaterSelectionComponent::FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

    // set the hilight flag
    self->SetHilighted( true );

    StartFadeOutTime = -1.0;
    StartFadeInTime = vrapi_GetTimeInSeconds();

    Sound.PlaySoundEffect( guiSys, "gaze_on", 0.1 );

    return MSG_STATUS_ALIVE;
}

//==============================
//  TheaterSelectionComponent::FocusLost
eMsgStatus TheaterSelectionComponent::FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

    // clear the hilight flag
    self->SetHilighted( false );

    StartFadeInTime = -1.0;
    StartFadeOutTime = vrapi_GetTimeInSeconds();

    Sound.PlaySoundEffect( guiSys, "gaze_off", 0.1 );

    return MSG_STATUS_ALIVE;
}

//==============================
//  TheaterSelectionComponent::Frame
eMsgStatus TheaterSelectionComponent::Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( event );

    double t = vrapi_GetTimeInSeconds();
    if ( StartFadeInTime >= 0.0f && t >= StartFadeInTime )
    {
        HilightFader.StartFadeIn();
        StartFadeInTime = -1.0f;
    }
    else if ( StartFadeOutTime >= 0.0f && t > StartFadeOutTime )
    {
        HilightFader.StartFadeOut();
        StartFadeOutTime = -1.0f;
    }

    float const fadeRate = 1.0f / FadeDuration;
    HilightFader.Update( fadeRate, vrFrame.DeltaSeconds );

    float const hilightAlpha = HilightFader.GetFinalAlpha();

    float const scale = ( ( HilightScale - 1.0f ) * hilightAlpha ) + 1.0f;
    self->SetHilightScale( scale );


#if 0
	if ( ( CallbackView != NULL ) && self->IsHilighted() && ( vrFrame.Input.buttonReleased & BUTTON_A ) )
	{
		if ( vrFrame.Input.buttonPressed & BUTTON_A )
		{
			Sound.PlaySoundEffect( guiSys, "touch_down", 0.1 );
		}
		if ( vrFrame.Input.buttonReleased & BUTTON_A )
		{
			Sound.PlaySoundEffect( guiSys, "touch_up", 0.1 );
			CallbackView->SelectPressed();
		}
	}
#endif

    return MSG_STATUS_ALIVE;
}

} // namespace OculusCinema
