/************************************************************************************

Filename    :   PcSelectionView.cpp
Content     :
Created     :	6/19/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include <android/keycodes.h>
#include "OVR_Input.h"
#include "Kernel/OVR_String_Utils.h"
#include "App.h"
#include "PcSelectionView.h"
#include "CinemaApp.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "PcCategoryComponent.h"
#include "MoviePosterComponent.h"
#include "MovieSelectionComponent.h"
#include "CarouselSwipeHintComponent.h"
#include "PackageFiles.h"
#include "CinemaStrings.h"
#include "Native.h"

namespace OculusCinema {

static const int PosterWidth = 228;
static const int PosterHeight = 344;

static const Vector3f PosterScale( 4.4859375f * 0.98f );

static const double TimerTotalTime = 10;

static const int NumSwipeTrails = 3;


//=======================================================================================

PcSelectionView::PcSelectionView( CinemaApp &cinema ) :
	SelectionView( "PcSelectionView" ),
	Cinema( cinema ),
	SelectionTexture(),
	Is3DIconTexture(),
	ShadowTexture(),
	BorderTexture(),
	SwipeIconLeftTexture(),
	SwipeIconRightTexture(),
	ResumeIconTexture(),
	ErrorIconTexture(),
	SDCardTexture(),
	Menu( NULL ),
	CenterRoot( NULL ),
	ErrorMessage( NULL ),
	SDCardMessage( NULL ),
	PlainErrorMessage( NULL ),
	ErrorMessageClicked( false ),
	MovieRoot( NULL ),
	CategoryRoot( NULL ),
	TitleRoot( NULL ),
	MovieTitle( NULL ),
	SelectionFrame( NULL ),
	CenterPoster( NULL ),
	CenterIndex( 0 ),
	CenterPosition(),
	LeftSwipes(),
	RightSwipes(),
	ResumeIcon( NULL ),
	TimerIcon( NULL ),
	TimerText( NULL ),
	TimerStartTime( 0 ),
	TimerValue( 0 ),
	ShowTimer( false ),
	MoveScreenLabel( NULL ),
	MoveScreenAlpha(),
	SelectionFader(),
	MovieBrowser( NULL ),
	MoviePanelPositions(),
	MoviePosterComponents(),
	Categories(),
	CurrentCategory( CATEGORY_LIMELIGHT ),
	MovieList(),
	MoviesIndex( 0 ),
	LastMovieDisplayed( NULL ),
	RepositionScreen( false ),
	HadSelection( false ),
	newPCWidth( 0 ),
	newPCHeight( 0 ),
	newPCTex(),
	newPCMenu( NULL ),
	bgTintTexture(),
	newPCbg( Cinema.GetGuiSys() ),
	newPCIPLabel( Cinema.GetGuiSys() ),
	newPCIPButtons(),
	IPoctets(),
	currentOctet(0),
	IPString("0_.0.0.0")


{
	// This is called at library load time, so the system is not initialized
	// properly yet.
}

PcSelectionView::~PcSelectionView()
{
	DeletePointerArray( MovieBrowserItems );
}

void PcSelectionView::OneTimeInit( const char * launchIntent )
{
	OVR_UNUSED( launchIntent );

	LOG( "PcSelectionView::OneTimeInit" );

	const double start = SystemClock::GetTimeInSeconds();

	CreateMenu( Cinema.GetGuiSys() );

	SetCategory( CATEGORY_LIMELIGHT );

    Native::InitPcSelector( Cinema.app );

	LOG( "PcSelectionView::OneTimeInit %3.1f seconds",  SystemClock::GetTimeInSeconds() - start );
}

void PcSelectionView::OneTimeShutdown()
{
	LOG( "PcSelectionView::OneTimeShutdown" );
}

void PcSelectionView::OnOpen()
{
	LOG( "OnOpen" );
	CurViewState = VIEWSTATE_OPEN;

	LastMovieDisplayed = NULL;
	HadSelection = false;

	if ( Cinema.InLobby )
	{
		Cinema.SceneMgr.SetSceneModel( *Cinema.ModelMgr.BoxOffice );
		Cinema.SceneMgr.UseOverlay = false;

		Vector3f size( PosterWidth * VRMenuObject::DEFAULT_TEXEL_SCALE * PosterScale.x, PosterHeight * VRMenuObject::DEFAULT_TEXEL_SCALE * PosterScale.y, 0.0f );

		Cinema.SceneMgr.SceneScreenBounds = Bounds3f( size * -0.5f, size * 0.5f );
		Cinema.SceneMgr.SceneScreenBounds.Translate( Vector3f(  0.00f, 1.76f,  -7.25f ) );
	}

	Cinema.SceneMgr.LightsOn( 1.5f );

	const double now = vrapi_GetTimeInSeconds();
	SelectionFader.Set( now, 0, now + 0.1, 1.0f );

	CategoryRoot->SetVisible( true );
	Menu->SetFlags( VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP );


	ResumeIcon->SetVisible( false );
	TimerIcon->SetVisible( false );
	CenterRoot->SetVisible( true );
	
	MoveScreenLabel->SetVisible( false );

	MovieBrowser->SetSelectionIndex( MoviesIndex );

	RepositionScreen = false;
	MoveScreenAlpha.Set( 0, 0, 0, 0.0f );

	UpdateMenuPosition();
	Cinema.SceneMgr.ClearGazeCursorGhosts();
	Menu->Open();

	MoviePosterComponent::ShowShadows = Cinema.InLobby;

	CarouselSwipeHintComponent::ShowSwipeHints = true;
}

void PcSelectionView::OnClose()
{
	LOG( "OnClose" );
	ShowTimer = false;
	CurViewState = VIEWSTATE_CLOSED;
	CenterRoot->SetVisible( false );
	Menu->Close();
	Cinema.SceneMgr.ClearMovie();
}

bool PcSelectionView::BackPressed()
{
	if(ErrorShown())
	{
		ClearError();
		return true;
	}
	if(newPCMenu->GetVisible())
	{
		newPCMenu->SetVisible(false);
		IPoctets[0] = IPoctets[1] = IPoctets[2] = IPoctets[3] = 0;
		currentOctet = 0;
		IPString = "_.0.0.0";

		return true;
	}
	return false;
}


bool PcSelectionView::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	switch ( keyCode )
	{
		case AKEYCODE_BACK:
		{
			switch ( eventType )
			{
				case KEY_EVENT_SHORT_PRESS:
					return BackPressed();
					break;
				default:
					break;
			}
		}
	}
	return false;
}

//=======================================================================================

void NewPCIPButtonCallback( UIButton *button, void *object )
{
	( ( PcSelectionView * )object )->NewPCIPButtonPressed(button);
}



	void PcSelectionView::CreateMenu( OvrGuiSys & guiSys )
{
	OVR_UNUSED( guiSys );

	const Quatf forward( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f );

	// ==============================================================================
	//
	// load textures
	//
	SelectionTexture.LoadTextureFromApplicationPackage( "assets/selection.png" );
	Is3DIconTexture.LoadTextureFromApplicationPackage( "assets/3D_icon.png" );
	ShadowTexture.LoadTextureFromApplicationPackage( "assets/shadow.png" );
	BorderTexture.LoadTextureFromApplicationPackage( "assets/category_border.png" );
	SwipeIconLeftTexture.LoadTextureFromApplicationPackage( "assets/SwipeSuggestionArrowLeft.png" );
	SwipeIconRightTexture.LoadTextureFromApplicationPackage( "assets/SwipeSuggestionArrowRight.png" );
	ResumeIconTexture.LoadTextureFromApplicationPackage( "assets/resume.png" );
	ErrorIconTexture.LoadTextureFromApplicationPackage( "assets/error.png" );
	SDCardTexture.LoadTextureFromApplicationPackage( "assets/sdcard.png" );

	newPCTex = LoadTextureFromApplicationPackage(
			"assets/generic_add_poster.png",
			TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), newPCWidth, newPCHeight);
	bgTintTexture.LoadTextureFromApplicationPackage( "assets/backgroundTint.png" );

    ButtonTexture.LoadTextureFromApplicationPackage( "assets/button.png" );
    ButtonHoverTexture.LoadTextureFromApplicationPackage( "assets/button.png" );
    ButtonPressedTexture.LoadTextureFromApplicationPackage( "assets/button.png" );

	// ==============================================================================
	//
	// create menu
	//
	Menu = new UIMenu( Cinema.GetGuiSys() );
	Menu->Create( "PcBrowser" );

	CenterRoot = new UIContainer( Cinema.GetGuiSys() );
	CenterRoot->AddToMenu( Menu );
	CenterRoot->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, 0.0f ) );

	MovieRoot = new UIContainer( Cinema.GetGuiSys() );
	MovieRoot->AddToMenu( Menu, CenterRoot );
	MovieRoot->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, 0.0f ) );

	CategoryRoot = new UIContainer( Cinema.GetGuiSys() );
	CategoryRoot->AddToMenu( Menu, CenterRoot );
	CategoryRoot->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, 0.0f ) );

	TitleRoot = new UIContainer( Cinema.GetGuiSys() );
	TitleRoot->AddToMenu( Menu, CenterRoot );
	TitleRoot->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, 0.0f ) );

	// ==============================================================================
	//
	// error message
	//
	ErrorMessage = new UILabel( Cinema.GetGuiSys() );
	ErrorMessage->AddToMenu( Menu, CenterRoot );
	ErrorMessage->SetLocalPose( forward, Vector3f( 0.00f, 1.76f, -7.39f + 0.5f ) );
	ErrorMessage->SetLocalScale( Vector3f( 5.0f ) );
	ErrorMessage->SetFontScale( 0.5f );
	ErrorMessage->SetTextOffset( Vector3f( 0.0f, -48 * VRMenuObject::DEFAULT_TEXEL_SCALE, 0.0f ) );
	ErrorMessage->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, ErrorIconTexture );
	ErrorMessage->SetVisible( false );

	// ==============================================================================
	//
	// sdcard icon
	//
	SDCardMessage = new UILabel( Cinema.GetGuiSys() );
	SDCardMessage->AddToMenu( Menu, CenterRoot );
	SDCardMessage->SetLocalPose( forward, Vector3f( 0.00f, 1.76f + 330.0f * VRMenuObject::DEFAULT_TEXEL_SCALE, -7.39f + 0.5f ) );
	SDCardMessage->SetLocalScale( Vector3f( 5.0f ) );
	SDCardMessage->SetFontScale( 0.5f );
	SDCardMessage->SetTextOffset( Vector3f( 0.0f, -96 * VRMenuObject::DEFAULT_TEXEL_SCALE, 0.0f ) );
	SDCardMessage->SetVisible( false );
	SDCardMessage->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, SDCardTexture );

	// ==============================================================================
	//
	// error without icon
	//
	PlainErrorMessage = new UILabel( Cinema.GetGuiSys() );
	PlainErrorMessage->AddToMenu( Menu, CenterRoot );
	PlainErrorMessage->SetLocalPose( forward, Vector3f( 0.00f, 1.76f + ( 330.0f - 48 ) * VRMenuObject::DEFAULT_TEXEL_SCALE, -7.39f + 0.5f ) );
	PlainErrorMessage->SetLocalScale( Vector3f( 5.0f ) );
	PlainErrorMessage->SetFontScale( 0.5f );
	PlainErrorMessage->SetVisible( false );

	// ==============================================================================
	//
	// movie browser
	//
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f( -5.59f, 1.76f, -12.55f ), Vector4f( 0.0f, 0.0f, 0.0f, 0.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f( -3.82f, 1.76f, -10.97f ), Vector4f( 0.1f, 0.1f, 0.1f, 1.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f( -2.05f, 1.76f,  -9.39f ), Vector4f( 0.2f, 0.2f, 0.2f, 1.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f(  0.00f, 1.76f,  -7.39f ), Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f(  2.05f, 1.76f,  -9.39f ), Vector4f( 0.2f, 0.2f, 0.2f, 1.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f(  3.82f, 1.76f, -10.97f ), Vector4f( 0.1f, 0.1f, 0.1f, 1.0f ) ) );
	MoviePanelPositions.PushBack( PanelPose( forward, Vector3f(  5.59f, 1.76f, -12.55f ), Vector4f( 0.0f, 0.0f, 0.0f, 0.0f ) ) );

	CenterIndex = MoviePanelPositions.GetSize() / 2;
	CenterPosition = MoviePanelPositions[ CenterIndex ].Position;

	MovieBrowser = new CarouselBrowserComponent( MovieBrowserItems, MoviePanelPositions );
	MovieRoot->AddComponent( MovieBrowser );

	// ==============================================================================
	//
	// selection rectangle
	//
	SelectionFrame = new UIImage( Cinema.GetGuiSys() );
	SelectionFrame->AddToMenu( Menu, MovieRoot );
	SelectionFrame->SetLocalPose( forward, CenterPosition + Vector3f( 0.0f, 0.0f, 0.1f ) );
	SelectionFrame->SetLocalScale( PosterScale );
	SelectionFrame->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, SelectionTexture );
	SelectionFrame->AddComponent( new MovieSelectionComponent( this ) );

	const Vector3f selectionBoundsExpandMin = Vector3f( 0.0f );
	const Vector3f selectionBoundsExpandMax = Vector3f( 0.0f, -0.13f, 0.0f );
	SelectionFrame->GetMenuObject()->SetLocalBoundsExpand( selectionBoundsExpandMin, selectionBoundsExpandMax );

	// ==============================================================================
	//
	// add shadow and 3D icon to movie poster panels
	//
	Array<VRMenuObject *> menuObjs;
	for ( UPInt i = 0; i < MoviePanelPositions.GetSize(); ++i )
	{
		UIContainer *posterContainer = new UIContainer( Cinema.GetGuiSys() );
		posterContainer->AddToMenu( Menu, MovieRoot );
		posterContainer->SetLocalPose( MoviePanelPositions[ i ].Orientation, MoviePanelPositions[ i ].Position );
		posterContainer->GetMenuObject()->AddFlags( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED );

		//
		// posters
		//
		UIImage * posterImage = new UIImage( Cinema.GetGuiSys() );
		posterImage->AddToMenu( Menu, posterContainer );
		posterImage->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, 0.0f ) );
		posterImage->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, SelectionTexture.Texture, PosterWidth, PosterHeight );
		posterImage->SetLocalScale( PosterScale );
		posterImage->GetMenuObject()->AddFlags( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED );
		posterImage->GetMenuObject()->SetLocalBoundsExpand( selectionBoundsExpandMin, selectionBoundsExpandMax );

		if ( i == CenterIndex )
		{
			CenterPoster = posterImage;
		}

		//
		// 3D icon
		//
		UIImage * is3DIcon = new UIImage( Cinema.GetGuiSys() );
		is3DIcon->AddToMenu( Menu, posterContainer );
		is3DIcon->SetLocalPose( forward, Vector3f( 0.75f, 1.3f, 0.02f ) );
		is3DIcon->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, Is3DIconTexture );
		is3DIcon->SetLocalScale( PosterScale );
		is3DIcon->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );

		//
		// shadow
		//
		UIImage * shadow = new UIImage( Cinema.GetGuiSys() );
		shadow->AddToMenu( Menu, posterContainer );
		shadow->SetLocalPose( forward, Vector3f( 0.0f, -1.97f, 0.00f ) );
		shadow->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, ShadowTexture );
		shadow->SetLocalScale( PosterScale );
		shadow->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );

		//
		// add the component
		//
		MoviePosterComponent *posterComp = new MoviePosterComponent();
		posterComp->SetMenuObjects( PosterWidth, PosterHeight, posterContainer, posterImage, is3DIcon, shadow );
		posterContainer->AddComponent( posterComp );

		menuObjs.PushBack( posterContainer->GetMenuObject() );
		MoviePosterComponents.PushBack( posterComp );
	}

	MovieBrowser->SetMenuObjects( menuObjs, MoviePosterComponents );

	// ==============================================================================
	//
	// category browser
	//
	Categories.PushBack( PcCategoryButton( CATEGORY_LIMELIGHT, Cinema.GetCinemaStrings().Category_LimeLight ) );

	// create the buttons and calculate their size
	const float itemWidth = 1.10f;
	float categoryBarWidth = 0.0f;
	for ( UPInt i = 0; i < Categories.GetSize(); ++i )
	{
		Categories[ i ].Button = new UILabel( Cinema.GetGuiSys() );
		Categories[ i ].Button->AddToMenu( Menu, CategoryRoot );
		Categories[ i ].Button->SetFontScale( 2.2f );
		Categories[ i ].Button->SetText( Categories[ i ].Text.ToCStr() );
		Categories[ i ].Button->AddComponent( new PcCategoryComponent( this, Categories[ i ].Category ) );

		const Bounds3f & bounds = Categories[ i ].Button->GetTextLocalBounds( Cinema.GetGuiSys().GetDefaultFont() );
		Categories[ i ].Width = OVR::Alg::Max( bounds.GetSize().x, itemWidth ) + 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
		Categories[ i ].Height = bounds.GetSize().y + 108.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
		categoryBarWidth += Categories[ i ].Width;
	}

	// reposition the buttons and set the background and border
	float startX = categoryBarWidth * -0.5f;
	for ( UPInt i = 0; i < Categories.GetSize(); ++i )
	{
		VRMenuSurfaceParms panelSurfParms( "",
				BorderTexture.Texture, BorderTexture.Width, BorderTexture.Height, SURFACE_TEXTURE_ADDITIVE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );

		panelSurfParms.Border = Vector4f( 14.0f );
		panelSurfParms.Dims = Vector2f( Categories[ i ].Width * VRMenuObject::TEXELS_PER_METER, Categories[ i ].Height * VRMenuObject::TEXELS_PER_METER );

		Categories[ i ].Button->SetImage( 0, panelSurfParms );
		Categories[ i ].Button->SetLocalPose( forward, Vector3f( startX + Categories[ i ].Width * 0.5f, 3.6f, -7.39f ) );
		Categories[ i ].Button->SetLocalBoundsExpand( Vector3f( 0.0f, 0.13f, 0.0f ), Vector3f::ZERO );

    	startX += Categories[ i ].Width;
	}

	// ==============================================================================
	//
	// movie title
	//
	MovieTitle = new UILabel( Cinema.GetGuiSys() );
	MovieTitle->AddToMenu( Menu, TitleRoot );
	MovieTitle->SetLocalPose( forward, Vector3f( 0.0f, 0.045f, -7.37f ) );
	MovieTitle->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	MovieTitle->SetFontScale( 2.5f );

	// ==============================================================================
	//
	// swipe icons
	//
	float yPos = 1.76f - ( PosterHeight - SwipeIconLeftTexture.Height ) * 0.5f * VRMenuObject::DEFAULT_TEXEL_SCALE * PosterScale.y;

	for ( int i = 0; i < NumSwipeTrails; i++ )
	{
		Vector3f swipeIconPos = Vector3f( 0.0f, yPos, -7.17f + 0.01f * ( float )i );

		LeftSwipes[ i ] = new UIImage( Cinema.GetGuiSys() );
		LeftSwipes[ i ]->AddToMenu( Menu, CenterRoot );
		LeftSwipes[ i ]->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, SwipeIconLeftTexture );
		LeftSwipes[ i ]->SetLocalScale( PosterScale );
		LeftSwipes[ i ]->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
		LeftSwipes[ i ]->AddComponent( new CarouselSwipeHintComponent( MovieBrowser, false, 1.3333f, 0.4f + ( float )i * 0.13333f, 5.0f ) );

		swipeIconPos.x = ( ( PosterWidth + SwipeIconLeftTexture.Width * ( i + 2 ) ) * -0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE * PosterScale.x;
		LeftSwipes[ i ]->SetLocalPosition( swipeIconPos );

		RightSwipes[ i ] = new UIImage( Cinema.GetGuiSys() );
		RightSwipes[ i ]->AddToMenu( Menu, CenterRoot );
		RightSwipes[ i ]->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, SwipeIconRightTexture );
		RightSwipes[ i ]->SetLocalScale( PosterScale );
		RightSwipes[ i ]->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
		RightSwipes[ i ]->AddComponent( new CarouselSwipeHintComponent( MovieBrowser, true, 1.3333f, 0.4f + ( float )i * 0.13333f, 5.0f ) );

		swipeIconPos.x = ( ( PosterWidth + SwipeIconRightTexture.Width * ( i + 2 ) ) * 0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE * PosterScale.x;
		RightSwipes[ i ]->SetLocalPosition( swipeIconPos );
    }

	// ==============================================================================
	//
	// resume icon
	//
	ResumeIcon = new UILabel( Cinema.GetGuiSys() );
	ResumeIcon->AddToMenu( Menu, MovieRoot );
	ResumeIcon->SetLocalPose( forward, CenterPosition + Vector3f( 0.0f, 0.0f, 0.5f ) );
	ResumeIcon->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, ResumeIconTexture );
	ResumeIcon->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	ResumeIcon->SetFontScale( 0.3f );
	ResumeIcon->SetLocalScale( 6.0f );
	ResumeIcon->SetText( Cinema.GetCinemaStrings().MovieSelection_Resume.ToCStr() );
	ResumeIcon->SetTextOffset( Vector3f( 0.0f, -ResumeIconTexture.Height * VRMenuObject::DEFAULT_TEXEL_SCALE * 0.5f, 0.0f ) );
	ResumeIcon->SetVisible( false );

	// ==============================================================================
	//
	// timer
	//
	TimerIcon = new UILabel( Cinema.GetGuiSys() );
	TimerIcon->AddToMenu( Menu, MovieRoot );
	TimerIcon->SetLocalPose( forward, CenterPosition + Vector3f( 0.0f, 0.0f, 0.5f ) );
	TimerIcon->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	TimerIcon->SetFontScale( 1.0f );
	TimerIcon->SetLocalScale( 2.0f );
	TimerIcon->SetText( "10" );
	TimerIcon->SetVisible( false );

	VRMenuSurfaceParms timerSurfaceParms( "timer",
		"apk:///assets/timer.tga", SURFACE_TEXTURE_DIFFUSE,
		"apk:///assets/timer_fill.tga", SURFACE_TEXTURE_COLOR_RAMP_TARGET,
		"apk:///assets/color_ramp_timer.tga", SURFACE_TEXTURE_COLOR_RAMP );

	TimerIcon->SetImage( 0, timerSurfaceParms );

	// text
	TimerText = new UILabel( Cinema.GetGuiSys() );
	TimerText->AddToMenu( Menu, TimerIcon );
	TimerText->SetLocalPose( forward, Vector3f( 0.0f, -0.3f, 0.0f ) );
	TimerText->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	TimerText->SetFontScale( 1.0f );
	TimerText->SetText( Cinema.GetCinemaStrings().MovieSelection_Next );

	// ==============================================================================
	//
	// reorient message
	//
	MoveScreenLabel = new UILabel( Cinema.GetGuiSys() );
	MoveScreenLabel->AddToMenu( Menu, NULL );
	MoveScreenLabel->SetLocalPose( forward, Vector3f( 0.0f, 0.0f, -1.8f ) );
	MoveScreenLabel->GetMenuObject()->AddFlags( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ) );
	MoveScreenLabel->SetFontScale( 0.5f );
	MoveScreenLabel->SetText( Cinema.GetCinemaStrings().MoviePlayer_Reorient );
	MoveScreenLabel->SetTextOffset( Vector3f( 0.0f, -24 * VRMenuObject::DEFAULT_TEXEL_SCALE, 0.0f ) );  // offset to be below gaze cursor
	MoveScreenLabel->SetVisible( false );

	// ==============================================================================
	//
	// IP Entry
	//
	newPCMenu= new UIContainer( Cinema.GetGuiSys() );
	newPCMenu->AddToMenu(  Menu );
	newPCMenu->SetLocalPose( forward, Vector3f( 0.0f, 1.5f, -3.0f ) );
	newPCMenu->SetVisible(false);

	newPCbg.AddToMenu(  Menu, newPCMenu);
    newPCbg.SetImage( 0, SURFACE_TEXTURE_DIFFUSE, bgTintTexture, 550, 1000 );



	newPCIPLabel.AddToMenu(  Menu, &newPCbg );
	newPCIPLabel.SetLocalPosition( Vector3f( 0.0f, 0.8f, 0.1f ) );
	newPCIPLabel.SetFontScale( 1.4f );
	newPCIPLabel.SetText( IPString );
	newPCIPLabel.SetTextOffset( Vector3f( 0.0f, 0.0f, 0.01f ) );
	newPCIPLabel.SetImage( 0, SURFACE_TEXTURE_DIFFUSE, bgTintTexture, 550, 120 );

	const int numButtons = 13;
	const char* buttons[numButtons] = {"7","8","9","4","5","6","1","2","3","0",".","<","Enter"};
	int cols = 3;
	newPCIPButtons.Resize(numButtons);
	for(int i = 0; i < numButtons; i++ )
	{
		UIButton *button = new UIButton( Cinema.GetGuiSys() );
		button->AddToMenu( Menu, &newPCbg );
		button->SetLocalPosition( Vector3f( -0.3f + (i % cols) * 0.3f, 0.45f + (i / cols) * -0.3f, 0.15f ) );
		button->SetText( buttons[i] );
		button->SetLocalScale( Vector3f( 1.0f ) );
		//button->SetFontScale( 1.0f );
		button->SetColor( Vector4f( 0.0f, 0.0f, 0.0f, 1.0f ) );
		//button->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, bgTintTexture, 120, 120 );
        button->SetButtonImages( ButtonTexture, ButtonHoverTexture, ButtonPressedTexture );
		button->SetOnClick( NewPCIPButtonCallback, this);
		//button->UpdateButtonState();
		newPCIPButtons[i] = button;
	}
	//newPCIPButtons[numButtons - 1]->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, bgTintTexture, 320, 120 );
    newPCIPButtons[numButtons - 1]->SetButtonImages( ButtonTexture, ButtonHoverTexture, ButtonPressedTexture );
	newPCIPButtons[numButtons - 1]->SetLocalPosition(Vector3f( 0.0f, 0.45f + ( (numButtons -1) / cols) * -0.3f, 0.15f ));

}

	void PcSelectionView::NewPCIPButtonPressed( UIButton *button)
	{

		char bLabel = button->GetText()[0];
        //char bLabel = '.';
        int error=0;
        switch(bLabel)
		{
			case '<':
				if( IPoctets[currentOctet] == 0 && currentOctet > 0) currentOctet--;
				IPoctets[currentOctet] /= 10;
				break;
			case '.':
				currentOctet++;
				if(currentOctet > 3) currentOctet = 3;
				break;
			case 'E':
				newPCMenu->SetVisible(false);
                IPString = "";
                for(int i=0;i<=3;i++)
				{
					if( i != 0 ) IPString += ".";
					IPString += StringUtils::Va( "%i", IPoctets[i] );
                }
                error = Native::addPCbyIP(Cinema.app, IPString.ToCStr());
                if ( error == 2 )
                {
                    SetError( "Error_UnknownHost", false, false );
                }
                else if( error == 1 )
                {
                    SetError( "Error_AddPCFailed", false, false );
				}
				IPoctets[0] = IPoctets[1] = IPoctets[2] = IPoctets[3] = 0;
				currentOctet = 0;
				break;
			case '0':
				if(IPoctets[currentOctet]==0)
				{
					currentOctet++;
					if(currentOctet > 3) currentOctet = 3;
				}
			default: // numbers
				int number = bLabel - '0';
				if(IPoctets[currentOctet] * 10 + number > 255)
				{
					currentOctet++;
					if(currentOctet > 3) currentOctet = 3;
				}
				if(IPoctets[currentOctet] * 10 + number <= 255)
				{
					IPoctets[currentOctet] = IPoctets[currentOctet] * 10 + number;
				}
				if(IPoctets[currentOctet] >= 26)
				{
					currentOctet++;
					if(currentOctet > 3) currentOctet = 3;
				}
				break;
		}
		IPString = "";
		for(int i=0;i<=3;i++)
		{
			if( i != 0 ) IPString += ".";
			if( i != currentOctet || IPoctets[i] != 0) IPString += StringUtils::Va( "%i", IPoctets[i] );
			if( i == currentOctet ) IPString += "_";
		}
		newPCIPLabel.SetText( IPString );

	}

Vector3f PcSelectionView::ScalePosition( const Vector3f &startPos, const float scale, const float menuOffset ) const
{
	const float eyeHieght = Cinema.SceneMgr.Scene.GetEyeHeight();

	Vector3f pos = startPos;
	pos.x *= scale;
	pos.y = ( pos.y - eyeHieght ) * scale + eyeHieght + menuOffset;
	pos.z *= scale;
	pos += Cinema.SceneMgr.Scene.GetFootPos();

	return pos;
}

//
// Repositions the menu for the lobby scene or the theater
//
void PcSelectionView::UpdateMenuPosition()
{
	// scale down when in a theater
	const float scale = Cinema.InLobby ? 1.0f : 0.55f;
	CenterRoot->GetMenuObject()->SetLocalScale( Vector3f( scale ) );

	if ( !Cinema.InLobby && Cinema.SceneMgr.SceneInfo.UseFreeScreen )
	{
		Quatf orientation = Quatf( Cinema.SceneMgr.FreeScreenPose );
		CenterRoot->GetMenuObject()->SetLocalRotation( orientation );
		CenterRoot->GetMenuObject()->SetLocalPosition( Cinema.SceneMgr.FreeScreenPose.Transform( Vector3f( 0.0f, -1.76f * scale, 0.0f ) ) );
	}
	else
	{
		const float menuOffset = Cinema.InLobby ? 0.0f : 0.5f;
		CenterRoot->GetMenuObject()->SetLocalRotation( Quatf() );
		CenterRoot->GetMenuObject()->SetLocalPosition( ScalePosition( Vector3f::ZERO, scale, menuOffset ) );
	}
}

//============================================================================================

void PcSelectionView::Select()
{
	LOG( "SelectMovie");

	// ignore selection while repositioning screen
	if ( RepositionScreen )
	{
		return;
	}


	MoviesIndex = MovieBrowser->GetSelection();

	if (MoviesIndex >= (int)MovieList.GetSize())
	{
		// open up manual IP entry menu
		newPCMenu->SetVisible(true);
		return;
	}


	Cinema.SetPc(MovieList[MoviesIndex]);


	if ( !Cinema.InLobby )
	{
		Cinema.AppSelection(false);
	}
	else
	{
		Cinema.AppSelection( false );

	}
}

void PcSelectionView::StartTimer()
{
	const double now = vrapi_GetTimeInSeconds();
	TimerStartTime = now;
	TimerValue = -1;
	ShowTimer = true;
}

const PcDef *PcSelectionView::GetSelectedPc() const
{
	int selectedItem = MovieBrowser->GetSelection();
	if ( ( selectedItem >= 0 ) && ( selectedItem < MovieList.GetSizeI() ) )
	{
		return MovieList[ selectedItem ];
	}

	return NULL;
}

void PcSelectionView::SetPcList( const Array<const PcDef *> &movies, const PcDef *nextMovie )
{
	LOG( "SetPcList: %zu movies", movies.GetSize() );

	MovieList = movies;
	DeletePointerArray( MovieBrowserItems );
	for ( UPInt i = 0; i < MovieList.GetSize(); i++ )
	{
		const PcDef *movie = MovieList[ i ];

		LOG( "AddMovie: %s", movie->Name.ToCStr() );


		CarouselItem *item = new CarouselItem();
		item->Texture 		= movie->Poster;
		item->TextureWidth 	= movie->PosterWidth;
		item->TextureHeight	= movie->PosterHeight;
		item->UserData 		= ( void * )movie;
		MovieBrowserItems.PushBack( item );
	}

	CarouselItem *addPCitem        = new CarouselItem();
	addPCitem->Texture             = newPCTex;
	addPCitem->TextureWidth     = newPCWidth;
	addPCitem->TextureHeight    = newPCHeight;
	addPCitem->UserData = 0;
	MovieBrowserItems.PushBack( addPCitem );


	MovieBrowser->SetItems( MovieBrowserItems );

	MovieTitle->SetText( "" );
	LastMovieDisplayed = NULL;


	if(MoviesIndex > MovieList.GetSizeI()) MoviesIndex = 0;
	/*
    MoviesIndex = 0;
    if ( nextMovie != NULL )
    {
        for ( int i = 0; i < MovieList.GetSizeI(); i++ )
        {
            if ( movies[ i ] == nextMovie )
            {
                StartTimer();
                MoviesIndex = i;
                break;
            }
        }
    }

    MovieBrowser->SetSelectionIndex( MoviesIndex );
*/
/*    if ( MovieList.GetSize() == 0 )
    {
        if ( CurrentCategory == CATEGORY_LIMELIGHT )
        {
            SetError( Cinema.GetCinemaStrings().Error_NoVideosInLimeLight.ToCStr(), false, false );
        }
        else
        {
            SetError( Cinema.GetCinemaStrings().Error_NoVideosOnPhone.ToCStr(), true, false );
        }
    }
    else
    {
        ClearError();
    }
        */
}

