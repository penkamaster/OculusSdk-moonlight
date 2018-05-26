/************************************************************************************

Filename    :   UIButton.cpp
Content     :
Created     :	1/23/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "UI/UIButton.h"
#include "UI/UIMenu.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "App.h"

namespace OVR {

UIButton::UIButton( OvrGuiSys &guiSys ) :
	UIObject( guiSys ),
	ButtonComponent( NULL ),
	NormalTexture(),
	HoverTexture(),
	PressedTexture(),
	NormalColor( 1.0f ),
	HoverColor( 1.0f ),
	PressedColor( 1.0f ),
	OnClickFunction( NULL ),
	OnClickObject( NULL ),
	OnFocusGainedFunction( NULL ),
	OnFocusGainedObject( NULL ),
	OnFocusLostFunction( NULL ),
	OnFocusLostObject( NULL )
{
}

UIButton::~UIButton()
{
}

void UIButton::AddToMenu( UIMenu *menu, UIObject *parent )
{
	const Posef pose( Quatf( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f ), Vector3f( 0.0f, 0.0f, 0.0f ) );

	Vector3f defaultScale( 1.0f );
	VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f );
	
	VRMenuObjectParms parms( VRMENU_BUTTON, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
			"", pose, defaultScale, fontParms, menu->AllocId(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	AddToMenuWithParms( menu, parent, parms );

	VRMenuObject * object = GetMenuObject();
	OVR_ASSERT( object );

	ButtonComponent = new UIButtonComponent( *this );
	object->AddComponent( ButtonComponent );
}

void UIButton::SetButtonImages( const UITexture &normal, const UITexture &hover, const UITexture &pressed, const UIRectf &border )
{
	NormalTexture 	= &normal;
	HoverTexture 	= &hover;
	PressedTexture 	= &pressed;

	SetBorder( border );

	UpdateButtonState();
}

void UIButton::SetButtonColors( const Vector4f &normal, const Vector4f &hover, const Vector4f &pressed )
{
	NormalColor 	= normal;
	HoverColor 		= hover;
	PressedColor	= pressed;

	UpdateButtonState();
}

void UIButton::SetAsToggleButton( const UITexture & pressedHoverTexture, const Vector4f & pressedHoverColor )
{
	ToggleButton = true;
	PressedHoverColor = pressedHoverColor;
	PressedHoverTexture = &pressedHoverTexture;
}

void UIButton::SetOnClick( void ( *callback )( UIButton *, void * ), void *object )
{
	OnClickFunction = callback;
	OnClickObject = object;
}

void UIButton::SetText( const char * text )
{
	VRMenuObject * object = GetMenuObject();
	OVR_ASSERT( object );
	object->SetText( text );
}

void UIButton::SetOnFocusGained( void( *callback )( UIButton *, void * ), void *object )
{
	OnFocusGainedFunction = callback;
	OnFocusGainedObject = object;
}

void UIButton::SetOnFocusLost( void( *callback )( UIButton *, void * ), void *object )
{
	OnFocusLostFunction = callback;
	OnFocusLostObject = object;
}

void UIButton::OnClick()
{
	if ( OnClickFunction != NULL )
	{
		( *OnClickFunction )( this, OnClickObject );
	}
}

void UIButton::FocusGained()
{
	if ( OnFocusGainedFunction != NULL )
	{
		( *OnFocusGainedFunction )( this, OnFocusGainedObject );
	}
}

void UIButton::FocusLost()
{
	if ( OnFocusLostFunction != NULL )
	{
		( *OnFocusLostFunction )( this, OnFocusLostObject );
	}
}

//==============================
//  UIButton::UpdateButtonState
void UIButton::UpdateButtonState()
{
	const UIRectf border = GetBorder();
	const Vector2f dims = GetDimensions();

	if ( ToggleButton && PressedHoverTexture && ButtonComponent->IsPressed() && IsHilighted() )
	{
		SetImage( 0, SURFACE_TEXTURE_DIFFUSE, *PressedHoverTexture, dims.x, dims.y, border );
		SetColor( PressedHoverColor );
	}
	else if ( ButtonComponent->IsPressed() )
	{
		SetImage( 0, SURFACE_TEXTURE_DIFFUSE, *PressedTexture, dims.x, dims.y, border );
		SetColor( PressedColor );
	}
	else if ( IsHilighted() )
	{
		SetImage( 0, SURFACE_TEXTURE_DIFFUSE, *HoverTexture, dims.x, dims.y, border );
		SetColor( HoverColor );
	}
	else
	{
		SetImage( 0, SURFACE_TEXTURE_DIFFUSE, *NormalTexture, dims.x, dims.y, border );
		SetColor( NormalColor );
	}
}

void UIButton::SetTouchDownSnd( const char * touchDownSnd )
{
	if ( ButtonComponent )
	{
		ButtonComponent->SetTouchDownSnd( touchDownSnd );
	}
}

void UIButton::SetTouchUpSnd( const char * touchUpSnd )
{
	if ( ButtonComponent )
	{
		ButtonComponent->SetTouchUpSnd( touchUpSnd );
	}
}

/*************************************************************************************/

