/************************************************************************************

Filename    :   VideoMenu.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VideoMenu.h"

#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"
#include "ActionComponents.h"
#include "FolderBrowser.h"

#include "Oculus360Videos.h"
#include "VideosMetaData.h"

namespace OVR {

const VRMenuId_t OvrVideoMenu::ID_CENTER_ROOT( 1000 );
const VRMenuId_t OvrVideoMenu::ID_BROWSER_BUTTON( 1000 + 1011 );
const VRMenuId_t OvrVideoMenu::ID_VIDEO_BUTTON( 1000 + 1012 );

char const * OvrVideoMenu::MENU_NAME = "VideoMenu";

static const Vector3f FWD( 0.0f, 0.0f, 1.0f );
static const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
static const Vector3f UP( 0.0f, 1.0f, 0.0f );
static const Vector3f DOWN( 0.0f, -1.0f, 0.0f );

static const float BUTTON_COOL_DOWN_SECONDS = 0.25f;

//==============================
// OvrVideoMenuRootComponent
// This component is attached to the root of VideoMenu 
class OvrVideoMenuRootComponent : public VRMenuComponent
{
public:
	OvrVideoMenuRootComponent( OvrVideoMenu & videoMenu )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_OPENING )
		, VideoMenu( videoMenu )
		, CurrentVideo( NULL )
	{
	}

private:
	// private assignment operator to prevent copying
	OvrVideoMenuRootComponent &	operator = ( OvrVideoMenuRootComponent & );

	virtual eMsgStatus	OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
		VRMenuObject * self, VRMenuEvent const & event )
	{
		switch ( event.EventType )
		{
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrame( guiSys, vrFrame, self, event );
		case VRMENU_EVENT_OPENING:
			return OnOpening( guiSys, vrFrame, self, event );
		default:
			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
			return MSG_STATUS_ALIVE;
		}
	}

	eMsgStatus OnOpening( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		OVR_UNUSED( vrFrame );
		OVR_UNUSED( event );

		Oculus360Videos * videos = static_cast< Oculus360Videos* >( guiSys.GetApp()->GetAppInterface() );
		CurrentVideo = (OvrVideosMetaDatum *)( videos->GetActiveVideo() );
		// If opening VideoMenu without a Video selected, bail 
		if ( CurrentVideo == NULL )
		{
			guiSys.CloseMenu( &VideoMenu, false );
		}
		LoadAttribution( self );
		return MSG_STATUS_CONSUMED;
	}

	void LoadAttribution( VRMenuObject * self )
	{
		if ( CurrentVideo )
		{
			self->SetText( CurrentVideo->Title.ToCStr() );
		}
	}

	eMsgStatus OnFrame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		OVR_UNUSED( guiSys );
		OVR_UNUSED( vrFrame );
		OVR_UNUSED( self );
		OVR_UNUSED( event );

		return MSG_STATUS_ALIVE;
	}

private:
	OvrVideoMenu &			VideoMenu;
	OvrVideosMetaDatum *	CurrentVideo;
};

//==============================
// OvrVideoMenu
OvrVideoMenu * OvrVideoMenu::Create( OvrGuiSys & guiSys, OvrMetaData & metaData, float radius )
{
	return new OvrVideoMenu( guiSys, metaData, radius );
}

OvrVideoMenu::OvrVideoMenu( OvrGuiSys & guiSys, OvrMetaData & metaData, float radius )
	: VRMenu( MENU_NAME )
	, MetaData( metaData )
	, LoadingIconHandle( 0 )
	, AttributionHandle( 0 )
	, BrowserButtonHandle( 0 )
	, VideoControlButtonHandle( 0 )
	, Radius( radius )
	, ButtonCoolDown( 0.0f )
	, OpenTime( 0.0 )
{
	// Init with empty root
	Init( guiSys, 0.0f, VRMenuFlags_t() );

	// Create Attribution info view
	Array< VRMenuObjectParms const * > parms;
	Array< VRMenuComponent* > comps;
	VRMenuId_t attributionPanelId( ID_CENTER_ROOT.Get() + 10 );

	comps.PushBack( new OvrVideoMenuRootComponent( *this ) );

	Quatf rot( DOWN, 0.0f );
	Vector3f dir( -FWD );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );

	VRMenuObjectParms attrParms( VRMENU_STATIC, comps,
		VRMenuSurfaceParms(), "Attribution Panel", panelPose, panelScale, Posef(), Vector3f( 1.0f ), fontParms, attributionPanelId,
		VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &attrParms );

	AddItems( guiSys, parms, GetRootHandle(), false );
	parms.Clear();
	comps.Clear();

	AttributionHandle = HandleForId( guiSys.GetVRMenuMgr(), attributionPanelId );
	VRMenuObject * attributionObject = guiSys.GetVRMenuMgr().ToObject( AttributionHandle );
	OVR_ASSERT( attributionObject != NULL );

	//Browser button
	float const ICON_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	Array< VRMenuSurfaceParms > surfParms;

	Posef browserButtonPose( Quatf(), UP * ICON_HEIGHT * 2.0f );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_BROWSER_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent( ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"apk:///assets/nav_home_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"apk:///assets/nav_home_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	VRMenuObjectParms browserButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		browserButtonPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms,
		ID_BROWSER_BUTTON, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &browserButtonParms );

	AddItems( guiSys, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();
	surfParms.Clear();

	BrowserButtonHandle = attributionObject->ChildHandleForId( guiSys.GetVRMenuMgr(), ID_BROWSER_BUTTON );
	VRMenuObject * browserButtonObject = guiSys.GetVRMenuMgr().ToObject( BrowserButtonHandle );
	OVR_ASSERT( browserButtonObject != NULL );
	OVR_UNUSED( browserButtonObject );

	//Video control button 
	Posef videoButtonPose( Quatf(), DOWN * ICON_HEIGHT * 2.0f );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_VIDEO_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent( ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"apk:///assets/nav_restart_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"apk:///assets/nav_restart_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	VRMenuObjectParms controlButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		videoButtonPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms,
		ID_VIDEO_BUTTON, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &controlButtonParms );

	AddItems( guiSys, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();

	VideoControlButtonHandle = attributionObject->ChildHandleForId( guiSys.GetVRMenuMgr(), ID_VIDEO_BUTTON );
	VRMenuObject * controlButtonObject = guiSys.GetVRMenuMgr().ToObject( VideoControlButtonHandle );
	OVR_ASSERT( controlButtonObject != NULL );
	OVR_UNUSED( controlButtonObject );

}

OvrVideoMenu::~OvrVideoMenu()
{

}

void OvrVideoMenu::Open_Impl( OvrGuiSys & guiSys )
{
	OVR_UNUSED( guiSys );

	ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

	OpenTime = vrapi_GetTimeInSeconds();
}

void OvrVideoMenu::Frame_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );

	if ( ButtonCoolDown > 0.0f )
	{
		ButtonCoolDown -= vrFrame.DeltaSeconds;
	}
}

void OvrVideoMenu::OnItemEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, 
		VRMenuId_t const itemId, VRMenuEvent const & event )
{
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( event );

	const double now = vrapi_GetTimeInSeconds();
	if ( ButtonCoolDown <= 0.0f && (now - OpenTime > 0.5))
	{
		ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

		Oculus360Videos * videos = static_cast< Oculus360Videos* >( guiSys.GetApp()->GetAppInterface() );
		if ( itemId.Get() == ID_BROWSER_BUTTON.Get() )
		{
			videos->SetMenuState( Oculus360Videos::MENU_BROWSER );
		}
		else if ( itemId.Get( ) == ID_VIDEO_BUTTON.Get( ) )
		{
			videos->SeekTo( 0 );
		}
	}
}

}