void PcSelectionView::SetCategory( const PcCategory category )
{
    // default to category in index 0
    UPInt categoryIndex = 0;
    for ( UPInt i = 0; i < Categories.GetSize(); ++i )
    {
        if ( category == Categories[ i ].Category )
        {
            categoryIndex = i;
            break;
        }
    }

    LOG( "SetCategory: %s", Categories[ categoryIndex ].Text.ToCStr() );
    CurrentCategory = Categories[ categoryIndex ].Category;
    for ( UPInt i = 0; i < Categories.GetSize(); ++i )
    {
        Categories[ i ].Button->SetHilighted( i == categoryIndex );
    }

    // reset all the swipe icons so they match the current poster
    for ( int i = 0; i < NumSwipeTrails; i++ )
    {
        CarouselSwipeHintComponent * compLeft = LeftSwipes[ i ]->GetMenuObject()->GetComponentByTypeName<CarouselSwipeHintComponent>();
        compLeft->Reset( LeftSwipes[ i ]->GetMenuObject() );
        CarouselSwipeHintComponent * compRight = RightSwipes[ i ]->GetMenuObject()->GetComponentByTypeName<CarouselSwipeHintComponent>();
        compRight->Reset( RightSwipes[ i ]->GetMenuObject() );
    }

    SetPcList( Cinema.PcMgr.GetPcList( CurrentCategory ), NULL );

    LOG( "%zu movies added", MovieList.GetSize() );
}

