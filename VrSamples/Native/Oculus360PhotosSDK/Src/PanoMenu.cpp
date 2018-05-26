/************************************************************************************

Filename    :   PanoMenu.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "PanoMenu.h"

#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"
#include "ActionComponents.h"
#include "AnimComponents.h"
#include "FolderBrowser.h"

#include "Oculus360Photos.h"
#include "PhotosMetaData.h"

namespace OVR {

const VRMenuId_t OvrPanoMenu::ID_CENTER_ROOT( 1000 );
const VRMenuId_t OvrPanoMenu::ID_BROWSER_BUTTON( 1000 + 1011 );
const VRMenuId_t OvrPanoMenu::ID_FAVORITES_BUTTON( 1000 + 1012 );

char const * OvrPanoMenu::MENU_NAME = "PanoMenu";

static const Vector3f FWD( 0.0f, 0.0f, 1.0f );
static const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
static const Vector3f UP( 0.0f, 1.0f, 0.0f );
static const Vector3f DOWN( 0.0f, -1.0f, 0.0f );

static const float BUTTON_COOL_DOWN_SECONDS = 0.25f;

//==============================
// OvrPanoMenuRootComponent
// This component is attached to the root of PanoMenu 
class OvrPanoMenuRootComponent : public VRMenuComponent
{
public:
	OvrPanoMenuRootComponent( OvrPanoMenu & panoMenu )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_OPENING )
		, PanoMenu( panoMenu )
		, CurrentPano( NULL )
	{
	}

private:
	virtual eMsgStatus OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
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

		Oculus360Photos * photos = static_cast< Oculus360Photos* >( guiSys.GetApp()->GetAppInterface() );
		CurrentPano = photos->GetActivePano();
		// If opening PanoMenu without a Pano selected, bail 
		if ( CurrentPano == NULL )
		{
			guiSys.CloseMenu( &PanoMenu, false );
		}
		LoadAttribution( self );
		return MSG_STATUS_CONSUMED;
	}

	void LoadAttribution( VRMenuObject * self )
	{
		if ( CurrentPano )
		{
			String attribution = CurrentPano->Title + "\n";
			attribution += "by ";
			attribution += CurrentPano->Author;
			self->SetText( attribution.ToCStr() );
		}
	}

	eMsgStatus OnFrame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		OVR_UNUSED( vrFrame );
		OVR_UNUSED( event );

		Vector4f selfColor = self->GetColor( );
		Vector4f selfTextColor = self->GetTextColor();
		
		VRMenuObjectFlags_t attributionFlags = self->GetFlags();

		Oculus360Photos * photos = static_cast< Oculus360Photos* >( guiSys.GetApp()->GetAppInterface() );
		const float fadeInAlpha = photos->GetFadeLevel();
		const float fadeOutAlpha = PanoMenu.GetFadeAlpha();
		switch ( photos->GetCurrentState() )
		{
		case Oculus360Photos::MENU_PANO_LOADING:
			OVR_ASSERT( photos );
			if ( CurrentPano != photos->GetActivePano() )
			{
				CurrentPano = photos->GetActivePano();
				LoadAttribution( self );
			}
			// Hide attribution 
			attributionFlags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			break;
		case Oculus360Photos::MENU_PANO_REOPEN_FADEIN:
		case Oculus360Photos::MENU_PANO_FADEIN:		
			// Show attribution 
			attributionFlags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			// Fade in burger
			selfColor.w = fadeInAlpha;
			selfTextColor.w = fadeInAlpha;		
			break;
		case Oculus360Photos::MENU_PANO_FULLY_VISIBLE:
			// Show attribution 
			attributionFlags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			break;
		case Oculus360Photos::MENU_PANO_FADEOUT:
			// Fade out burger
			selfColor.w = fadeOutAlpha;
			selfTextColor.w = fadeOutAlpha;
			if ( fadeOutAlpha == 0.0f )
			{
				guiSys.GetGazeCursor().HideCursor();
				guiSys.CloseMenu( &PanoMenu, false );
			}		
			break;
		default:
			// Hide attribution 
			attributionFlags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			break;
		}

		self->SetFlags( attributionFlags );
		self->SetTextColor( selfTextColor );
		self->SetColor( selfColor );

		return MSG_STATUS_ALIVE;
	}

private:
	// private assignment operator to prevent copying
	OvrPanoMenuRootComponent &	operator = ( OvrPanoMenuRootComponent & );

private:
	OvrPanoMenu &				PanoMenu;
	const OvrPhotosMetaDatum *	CurrentPano;
};

//==============================
// OvrPanoMenu
OvrPanoMenu * OvrPanoMenu::Create( OvrGuiSys & guiSys, OvrMetaData & metaData, float fadeOutTime, float radius )
{
	return new OvrPanoMenu( guiSys, metaData, fadeOutTime, radius );
}

OvrPanoMenu::OvrPanoMenu( OvrGuiSys & guiSys, OvrMetaData & metaData, float fadeOutTime, float radius )
	: VRMenu( MENU_NAME )
	, MetaData( metaData )
	, LoadingIconHandle( 0 )
	, AttributionHandle( 0 )
	, BrowserButtonHandle( 0 )
	, SwipeLeftIndicatorHandle( 0 )
	, SwipeRightIndicatorHandle( 0 )
	, Fader( 1.0f )
	, FadeOutTime( fadeOutTime )
	, currentFadeRate( 0.0f )
	, Radius( radius )
	, ButtonCoolDown( 0.0f )
{	
	currentFadeRate = 1.0f / FadeOutTime;

	// Init with empty root
	Init( guiSys, 0.0f, VRMenuFlags_t() );
	
	// Create Attribution info view
	Array< VRMenuObjectParms const * > parms;
	Array< VRMenuComponent* > comps;
	VRMenuId_t attributionPanelId( ID_CENTER_ROOT.Get() + 10 );

	comps.PushBack( new OvrPanoMenuRootComponent( *this ) );

	Quatf rot( DOWN, 0.0f );
	Vector3f dir( -FWD );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	//const Posef textPose( Quatf(), Vector3f( 0.0f, 0.0f, 0.0f ) );

	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );
	
	VRMenuObjectParms attrParms( VRMENU_STATIC, comps,
		VRMenuSurfaceParms(), "Attribution Panel", panelPose, panelScale, Posef(), Vector3f( 1.0f ), fontParms, attributionPanelId,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &attrParms );

	AddItems( guiSys, parms, GetRootHandle(), false );
	parms.Clear();
	comps.Clear();

	AttributionHandle = HandleForId( guiSys.GetVRMenuMgr(), attributionPanelId );
	VRMenuObject * attributionObject = guiSys.GetVRMenuMgr().ToObject( AttributionHandle );
	OVR_ASSERT( attributionObject != NULL );

	//Browser button
	float const ICON_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
    float const ICON_WIDTH = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	Array< VRMenuSurfaceParms > surfParms;

	Posef browserButtonPose( Quatf(), RIGHT * ICON_WIDTH + DOWN * ICON_HEIGHT * 2.0 );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_BROWSER_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent( ) );
	surfParms.PushBack( VRMenuSurfaceParms ( "browser",
		"apk:///assets/nav_home_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"apk:///assets/nav_home_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	VRMenuObjectParms browserButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		browserButtonPose, Vector3f( 1.0f ), Posef( ), Vector3f( 1.0f ), fontParms,
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

	//Favorites button
	Posef favoritesButtonPose( Quatf(), -RIGHT * ICON_WIDTH + DOWN * ICON_HEIGHT * 2.0 );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_FAVORITES_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent() );
	
	surfParms.PushBack( VRMenuSurfaceParms( "favorites_off",
		"apk:///assets/nav_star_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );

	surfParms.PushBack( VRMenuSurfaceParms( "favorites_on",
		"apk:///assets/nav_star_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );

	surfParms.PushBack( VRMenuSurfaceParms( "favorites_active_off",
		"apk:///assets/nav_star_active_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );

	surfParms.PushBack( VRMenuSurfaceParms( "favorites_active_on",
		"apk:///assets/nav_star_active_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );

	VRMenuObjectParms favoritesButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		favoritesButtonPose, Vector3f( 1.0f ), Posef( ), Vector3f( 1.0f ), fontParms,
		ID_FAVORITES_BUTTON, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &favoritesButtonParms );

	AddItems( guiSys, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();

	FavoritesButtonHandle = attributionObject->ChildHandleForId( guiSys.GetVRMenuMgr(), ID_FAVORITES_BUTTON );
	VRMenuObject * favoritesButtonObject = guiSys.GetVRMenuMgr().ToObject( FavoritesButtonHandle );
	OVR_ASSERT( favoritesButtonObject != NULL );
	OVR_UNUSED( favoritesButtonObject );

	// Swipe icons
	const int numFrames = 10;
	const int numTrails = 3;
	const int numChildren = 5;
	const float swipeFPS = 3.0f;
	const float factor = 1.0f / 8.0f;

	// Right container
	VRMenuId_t swipeRightId( ID_CENTER_ROOT.Get() + 401 );
	Quatf rotRight( DOWN, ( MATH_FLOAT_TWOPI * factor ) );
	Vector3f rightDir( -FWD * rotRight );	
	comps.PushBack( new OvrTrailsAnimComponent( swipeFPS, true, numFrames, numTrails, numTrails ) );
	VRMenuObjectParms swipeRightRoot( VRMENU_CONTAINER, comps, VRMenuSurfaceParms( ), "",
		Posef( rotRight, rightDir * Radius ), Vector3f( 1.0f ), Posef( ), Vector3f( 1.0f ), fontParms, swipeRightId,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &swipeRightRoot );
	AddItems( guiSys, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();

	SwipeRightIndicatorHandle = attributionObject->ChildHandleForId( guiSys.GetVRMenuMgr(), swipeRightId );
	VRMenuObject * swipeRightRootObject = guiSys.GetVRMenuMgr().ToObject( SwipeRightIndicatorHandle );
	OVR_ASSERT( swipeRightRootObject != NULL );

	// Left container
	VRMenuId_t swipeLeftId( ID_CENTER_ROOT.Get( ) + 402 );
	Quatf rotLeft( DOWN, ( MATH_FLOAT_TWOPI * -factor ) );
	Vector3f leftDir( -FWD * rotLeft );
	comps.PushBack( new OvrTrailsAnimComponent( swipeFPS, true, numFrames, numTrails, numTrails ) );
	VRMenuObjectParms swipeLeftRoot( VRMENU_CONTAINER, comps, VRMenuSurfaceParms( ), "",
		Posef( rotLeft, leftDir * Radius ), Vector3f( 1.0f ), Posef( ), Vector3f( 1.0f ), fontParms, swipeLeftId,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &swipeLeftRoot );
	AddItems( guiSys, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();

	SwipeLeftIndicatorHandle = attributionObject->ChildHandleForId( guiSys.GetVRMenuMgr(), swipeLeftId );
	VRMenuObject * swipeLeftRootObject = guiSys.GetVRMenuMgr().ToObject( SwipeLeftIndicatorHandle );
	OVR_ASSERT( swipeLeftRootObject != NULL );
	
	// Arrow frame children
	const char * swipeRightIcon = "apk:///assets/nav_arrow_right.png";
	const char * swipeLeftIcon = "apk:///assets/nav_arrow_left.png";

	VRMenuSurfaceParms rightIndicatorSurfaceParms( "swipeRightSurface",
		swipeRightIcon, SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );

	VRMenuSurfaceParms leftIndicatorSurfaceParms( "swipeLeftSurface",
		swipeLeftIcon, SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX );
	
	const float surfaceWidth = 25.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;

	for ( int i = 0; i < numChildren; ++i )
	{
		const float childIndexf = static_cast<float>( i );

 		// right frame
		const Vector3f rightPos = ( RIGHT * surfaceWidth * childIndexf ) - ( FWD * childIndexf * 0.1f );
		VRMenuObjectParms swipeRightFrame( VRMENU_STATIC, Array< VRMenuComponent* >(), rightIndicatorSurfaceParms, "",
			Posef( Quatf( ), rightPos ), Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms, VRMenuId_t(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &swipeRightFrame );
		AddItems( guiSys, parms, SwipeRightIndicatorHandle, false );
		parms.Clear();

		// left frame
		const Vector3f leftPos = ( (-RIGHT) * surfaceWidth * childIndexf ) - ( FWD * childIndexf * 0.1f );
		VRMenuObjectParms swipeLeftFrame( VRMENU_STATIC, Array< VRMenuComponent* >(), leftIndicatorSurfaceParms, "",
			Posef( Quatf( ), leftPos ), Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms, VRMenuId_t(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( &swipeLeftFrame );
		AddItems( guiSys, parms, SwipeLeftIndicatorHandle, false );
		parms.Clear();
	}

	if ( OvrTrailsAnimComponent * animRightComp = swipeRightRootObject->GetComponentByTypeName< OvrTrailsAnimComponent >() )
	{
		animRightComp->Play( );
	}
	
	if ( OvrTrailsAnimComponent * animLeftComp = swipeLeftRootObject->GetComponentByTypeName< OvrTrailsAnimComponent >() )
	{
		animLeftComp->Play( );
	}
}

OvrPanoMenu::~OvrPanoMenu()
{

}

void OvrPanoMenu::Frame_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame )
{
	Fader.Update( currentFadeRate, vrFrame.DeltaSeconds );
	
	if ( ButtonCoolDown > 0.0f )
	{
		ButtonCoolDown -= vrFrame.DeltaSeconds;
	}

	if ( IsOpen() )
	{
		OvrGazeCursor & gazeCursor = guiSys.GetGazeCursor();
		if ( GazingAtMenu( guiSys.GetApp()->GetLastViewMatrix() ) )
		{
			gazeCursor.ShowCursor();
		}
		else
		{
			gazeCursor.HideCursor();
		}
	}
}

void OvrPanoMenu::OnItemEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, 
			VRMenuId_t const itemId, VRMenuEvent const & event )
{
	OVR_UNUSED( event );
	OVR_UNUSED( vrFrame );

	Oculus360Photos * photos = static_cast< Oculus360Photos* >( guiSys.GetApp()->GetAppInterface() );

	if ( ButtonCoolDown <= 0.0f )
	{
		ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

		if ( photos->AllowPanoInput() )
		{
			if ( itemId.Get() == ID_BROWSER_BUTTON.Get() )
			{
				photos->SetMenuState( Oculus360Photos::MENU_BROWSER );
			}
			else if ( itemId.Get() == ID_FAVORITES_BUTTON.Get() )
			{
				const TagAction actionTaken = static_cast< TagAction >( photos->ToggleCurrentAsFavorite() );
				const bool forceShowSwipeIndicators = ( actionTaken == TAG_REMOVED ) && ( photos->GetNumPanosInActiveCategory( guiSys ) == 1 );
				UpdateButtonsState( guiSys, photos->GetActivePano( ), forceShowSwipeIndicators );
			}
		}
	}
}

void OvrPanoMenu::StartFadeOut()
{
	Fader.Reset( );
	Fader.StartFadeOut();
}

float OvrPanoMenu::GetFadeAlpha() const
{
	return Fader.GetFinalAlpha();
}

bool OvrPanoMenu::GazingAtMenu( const Matrix4f & viewMatrix ) const
{
	Vector3f viewForward( viewMatrix.M[ 2 ][ 0 ], viewMatrix.M[ 2 ][ 1 ], viewMatrix.M[ 2 ][ 2 ] );
	viewForward.Normalize();

	const float angle = 0.35f;
	const float cosAngle = cos( angle );
	const float dot = viewForward.Dot( FWD * GetMenuPose().Rotation );
	if ( dot >= cosAngle )
	{
		return true;
	}

	return false;
}

void OvrPanoMenu::UpdateButtonsState( OvrGuiSys & guiSys, const OvrMetaDatum * const ActivePano, bool showSwipeOverride /*= false*/ )
{
	// Reset button time
	ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

	// Update favo
	bool isFavorite = false;

	for ( int i = 0; i < ActivePano->Tags.GetSizeI( ); ++i )
	{
		if ( ActivePano->Tags[ i ] == "Favorites" )
		{
			isFavorite = true;
			break;
		}
	}

	VRMenuObject * favoritesButtonObject = guiSys.GetVRMenuMgr().ToObject( FavoritesButtonHandle );
	OVR_ASSERT( favoritesButtonObject != NULL );

	if ( OvrSurfaceToggleComponent * favToggleComp = favoritesButtonObject->GetComponentByTypeName<OvrSurfaceToggleComponent>() )
	{
		const int fav = isFavorite ? 2 : 0;
		favToggleComp->SetGroupIndex( fav );
	}
	
	VRMenuObject * swipeRight = guiSys.GetVRMenuMgr().ToObject( SwipeRightIndicatorHandle );
	OVR_ASSERT( swipeRight != NULL );

	VRMenuObject * swipeLeft = guiSys.GetVRMenuMgr().ToObject( SwipeLeftIndicatorHandle );
	OVR_ASSERT( swipeLeft != NULL );

	Oculus360Photos * photos = static_cast< Oculus360Photos* >( guiSys.GetApp()->GetAppInterface() );
	const bool showSwipeIndicators = showSwipeOverride || ( photos->GetNumPanosInActiveCategory( guiSys ) > 1 );

	VRMenuObjectFlags_t flagsLeft = swipeRight->GetFlags( );
	VRMenuObjectFlags_t flagsRight = swipeRight->GetFlags( );

	if ( showSwipeIndicators )
	{
		flagsLeft &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		flagsRight &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
	}
	else
	{
		flagsLeft |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
		flagsRight |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
	}

	swipeLeft->SetFlags( flagsLeft );
	swipeRight->SetFlags( flagsRight );
}

}