//==============================
//  UIButtonComponent::
UIButtonComponent::UIButtonComponent( UIButton &button ) :
    VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_TOUCH_DOWN ) |
            VRMENU_EVENT_TOUCH_UP |
            VRMENU_EVENT_FOCUS_GAINED |
            VRMENU_EVENT_FOCUS_LOST ),
    Button( button ),
	TouchDown( false )

{
}

//==============================
//  UIButtonComponent::OnEvent_Impl
eMsgStatus UIButtonComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    switch( event.EventType )
    {
        case VRMENU_EVENT_FOCUS_GAINED:
            return FocusGained( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_FOCUS_LOST:
            return FocusLost( guiSys, vrFrame, self, event );
        case VRMENU_EVENT_TOUCH_DOWN:
			if ( Button.ToggleButton )
			{
				TouchDown = !TouchDown;
				Button.OnClick();
			}
			else
			{
				TouchDown = true;
				if ( Button.ActionType == UIButton::eButtonActionType::ClickOnDown )
				{
					Button.OnClick();
				}
			}
        	Button.UpdateButtonState();
			DownSoundLimiter.PlaySoundEffect( guiSys, TouchDownSnd.ToCStr(), 0.1 );
            return MSG_STATUS_ALIVE;
		case VRMENU_EVENT_TOUCH_UP:
			if ( !Button.ToggleButton )
			{
				TouchDown = false;
			}
        	Button.UpdateButtonState();
			if ( !Button.ToggleButton && Button.ActionType == UIButton::eButtonActionType::ClickOnUp )
			{
				Button.OnClick();
			}
			UpSoundLimiter.PlaySoundEffect( guiSys, TouchUpSnd.ToCStr(), 0.1 );
            return MSG_STATUS_ALIVE;
        default:
            OVR_ASSERT( !"Event flags mismatch!" );
            return MSG_STATUS_ALIVE;
    }
}

//==============================
//  UIButtonComponent::FocusGained
eMsgStatus UIButtonComponent::FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    // set the hilight flag
	Button.FocusGained();
    self->SetHilighted( true );
    Button.UpdateButtonState();
	GazeOverSoundLimiter.PlaySoundEffect( guiSys, "gaze_on", 0.1 );
    return MSG_STATUS_ALIVE;
}

//==============================
//  UIButtonComponent::FocusLost
eMsgStatus UIButtonComponent::FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
    // clear the hilight flag
	Button.FocusLost();
    self->SetHilighted( false );
	if ( !Button.ToggleButton )
		TouchDown = false;
    Button.UpdateButtonState();
    GazeOverSoundLimiter.PlaySoundEffect( guiSys, "gaze_off", 0.1 );
    return MSG_STATUS_ALIVE;
}

void UIButtonComponent::SetTouchDownSnd( const char * touchDownSnd )
{
	TouchDownSnd = touchDownSnd;
}

void UIButtonComponent::SetTouchUpSnd( const char * touchUpSnd )
{
	TouchUpSnd = touchUpSnd;
}

} // namespace OVR