void PcSelectionView::UpdatePcTitle()
{
    const PcDef * currentMovie = GetSelectedPc();
    if ( LastMovieDisplayed != currentMovie )
    {
        if ( currentMovie != NULL )
        {
            MovieTitle->SetText( currentMovie->Name.ToCStr() );
        }
        else
        {
			MovieTitle->SetText( "Add PC manually" );
		}

        LastMovieDisplayed = currentMovie;
    }
}

void PcSelectionView::SelectionHighlighted( bool isHighlighted )
{
    if ( isHighlighted && !ShowTimer && !Cinema.InLobby && ( MoviesIndex == MovieBrowser->GetSelection() ) )
    {
        // dim the poster when the resume icon is up and the poster is highlighted
        CenterPoster->SetColor( Vector4f( 0.55f, 0.55f, 0.55f, 1.0f ) );
    }
    else if ( MovieBrowser->HasSelection() )
    {
        CenterPoster->SetColor( Vector4f( 1.0f ) );
    }
}

void PcSelectionView::UpdateSelectionFrame( const ovrFrameInput & vrFrame )
{
    const double now = vrapi_GetTimeInSeconds();
    if ( !MovieBrowser->HasSelection() )
    {
        SelectionFader.Set( now, 0, now + 0.1, 1.0f );
        TimerStartTime = 0;
    }

    if ( !SelectionFrame->GetMenuObject()->IsHilighted() )
    {
        SelectionFader.Set( now, 0, now + 0.1, 1.0f );
    }
    else
    {
        MovieBrowser->CheckGamepad( Cinema.GetGuiSys(), vrFrame, MovieRoot->GetMenuObject() );
    }

    SelectionFrame->SetColor( Vector4f( static_cast<float>( SelectionFader.Value( now ) ) ) );

    if ( !ShowTimer && !Cinema.InLobby && ( MoviesIndex == MovieBrowser->GetSelection() ) )
    {
        ResumeIcon->SetColor( Vector4f( static_cast<float>( SelectionFader.Value( now ) ) ) );
        ResumeIcon->SetTextColor( Vector4f( static_cast<float>( SelectionFader.Value( now ) ) ) );
        ResumeIcon->SetVisible( true );
    }
    else
    {
        ResumeIcon->SetVisible( false );
    }

    if ( ShowTimer && ( TimerStartTime != 0 ) )
    {
        double frac = ( now - TimerStartTime ) / TimerTotalTime;
        if ( frac > 1.0f )
        {
            frac = 1.0f;
            //Cinema.SetPlaylist( MovieList, MovieBrowser->GetSelection() );
            Cinema.ResumeOrRestartMovie();
        }
        Vector2f offset( 0.0f, 1.0f - static_cast<float>( frac ) );
        TimerIcon->SetColorTableOffset( offset );

        int seconds = static_cast<int>( TimerTotalTime - ( TimerTotalTime * frac ) );
        if ( TimerValue != seconds )
        {
            TimerValue = seconds;
            const char * text = StringUtils::Va( "%d", seconds );
            TimerIcon->SetText( text );
        }
        TimerIcon->SetVisible( true );
        CenterPoster->SetColor( Vector4f( 0.55f, 0.55f, 0.55f, 1.0f ) );
    }
    else
    {
        TimerIcon->SetVisible( false );
    }
}

void PcSelectionView::SetError( const char *text, bool showSDCard, bool showErrorIcon )
{
    ClearError();

    LOG( "SetError: %s", text );
    if ( showSDCard )
    {
        SDCardMessage->SetVisible( true );
        SDCardMessage->SetTextWordWrapped( text, Cinema.GetGuiSys().GetDefaultFont(), 1.0f );
    }
    else if ( showErrorIcon )
    {
        ErrorMessage->SetVisible( true );
        ErrorMessage->SetTextWordWrapped( text, Cinema.GetGuiSys().GetDefaultFont(), 1.0f );
    }
    else
    {
        PlainErrorMessage->SetVisible( true );
        PlainErrorMessage->SetTextWordWrapped( text, Cinema.GetGuiSys().GetDefaultFont(), 1.0f );
    }
    TitleRoot->SetVisible( false );
    MovieRoot->SetVisible( false );

    CarouselSwipeHintComponent::ShowSwipeHints = false;
}

void PcSelectionView::ClearError()
{
    LOG( "ClearError" );
    ErrorMessageClicked = false;
    ErrorMessage->SetVisible( false );
    SDCardMessage->SetVisible( false );
    PlainErrorMessage->SetVisible( false );
    TitleRoot->SetVisible( true );
    MovieRoot->SetVisible( true );
	CategoryRoot->SetVisible( true );

    CarouselSwipeHintComponent::ShowSwipeHints = true;
}

bool PcSelectionView::ErrorShown() const
{
    return ErrorMessage->GetVisible() || SDCardMessage->GetVisible() || PlainErrorMessage->GetVisible();
}

void PcSelectionView::Frame( const ovrFrameInput & vrFrame )
{
    // We want 4x MSAA in the lobby
    ovrEyeBufferParms eyeBufferParms = Cinema.app->GetEyeBufferParms();
    eyeBufferParms.multisamples = 4;
    Cinema.app->SetEyeBufferParms( eyeBufferParms );

#if 0
    if ( !Cinema.InLobby && Cinema.SceneMgr.ChangeSeats( vrFrame ) )
    {
        UpdateMenuPosition();
    }
#endif

    if ( vrFrame.Input.buttonPressed & BUTTON_B )
    {
        if ( Cinema.InLobby )
        {
            Cinema.app->ShowSystemUI( VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
        }
        else
        {
            Cinema.GetGuiSys().CloseMenu( Menu->GetVRMenu(), false );
        }
    }

    // check if they closed the menu with the back button
    if ( !Cinema.InLobby && Menu->GetVRMenu()->IsClosedOrClosing() && !Menu->GetVRMenu()->IsOpenOrOpening() )
    {
        // if we finished the movie or have an error, don't resume it, go back to the lobby
        if ( ErrorShown() )
        {
            LOG( "Error closed.  Return to lobby." );
            ClearError();
            Cinema.PcSelection( true );
        }
        else if ( Cinema.IsMovieFinished() )
        {
            LOG( "Movie finished.  Return to lobby." );
            Cinema.PcSelection( true );
        }
        else
        {
            LOG( "Resume movie." );
            Cinema.ResumeMovieFromSavedLocation();
        }
    }

    if ( !Cinema.InLobby && ErrorShown() )
    {
        CarouselSwipeHintComponent::ShowSwipeHints = false;
        if ( vrFrame.Input.buttonPressed & ( BUTTON_TOUCH | BUTTON_A ) )
        {
            Cinema.GetGuiSys().GetSoundEffectPlayer().Play( "touch_down" );
        }
        else if ( vrFrame.Input.buttonReleased & ( BUTTON_TOUCH | BUTTON_A ) )
        {
            Cinema.GetGuiSys().GetSoundEffectPlayer().Play( "touch_up" );
            ErrorMessageClicked = true;
        }
        else if ( ErrorMessageClicked && ( ( vrFrame.Input.buttonState & ( BUTTON_TOUCH | BUTTON_A ) ) == 0 ) )
        {
			ClearError();
		}
    }

    if ( Cinema.SceneMgr.FreeScreenActive && !ErrorShown() )
    {
        if ( !RepositionScreen && !SelectionFrame->GetMenuObject()->IsHilighted() )
        {
            // outside of screen, so show reposition message
            const double now = vrapi_GetTimeInSeconds();
            float alpha = static_cast<float>( MoveScreenAlpha.Value( now ) );
            if ( alpha > 0.0f )
            {
                MoveScreenLabel->SetVisible( true );
                MoveScreenLabel->SetTextColor( Vector4f( alpha ) );
            }

            if ( vrFrame.Input.buttonPressed & ( BUTTON_A | BUTTON_TOUCH ) )
            {
                // disable hit detection on selection frame
                SelectionFrame->GetMenuObject()->AddFlags( VRMENUOBJECT_DONT_HIT_ALL );
                RepositionScreen = true;
            }
        }
        else
        {
            // onscreen, so hide message
            const double now = vrapi_GetTimeInSeconds();
            MoveScreenAlpha.Set( now, -1.0f, now + 1.0f, 1.0f );
            MoveScreenLabel->SetVisible( false );
        }

        const Matrix4f invViewMatrix( Cinema.SceneMgr.Scene.GetCenterEyeTransform() );
        const Vector3f viewPos( Cinema.SceneMgr.Scene.GetCenterEyePosition() );
        const Vector3f viewFwd( Cinema.SceneMgr.Scene.GetCenterEyeForward() );

        // spawn directly in front
        Quatf rotation( -viewFwd, 0.0f );
        Quatf viewRot( invViewMatrix );
        Quatf fullRotation = rotation * viewRot;

        const float menuDistance = 1.45f;
        Vector3f position( viewPos + viewFwd * menuDistance );

        MoveScreenLabel->SetLocalPose( fullRotation, position );
    }

    // while we're holding down the button or touchpad, reposition screen
    if ( RepositionScreen )
    {
        if ( vrFrame.Input.buttonState & ( BUTTON_A | BUTTON_TOUCH ) )
        {
            Cinema.SceneMgr.PutScreenInFront();
            Quatf orientation = Quatf( Cinema.SceneMgr.FreeScreenPose );
            CenterRoot->GetMenuObject()->SetLocalRotation( orientation );
            CenterRoot->GetMenuObject()->SetLocalPosition( Cinema.SceneMgr.FreeScreenPose.Transform( Vector3f( 0.0f, -1.76f * 0.55f, 0.0f ) ) );

        }
        else
        {
            RepositionScreen = false;
        }
    }
    else
    {
        // reenable hit detection on selection frame.
        // note: we do this on the frame following the frame we disabled RepositionScreen on
        // so that the selection object doesn't get the touch up.
        SelectionFrame->GetMenuObject()->RemoveFlags( VRMENUOBJECT_DONT_HIT_ALL );
    }

    UpdatePcTitle();
    UpdateSelectionFrame( vrFrame );
    if (Cinema.PcMgr.updated)
    {
        Cinema.PcMgr.updated = false;
        SetPcList( Cinema.PcMgr.GetPcList( CurrentCategory ), NULL );
    }

    Cinema.SceneMgr.Frame( vrFrame );
}

} // namespace OculusCinema
