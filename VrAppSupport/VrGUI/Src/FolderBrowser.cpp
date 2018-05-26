/************************************************************************************

Filename    :   FolderBrowser.cpp
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "FolderBrowser.h"

#include "Kernel/OVR_Threads.h"
#include <stdio.h>
#include "App.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"
#include "DefaultComponent.h"
#include "VrCommon.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_String_Utils.h"
#include "stb_image.h"
#include "PackageFiles.h"
#include "AnimComponents.h"
#include "VrCommon.h"
#include "VRMenuObject.h"
#include "ScrollBarComponent.h"
#include "SwipeHintComponent.h"

namespace OVR {

const float OvrFolderBrowser::CONTROLER_COOL_DOWN = 0.2f; // Controller goes to rest very frequently so cool down helps
const float OvrFolderBrowser::SCROLL_DIRECTION_DECIDING_DISTANCE = 10.0f;

char const * OvrFolderBrowser::MENU_NAME = "FolderBrowser";

const Vector3f FWD( 0.0f, 0.0f, -1.0f );
const Vector3f LEFT( -1.0f, 0.0f, 0.0f );
const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
const Vector3f UP( 0.0f, 1.0f, 0.0f );
const Vector3f DOWN( 0.0f, -1.0f, 0.0f );

const float EYE_HEIGHT_OFFSET 					= 0.0f;
const float MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ 	= 1800.0f;
const float SCROLL_HITNS_VISIBILITY_TOGGLE_TIME = 5.0f;
const float SCROLL_BAR_LENGTH					= 390;
const int 	HIDE_SCROLLBAR_UNTIL_ITEM_COUNT		= 1; // <= 0 makes scroll bar always visible

// Helper class that guarantees unique ids for VRMenuIds
class OvrUniqueId
{
public: 
	explicit OvrUniqueId( int startId )
		: currentId( startId )
	{}

	int Get( const int increment )
	{
		const int toReturn = currentId;
		currentId += increment;
		return toReturn;
	}

private:
	int currentId;
};
OvrUniqueId uniqueId( 1000 );

VRMenuId_t OvrFolderBrowser::ID_CENTER_ROOT( uniqueId.Get( 1 ) );

//==============================
// OvrFolderBrowserRootComponent
// This component is attached to the root parent of the folder browsers and gets to consume input first 
class OvrFolderBrowserRootComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 57514;

	OvrFolderBrowserRootComponent( OvrFolderBrowser & folderBrowser )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_TOUCH_DOWN |
			VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_SWIPE_COMPLETE | VRMENU_EVENT_OPENING )
		, FolderBrowser( folderBrowser )
		, ScrollMgr( VERTICAL_SCROLL )
		, TotalTouchDistance( 0.0f )
		, ValidFoldersCount( 0 )
	{
		LastInteractionTimeStamp = vrapi_GetTimeInSeconds() - SCROLL_HITNS_VISIBILITY_TOGGLE_TIME;
		ScrollMgr.SetScrollPadding(0.5f);
	}

	virtual int		GetTypeId() const
	{ 
		return TYPE_ID;
	}

	int GetCurrentIndex( OvrGuiSys & guiSys, VRMenuObject * self ) const
	{
		VRMenuObject * foldersRootObject = guiSys.GetVRMenuMgr().ToObject( FoldersRootHandle );
		OVR_ASSERT( foldersRootObject != NULL );

		// First calculating index of a folder with in valid folders(folder that has at least one panel) array based on its position
		const int validNumFolders = GetFolderBrowserValidFolderCount();
		const float deltaHeight = FolderBrowser.GetPanelHeight();
		const float maxHeight = deltaHeight * validNumFolders;
		const float positionRatio = foldersRootObject->GetLocalPosition().y / maxHeight;
		int idx = static_cast<int>( nearbyintf( positionRatio * validNumFolders ) );
		idx = Alg::Clamp( idx, 0, FolderBrowser.GetNumFolders() - 1 );

		// Remapping index with in valid folders to index in entire Folder array
		const int numValidFolders = GetFolderBrowserValidFolderCount();
		int counter = 0;
		int remapedIdx = 0;
		for ( ; remapedIdx < numValidFolders; ++remapedIdx )
		{
			OvrFolderBrowser::FolderView * folder = FolderBrowser.GetFolderView( remapedIdx );
			if ( folder && !folder->Panels.IsEmpty() )
			{
				if ( counter == idx )
				{
					break;
				}
				++counter;
			}
		}

		return Alg::Clamp( remapedIdx, 0, FolderBrowser.GetNumFolders() - 1 );
	}

	void SetActiveFolder( int folderIdx )
	{
		ScrollMgr.SetPosition( static_cast<float>( folderIdx - 1 ) );
	}

	void SetFoldersRootHandle( menuHandle_t handle )		{ FoldersRootHandle = handle; }
	void SetScrollDownHintHandle( menuHandle_t handle )		{ ScrollDownHintHandle = handle; }
	void SetScrollUpHintHandle( menuHandle_t handle )		{ ScrollUpHintHandle = handle; }


private:

	// private assignment operator to prevent copying
	OvrFolderBrowserRootComponent &	operator = ( OvrFolderBrowserRootComponent & );

	const static float ACTIVE_DEPTH_OFFSET;

	virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
										VRMenuObject * self, VRMenuEvent const & event )
	{
		switch( event.EventType )
        {
            case VRMENU_EVENT_FRAME_UPDATE:
                return Frame( guiSys, vrFrame, self, event );
			case VRMENU_EVENT_TOUCH_DOWN:
				return TouchDown( guiSys, vrFrame, self, event);
			case VRMENU_EVENT_TOUCH_UP:
				return TouchUp( guiSys, vrFrame, self, event );
			case VRMENU_EVENT_SWIPE_COMPLETE:
			{
				VRMenuEvent upEvent = event;
				upEvent.EventType = VRMENU_EVENT_TOUCH_UP;
				return TouchUp( guiSys, vrFrame, self, upEvent );
			}
            case VRMENU_EVENT_TOUCH_RELATIVE:
                return TouchRelative( guiSys, vrFrame, self, event );
			case VRMENU_EVENT_OPENING:
				return OnOpening( guiSys, vrFrame, self, event );
            default:
                OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
                return MSG_STATUS_ALIVE;
        }
	}

	eMsgStatus Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		const int folderCount = FolderBrowser.GetNumFolders();
		ValidFoldersCount = 0;
		// Hides invalid folders(folder that doesn't have at least one panel), and rearranges valid folders positions to avoid gaps between folders
		for ( int i = 0; i < folderCount; ++i )
		{
			const OvrFolderBrowser::FolderView * currentFolder = FolderBrowser.GetFolderView( i );
			if ( currentFolder != NULL )
			{
				VRMenuObject * folderObject = guiSys.GetVRMenuMgr().ToObject( currentFolder->Handle );
				if ( folderObject != NULL )
				{
					if ( currentFolder->Panels.GetSizeI() > 0 )
					{
						folderObject->SetLocalPosition( ( DOWN * FolderBrowser.GetPanelHeight() * static_cast<float>( ValidFoldersCount ) ) );
						++ValidFoldersCount;
					}
				}
			}
		}

		bool restrictedScrolling = ValidFoldersCount > 1;
		eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
		eScrollDirectionLockType controllerDirLock = FolderBrowser.GetControllerDirectionLock();

		ScrollMgr.SetMaxPosition( static_cast<float>( ValidFoldersCount - 1 ) );
		ScrollMgr.SetRestrictedScrollingData( restrictedScrolling, touchDirLock, controllerDirLock );

		unsigned int controllerInput = 0;
		if ( ValidFoldersCount > 1 ) // Need at least one folder in order to enable vertical scrolling
		{
			controllerInput = vrFrame.Input.buttonState;
			if ( controllerInput & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP | BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN | BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT | BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) )
			{
				LastInteractionTimeStamp = vrapi_GetTimeInSeconds();
			}

			if ( restrictedScrolling )
			{
				if ( touchDirLock == HORIZONTAL_LOCK )
				{
					if ( ScrollMgr.IsTouchIsDown() )
					{
						// Restricted scrolling enabled, but locked horizontal scrolling so ignore vertical scrolling
						ScrollMgr.TouchUp();
					}
				}

				if ( controllerDirLock != VERTICAL_LOCK )
				{
					// Restricted scrolling enabled, but not locked to vertical scrolling so lose the controller input
					controllerInput = 0;
				}
			}
		}
		ScrollMgr.Frame( vrFrame.DeltaSeconds, controllerInput );

		VRMenuObject * foldersRootObject = guiSys.GetVRMenuMgr().ToObject( FoldersRootHandle );
		OVR_ASSERT( foldersRootObject != NULL );
		const Vector3f & rootPosition = foldersRootObject->GetLocalPosition();
		foldersRootObject->SetLocalPosition( Vector3f( rootPosition.x, FolderBrowser.GetPanelHeight() * ScrollMgr.GetPosition(), rootPosition.z ) );

		const float alphaSpace = FolderBrowser.GetPanelHeight() * 2.0f;
		const float rootOffset = rootPosition.y - EYE_HEIGHT_OFFSET;

		// Fade + hide each category/folder based on distance from origin
		for ( int folderIndex = 0; folderIndex < FolderBrowser.GetNumFolders(); ++folderIndex )
		{
			OvrFolderBrowser::FolderView * folder = FolderBrowser.GetFolderView( folderIndex );
			VRMenuObject * child = guiSys.GetVRMenuMgr().ToObject( foldersRootObject->GetChildHandleForIndex( folderIndex ) );
			OVR_ASSERT( child != NULL );

			const Vector3f & position = child->GetLocalPosition();
			Vector4f color = child->GetColor();
			color.w = 0.0f;
			const bool showFolder = FolderBrowser.HasNoMedia() || ( folder->Panels.GetSizeI() > 0 );

			VRMenuObjectFlags_t flags = child->GetFlags();
			const float absolutePosition = fabsf( rootOffset - fabsf( position.y ) );
			if ( absolutePosition < alphaSpace && showFolder )
			{
				// Fade in / out
				float ratio = absolutePosition / alphaSpace;
				float ratioSq = ratio * ratio;
				float finalAlpha = 1.0f - ratioSq;
				color.w = Alg::Clamp( finalAlpha, 0.0f, 1.0f );

				// If we're showing the category for the first time, load the thumbs
				// Only queue up if velocity is below threshold to avoid queuing categories flying past view
				//const float scrollVelocity = ScrollMgr.GetVelocity();
				if ( !folder->Visible /*&& ( fabsf( scrollVelocity ) < 0.09f )*/ )
				{
					LOG( "Revealing %s - loading thumbs", folder->CategoryTag.ToCStr() );
					folder->Visible = true;
					// This loads entire category - instead we load specific thumbs in SwipeComponent
				}

				flags &= ~( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL );

				// Lerp the folder towards or away from viewer
				Vector3f activePosition = position;
				const float targetZ = ACTIVE_DEPTH_OFFSET * finalAlpha;
				activePosition.z = targetZ;
				child->SetLocalPosition( activePosition );
			}
			else
			{
				// If we're just about to hide the folderview, unload the thumbnails
				if ( folder->Visible )
				{
					LOG( "Hiding %s - unloading thumbs", folder->CategoryTag.ToCStr() );
					folder->Visible = false;
					folder->UnloadThumbnails( guiSys, FolderBrowser.GetDefaultThumbnailTextureId(), FolderBrowser.GetThumbWidth(), FolderBrowser.GetThumbHeight() );
				}

				flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL;
			}

			child->SetColor( color );
			child->SetFlags( flags );
		}

		// Updating Scroll Suggestions
		bool 		showScrollUpIndicator 	= false;
		bool 		showBottomIndicator 	= false;
		Vector4f 	finalCol( 1.0f, 1.0f, 1.0f, 1.0f );
		if ( LastInteractionTimeStamp > 0.0f ) // is user interaction currently going on? ( during interaction LastInteractionTimeStamp value will be negative )
		{
			float timeDiff = static_cast<float>( vrapi_GetTimeInSeconds() - LastInteractionTimeStamp );
			if ( timeDiff > SCROLL_HITNS_VISIBILITY_TOGGLE_TIME ) // is user not interacting for a while?
			{
				// Show Indicators
				showScrollUpIndicator = ( ScrollMgr.GetPosition() >  0.8f );
				showBottomIndicator = ( ScrollMgr.GetPosition() < ( ( ValidFoldersCount - 1 ) - 0.8f ) );

				finalCol.w = Alg::Clamp( timeDiff, 5.0f, 6.0f ) - 5.0f;
			}
		}

		VRMenuObject * scrollUpHintObject = guiSys.GetVRMenuMgr().ToObject( ScrollUpHintHandle );
		if ( scrollUpHintObject != NULL )
		{
			scrollUpHintObject->SetVisible( showScrollUpIndicator );
			scrollUpHintObject->SetColor( finalCol );
		}

		VRMenuObject * scrollDownHintObject = guiSys.GetVRMenuMgr().ToObject( ScrollDownHintHandle );
		if ( scrollDownHintObject != NULL )
		{
			scrollDownHintObject->SetVisible(showBottomIndicator);
			scrollDownHintObject->SetColor( finalCol );
		}

		return  MSG_STATUS_ALIVE;
    }

	eMsgStatus TouchDown( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchDown();
		if ( ValidFoldersCount > 1 )
		{
			ScrollMgr.TouchDown();
		}

		TotalTouchDistance = 0.0f;
		StartTouchPosition.x = event.FloatValue.x;
		StartTouchPosition.y = event.FloatValue.y;

		LastInteractionTimeStamp = -1.0f;

		return MSG_STATUS_ALIVE;	// don't consume -- we're just listening
	}

	eMsgStatus TouchUp( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchUp();
		if ( ValidFoldersCount > 1 )
		{
			ScrollMgr.TouchUp();
		}

		bool allowPanelTouchUp = false;
		if ( TotalTouchDistance < MAX_TOUCH_DISTANCE_FOR_TOUCH_SQ )
		{
			allowPanelTouchUp = true;
		}

		FolderBrowser.SetAllowPanelTouchUp( allowPanelTouchUp );
		LastInteractionTimeStamp = vrapi_GetTimeInSeconds();

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus TouchRelative( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		if ( FolderBrowser.HasNoMedia() )
		{
			return MSG_STATUS_CONSUMED;
		}

		FolderBrowser.TouchRelative(event.FloatValue);
		Vector2f currentTouchPosition( event.FloatValue.x, event.FloatValue.y );
		TotalTouchDistance += ( currentTouchPosition - StartTouchPosition ).LengthSq();
		if ( ValidFoldersCount > 1 )
		{
			ScrollMgr.TouchRelative(event.FloatValue);
		}

		return MSG_STATUS_ALIVE;
	}

	eMsgStatus OnOpening( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		return MSG_STATUS_ALIVE;
	}

	// Return valid folder count, folder that has atleast one panel is considered as valid folder.
	int GetFolderBrowserValidFolderCount() const
	{
		int validFoldersCount = 0;
		const int folderCount = FolderBrowser.GetNumFolders();
		for ( int i = 0; i < folderCount; ++i )
		{
			const OvrFolderBrowser::FolderView * currentFolder = FolderBrowser.GetFolderView( i );
			if ( currentFolder && currentFolder->Panels.GetSizeI() > 0 )
			{
				++validFoldersCount;
			}
		}
		return validFoldersCount;
	}

	OvrFolderBrowser &	FolderBrowser;
	OvrScrollManager	ScrollMgr;
	Vector2f			StartTouchPosition;
	float				TotalTouchDistance;
	int					ValidFoldersCount;
	menuHandle_t 		FoldersRootHandle;
	menuHandle_t		ScrollDownHintHandle;
	menuHandle_t		ScrollUpHintHandle;
	double				LastInteractionTimeStamp;
};

const float OvrFolderBrowserRootComponent::ACTIVE_DEPTH_OFFSET = 3.4f;

//==============================================================
// OvrFolderRootComponent
// Component attached to the root object of each folder 
class OvrFolderRootComponent : public VRMenuComponent
{
public:
	OvrFolderRootComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::FolderView * folder )
		 : VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE )  )
		, FolderBrowser( folderBrowser )
		, FolderPtr( folder )
	{
		OVR_ASSERT( FolderPtr );
	}

private:
	// private assignment operator to prevent copying
	OvrFolderRootComponent &	operator = ( OvrFolderRootComponent & );

	virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
											VRMenuObject * self, VRMenuEvent const & event )
		{
			switch( event.EventType )
	        {
	            case VRMENU_EVENT_FRAME_UPDATE:
	                return Frame( guiSys, vrFrame, self, event );
	            default:
	                OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
	                return MSG_STATUS_ALIVE;
	        }
		}

    eMsgStatus Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
    {
		VRMenuObjectFlags_t flags = self->GetFlags();
		OVR_ASSERT( FolderPtr );
		if ( FolderBrowser.GetFolderView( FolderBrowser.GetActiveFolderIndex( guiSys ) ) != FolderPtr )
		{
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		else
		{
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		self->SetFlags( flags );

    	return MSG_STATUS_ALIVE;
    }

private:
	OvrFolderBrowser &	FolderBrowser;
	OvrFolderBrowser::FolderView * FolderPtr;
};

//==============================================================
// OvrFolderSwipeComponent
// Component that holds panel sub-objects and manages swipe left/right
class OvrFolderSwipeComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 58524;

	OvrFolderSwipeComponent( OvrFolderBrowser & folderBrowser, OvrFolderBrowser::FolderView * folder )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_TOUCH_RELATIVE | VRMENU_EVENT_TOUCH_DOWN | VRMENU_EVENT_TOUCH_UP | VRMENU_EVENT_SWIPE_COMPLETE )
		, FolderBrowser( folderBrowser )
		, ScrollMgr( HORIZONTAL_SCROLL )
		, FolderPtr( folder )
		, TouchDown( false )
    {
		OVR_ASSERT( FolderBrowser.GetCircumferencePanelSlots() > 0 );
		ScrollMgr.SetScrollPadding(0.5f);
    }

	virtual int	GetTypeId() const
	{
		return TYPE_ID;
	}

	bool Rotate( const OvrFolderBrowser::CategoryDirection direction )
	{
		static float const MAX_SPEED = 5.5f;
		switch ( direction )
		{
		case OvrFolderBrowser::MOVE_PANELS_LEFT:
			ScrollMgr.SetVelocity( MAX_SPEED );
			return true;
		case OvrFolderBrowser::MOVE_PANELS_RIGHT:
			ScrollMgr.SetVelocity( -MAX_SPEED );
			return true;
		default:
			return false;
		}
	}

	void SetRotationByRatio( const float ratio )
	{
		OVR_ASSERT( ratio >= 0.0f && ratio <= 1.0f );
		ScrollMgr.SetPosition( FolderPtr->MaxRotation * ratio );
		ScrollMgr.SetVelocity( 0.0f );
	}

	void SetRotationByIndex( const int panelIndex )
	{
		OVR_ASSERT( panelIndex >= 0 && panelIndex < ( *FolderPtr ).Panels.GetSizeI() );
		ScrollMgr.SetPosition( static_cast< float >( panelIndex ) );
	}

	void SetAccumulatedRotation( const float rot )	
	{ 
		ScrollMgr.SetPosition( rot );
		ScrollMgr.SetVelocity( 0.0f );
	}

	float GetAccumulatedRotation() const { return ScrollMgr.GetPosition();  }

	int CurrentPanelIndex() const { return static_cast<int>( nearbyintf( ScrollMgr.GetPosition() ) ); }

	int GetNextPanelIndex( const int step ) const
	{
		const OvrFolderBrowser::FolderView & folder = *FolderPtr;
		const int numPanels = folder.Panels.GetSizeI();

		int nextPanelIndex = CurrentPanelIndex() + step;
		if ( nextPanelIndex >= numPanels )
		{
			nextPanelIndex = 0;
		}
		else if ( nextPanelIndex < 0 )
		{
			nextPanelIndex = numPanels - 1;
		}

		return nextPanelIndex;
	}

private:
	// private assignment operator to prevent copying
	OvrFolderSwipeComponent &	operator = ( OvrFolderSwipeComponent & );

    virtual eMsgStatus Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
    {
    	OVR_ASSERT( FolderPtr );
    	bool const isActiveFolder = ( FolderPtr == FolderBrowser.GetFolderView( FolderBrowser.GetActiveFolderIndex( guiSys ) ) );
		if ( !isActiveFolder )
		{
			TouchDown = false;
		}

		OvrFolderBrowser::FolderView & folder = *FolderPtr;
		const int numPanels = folder.Panels.GetSizeI();
		eScrollDirectionLockType touchDirLock = FolderBrowser.GetTouchDirectionLock();
		eScrollDirectionLockType conrollerDirLock = FolderBrowser.GetControllerDirectionLock();

		ScrollMgr.SetMaxPosition( static_cast<float>( numPanels - 1 ) );
		ScrollMgr.SetRestrictedScrollingData( FolderBrowser.GetNumFolders() > 1, touchDirLock, conrollerDirLock );

		unsigned int controllerInput = 0;
		if ( isActiveFolder )
		{
			controllerInput = vrFrame.Input.buttonState;
			bool restrictedScrolling = FolderBrowser.GetNumFolders() > 1;
			if ( restrictedScrolling )
			{
				if ( touchDirLock == VERTICAL_LOCK )
				{
					if ( ScrollMgr.IsTouchIsDown() )
					{
						// Restricted scrolling enabled, but locked to vertical scrolling so lose the touch input
						ScrollMgr.TouchUp();
					}
				}

				if ( conrollerDirLock != HORIZONTAL_LOCK )
				{
					// Restricted scrolling enabled, but not locked to horizontal scrolling so lose the controller input
					controllerInput = 0;
				}
			}
		}
		else
		{
			if ( ScrollMgr.IsTouchIsDown() )
			{
				// While touch down this specific folder became inactive so perform touch up on this folder.
				ScrollMgr.TouchUp();
			}
		}

		ScrollMgr.Frame( vrFrame.DeltaSeconds, controllerInput );

    	if ( numPanels <= 0 )
    	{
    		return MSG_STATUS_ALIVE;
    	}

		const float position = ScrollMgr.GetPosition();
		
		// Send the position to the ScrollBar
		VRMenuObject * scrollBarObject = guiSys.GetVRMenuMgr().ToObject( folder.ScrollBarHandle );
		OVR_ASSERT( scrollBarObject != NULL );
		if ( isActiveFolder )
		{
			bool isHidden = false;
			if ( HIDE_SCROLLBAR_UNTIL_ITEM_COUNT > 0 )
			{
				isHidden = ScrollMgr.GetMaxPosition() - (float)( HIDE_SCROLLBAR_UNTIL_ITEM_COUNT - 1 )  < MATH_FLOAT_SMALLEST_NON_DENORMAL;
			}
			scrollBarObject->SetVisible( !isHidden );
		}
		OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByTypeName< OvrScrollBarComponent >();
		if ( scrollBar != NULL )
		{
			scrollBar->SetScrollFrac(  guiSys.GetVRMenuMgr(), scrollBarObject, position );
		}

		// hide the scrollbar if not active 
		const float velocity = ScrollMgr.GetVelocity();
		if ( ( numPanels > 1 && TouchDown ) || fabsf( velocity ) >= 0.01f )
		{
			scrollBar->SetScrollState( scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_IN );
		}
		else if ( !TouchDown && ( !isActiveFolder || fabsf( velocity ) < 0.01f ) )
		{			
			scrollBar->SetScrollState( scrollBarObject, OvrScrollBarComponent::SCROLL_STATE_FADE_OUT );
		}	

    	const float curRot = position * ( MATH_FLOAT_TWOPI / FolderBrowser.GetCircumferencePanelSlots() );
		Quatf rot( UP, curRot );
		rot.Normalize();
		self->SetLocalRotation( rot );

		// show or hide panels based on current position
		//
		// for rendering, we want the switch to occur between panels - hence nearbyint
		const int curPanelIndex = CurrentPanelIndex();
		const int extraPanels = FolderBrowser.GetNumSwipePanels() / 2;
		for ( int i = 0; i < numPanels; ++i )
		{
			OvrFolderBrowser::PanelView * panel = folder.Panels.At( i );
			OVR_ASSERT( panel->Handle.IsValid() );
			VRMenuObject * panelObject = guiSys.GetVRMenuMgr().ToObject( panel->Handle );
			OVR_ASSERT( panelObject );

			VRMenuObjectFlags_t flags = panelObject->GetFlags();
			if ( i >= curPanelIndex - extraPanels && i <= curPanelIndex + extraPanels )
			{
				flags &= ~( VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL );
				
				if ( !panel->Visible && folder.Visible )
				{
					panel->Visible = true;
					const OvrPanel_OnUp * panelUpComp = panelObject->GetComponentById< OvrPanel_OnUp >();
					if ( panelUpComp )
					{
						const OvrMetaDatum * panelData = panelUpComp->GetData();
						OVR_ASSERT( panelData );
						FolderBrowser.QueueAsyncThumbnailLoad( panelData, folder.FolderIndex, panel->Id );
					}
				}

				panelObject->SetFadeDirection( Vector3f( 0.0f ) );
				if ( i == curPanelIndex - extraPanels )
				{
					panelObject->SetFadeDirection( -RIGHT );
				}
				else if ( i == curPanelIndex + extraPanels )
				{
					panelObject->SetFadeDirection( RIGHT );
				}
			}
			else
			{				
				flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) | VRMENUOBJECT_DONT_HIT_ALL;
				if ( panel->Visible )
				{
					panel->Visible = false;
					panel->LoadDefaultThumbnail( guiSys, FolderBrowser.GetDefaultThumbnailTextureId(), FolderBrowser.GetThumbWidth(), FolderBrowser.GetThumbHeight() );
				}
			}
			panelObject->SetFlags( flags );
		}

		return MSG_STATUS_ALIVE;
    }

    eMsgStatus OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
    {
    	switch( event.EventType )
    	{
    		case VRMENU_EVENT_FRAME_UPDATE:
    			return Frame( guiSys, vrFrame, self, event );
    		case VRMENU_EVENT_TOUCH_DOWN:
				return OnTouchDown_Impl( guiSys, vrFrame, self, event );
    		case VRMENU_EVENT_TOUCH_UP:
				return OnTouchUp_Impl( guiSys, vrFrame, self, event );
    		case VRMENU_EVENT_SWIPE_COMPLETE:
    		{
    		    		VRMenuEvent upEvent = event;
    		    		upEvent.EventType = VRMENU_EVENT_TOUCH_UP;
    		    		return OnTouchUp_Impl( guiSys, vrFrame, self, upEvent );
    		}
    		case VRMENU_EVENT_TOUCH_RELATIVE:
    			ScrollMgr.TouchRelative( event.FloatValue );
    			break;
    		default:
    			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
    	}

    	return MSG_STATUS_ALIVE;
    }

	eMsgStatus OnTouchDown_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		ScrollMgr.TouchDown();
		TouchDown = true;
		return MSG_STATUS_ALIVE;
	}

	eMsgStatus OnTouchUp_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event )
	{
		ScrollMgr.TouchUp();
		TouchDown = false;
		return MSG_STATUS_ALIVE;
	}

private:
	OvrFolderBrowser &				FolderBrowser;
	OvrScrollManager				ScrollMgr;
	OvrFolderBrowser::FolderView *	FolderPtr;			// Correlates the folder browser component to the folder it belongs to
	bool							TouchDown;
};

//==============================
// OvrFolderBrowser
unsigned char * OvrFolderBrowser::ThumbPanelBG = NULL;

OvrFolderBrowser::OvrFolderBrowser(
		OvrGuiSys & guiSys,
		OvrMetaData & metaData,
		float panelWidth,
		float panelHeight,
		float radius_,
		unsigned numSwipePanels, 
		unsigned thumbWidth,
		unsigned thumbHeight )
	: VRMenu( MENU_NAME )
	, MediaCount( 0 )
	, PanelWidth( 0.0f )
	, PanelHeight( 0.0f )
	, ThumbWidth( thumbWidth )
	, ThumbHeight( thumbHeight )
	, PanelTextSpacingScale( 0.5f )
	, FolderTitleSpacingScale( 0.5f )
	, ScrollBarSpacingScale( 0.5f )
	, ScrollBarRadiusScale( 0.97f )
	, NumSwipePanels( numSwipePanels )
	, NoMedia( false )
	, AllowPanelTouchUp( false )
	, TextureCommands( 10000 )
	, BackgroundCommands( 10000 )
	, ControllerDirectionLock( NO_LOCK )
	, LastControllerInputTimeStamp( 0.0f )
	, IsTouchDownPosistionTracked( false )
	, TouchDirectionLocked( NO_LOCK )
	, ThumbnailLoadingThread( Thread::CreateParams( & ThumbnailThread, this, 128 * 1024, -1, Thread::NotRunning, Thread::BelowNormalPriority) )
{
	//  Load up thumbnail alpha from panel.tga
	if ( ThumbPanelBG == NULL )
	{
		void * 	buffer;
		int		bufferLength;

		const char * panel = NULL;

		if ( ThumbWidth == ThumbHeight )
		{
			panel = "res/raw/panel_square.tga";
		}
		else
		{
			panel = "res/raw/panel.tga";
		}

		ovr_ReadFileFromApplicationPackage( panel, bufferLength, buffer );

		int panelW = 0;
		int panelH = 0;
		ThumbPanelBG = stbi_load_from_memory( ( stbi_uc const * )buffer, bufferLength, &panelW, &panelH, NULL, 4 );

		OVR_ASSERT( ThumbPanelBG != 0 && panelW == ThumbWidth && panelH == ThumbHeight );
	}

	// load up the default panel textures once
	{
		const char * panelSrc[ 2 ] = {};

		if ( ThumbWidth == ThumbHeight )
		{
			panelSrc[ 0 ] = "apk:///res/raw/panel_square.tga";
			panelSrc[ 1 ] = "apk:///res/raw/panel_hi_square.tga";
		}
		else
		{
			panelSrc[ 0 ]  = "apk:///res/raw/panel.tga";
			panelSrc[ 1 ]  = "apk:///res/raw/panel_hi.tga";
		}
		
		for ( int t = 0; t < 2; ++t )
		{
			int width = 0;
			int height = 0;
			DefaultPanelTextureIds[ t ] = LoadTextureFromUri( guiSys.GetApp()->GetFileSys(), panelSrc[ t ], TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );
			OVR_ASSERT( DefaultPanelTextureIds[ t ] && ( width == ThumbWidth ) && ( height == ThumbHeight ) );

			BuildTextureMipmaps( DefaultPanelTextureIds[ t ] );
			MakeTextureTrilinear( DefaultPanelTextureIds[ t ] );
			MakeTextureClamped( DefaultPanelTextureIds[ t ] );
		}
	}

	if ( ! ThumbnailLoadingThread.Start() )
	{
		FAIL( "Thumbnail thread start failed." );
	}
	ThumbnailLoadingThread.SetThreadName( "FolderBrowser" );

	PanelWidth = panelWidth * VRMenuObject::DEFAULT_TEXEL_SCALE;
	PanelHeight = panelHeight * VRMenuObject::DEFAULT_TEXEL_SCALE;
	Radius = radius_;
	const float circumference = MATH_FLOAT_TWOPI * Radius;

	CircumferencePanelSlots = ( int )( floor( circumference / PanelWidth ) );
	VisiblePanelsArcAngle = (( float )( NumSwipePanels + 1 ) / CircumferencePanelSlots ) * MATH_FLOAT_TWOPI;

	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderBrowserRootComponent( *this ) );
	Init( guiSys, 0.0f, VRMenuFlags_t(), comps );
}

VRMenuComponent* OvrFolderBrowser::CreateFolderBrowserSwipeComponent( OvrFolderBrowser& folderBrowser, OvrFolderBrowser::FolderView * folder )
{
    return new OvrFolderSwipeComponent( *this, folder );
}

OvrFolderBrowser::~OvrFolderBrowser()
{
	LOG( "OvrFolderBrowser::~OvrFolderBrowser" );
	// Wake up thumbnail thread if needed and shut it down
	ThumbnailThreadMutex.DoLock();
	ThumbnailThreadState.SetState( THUMBNAIL_THREAD_SHUTDOWN );
	ThumbnailThreadCondition.NotifyAll();
	ThumbnailThreadMutex.Unlock();
	BackgroundCommands.ClearMessages();
	
	int numFolders = Folders.GetSizeI();
	for ( int i = 0; i < numFolders; ++i )
	{
		FolderView * folder = Folders.At( i );
		if ( folder )
		{
			folder->FreeThumbnailTextures( DefaultPanelTextureIds[ 0 ] );
			delete folder;
		}
	}

	if ( ThumbPanelBG != NULL )
	{
		free( ThumbPanelBG );
		ThumbPanelBG = NULL;
	}

	for ( int t = 0; t < 2; ++t )
	{
		DeleteTexture( DefaultPanelTextureIds[ t ] );
	}


	LOG( "OvrFolderBrowser::~OvrFolderBrowser COMPLETE" );
}

void OvrFolderBrowser::Frame_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame )
{
	// Check for thumbnail loads
	while ( 1 )
	{
		const char * cmd = TextureCommands.GetNextMessage();
		if ( !cmd )
		{
			break;
		}

		//LOG( "TextureCommands: %s", cmd );
		LoadThumbnailToTexture( guiSys, cmd );
		free( ( void * )cmd );
	}

	// --
	// Logic for restricted scrolling
	unsigned int controllerInput = vrFrame.Input.buttonState;
	bool rightPressed 	= ( controllerInput & ( BUTTON_LSTICK_RIGHT | BUTTON_DPAD_RIGHT ) ) != 0;
	bool leftPressed 	= ( controllerInput & ( BUTTON_LSTICK_LEFT | BUTTON_DPAD_LEFT ) ) != 0;
	bool downPressed 	= ( controllerInput & ( BUTTON_LSTICK_DOWN | BUTTON_DPAD_DOWN ) ) != 0;
	bool upPressed 		= ( controllerInput & ( BUTTON_LSTICK_UP | BUTTON_DPAD_UP ) ) != 0;

	if ( !( rightPressed || leftPressed || downPressed || upPressed ) )
	{
		if ( ControllerDirectionLock != NO_LOCK )
		{
			if ( vrapi_GetTimeInSeconds() - LastControllerInputTimeStamp >= CONTROLER_COOL_DOWN )
			{
				// Didn't receive any input for last 'CONTROLER_COOL_DOWN' seconds, so release direction lock
				ControllerDirectionLock = NO_LOCK;
			}
		}
	}
	else
	{
		if ( rightPressed || leftPressed )
		{
			if ( ControllerDirectionLock == VERTICAL_LOCK )
			{
				rightPressed = false;
				leftPressed = false;
			}
			else
			{
				if ( ControllerDirectionLock != HORIZONTAL_LOCK )
				{
					ControllerDirectionLock = HORIZONTAL_LOCK;
				}
				LastControllerInputTimeStamp = static_cast<float>( vrapi_GetTimeInSeconds() );
			}
		}

		if ( downPressed || upPressed )
		{
			if ( ControllerDirectionLock == HORIZONTAL_LOCK )
			{
				downPressed = false;
				upPressed = false;
			}
			else
			{
				if ( ControllerDirectionLock != VERTICAL_LOCK )
				{
					ControllerDirectionLock = VERTICAL_LOCK;
				}
				LastControllerInputTimeStamp = static_cast<float>( vrapi_GetTimeInSeconds() );
			}
		}
	}
}
    
VRMenuId_t OvrFolderBrowser::NextId()
{
    return VRMenuId_t( uniqueId.Get( 1 ) );
}

void OvrFolderBrowser::Open_Impl( OvrGuiSys & guiSys )
{
	if ( NoMedia )
	{
		return;
	}

	// Rebuild favorites if not empty 
	OnBrowserOpen( guiSys );

	// Wake up thumbnail thread
	ThumbnailThreadMutex.DoLock();
	ThumbnailThreadState.SetState( THUMBNAIL_THREAD_WORK );
	ThumbnailThreadCondition.NotifyAll(); 
	ThumbnailThreadMutex.Unlock();
}

void OvrFolderBrowser::Close_Impl( OvrGuiSys & guiSys )
{
	ThumbnailThreadState.SetState( THUMBNAIL_THREAD_PAUSE );
}

void OvrFolderBrowser::OneTimeInit( OvrGuiSys & guiSys )
{
	const OvrStoragePaths & storagePaths = guiSys.GetApp()->GetStoragePaths();
	storagePaths.GetPathIfValidPermission( EST_PRIMARY_EXTERNAL_STORAGE, EFT_CACHE, "", permissionFlags_t( PERMISSION_WRITE ), AppCachePath );
	OVR_ASSERT( !AppCachePath.IsEmpty() );

	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", ThumbSearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", ThumbSearchPaths );
	OVR_ASSERT( !ThumbSearchPaths.IsEmpty() );

	// move the root up to eye height
	OvrVRMenuMgr & menuManager = guiSys.GetVRMenuMgr();
	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
	OVR_ASSERT( root );
	if ( root != NULL )
	{
		Vector3f pos = root->GetLocalPosition();
		pos.y += EYE_HEIGHT_OFFSET;
		root->SetLocalPosition( pos );
	}

	Array< VRMenuComponent* > comps;
	Array< VRMenuObjectParms const * > parms;

	// Folders root folder
	FoldersRootId = VRMenuId_t( uniqueId.Get( 1 ) );
	VRMenuObjectParms foldersRootParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		"Folder Browser Folders",
		Posef(),
		Vector3f( 1.0f ),
		VRMenuFontParms(),
		FoldersRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t()
		);
	parms.PushBack( &foldersRootParms );
	AddItems( guiSys, parms, GetRootHandle(), false );
	parms.Clear();
	comps.Clear();

	// Build scroll up/down hints attached to root
	VRMenuId_t scrollSuggestionRootId( uniqueId.Get( 1 ) );

	VRMenuObjectParms scrollSuggestionParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		"scroll hints",
		Posef(),
		Vector3f( 1.0f ),
		VRMenuFontParms(),
		scrollSuggestionRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t()
		);
	parms.PushBack( &scrollSuggestionParms );
	AddItems( guiSys, parms, GetRootHandle(), false );
	parms.Clear();

	ScrollSuggestionRootHandle = root->ChildHandleForId( menuManager, scrollSuggestionRootId );
	OVR_ASSERT( ScrollSuggestionRootHandle.IsValid() );

	VRMenuId_t suggestionDownId( uniqueId.Get( 1 ) );
	VRMenuId_t suggestionUpId( uniqueId.Get( 1 ) );

	const Posef swipeDownPose( Quatf(), FWD * ( 0.33f * Radius ) + DOWN * PanelHeight * 0.5f );
	menuHandle_t scrollDownHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator( guiSys, this, ScrollSuggestionRootHandle, static_cast<int>( suggestionDownId.Get() ),
		"res/raw/swipe_suggestion_arrow_down.png", swipeDownPose, DOWN );

	const Posef swipeUpPose( Quatf(), FWD * ( 0.33f * Radius ) + UP * PanelHeight * 0.5f );
	menuHandle_t scrollUpHintHandle = OvrSwipeHintComponent::CreateSwipeSuggestionIndicator( guiSys, this, ScrollSuggestionRootHandle, static_cast<int>( suggestionUpId.Get() ),
		"res/raw/swipe_suggestion_arrow_up.png", swipeUpPose, UP );

	OvrFolderBrowserRootComponent * rootComp = root->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	menuHandle_t foldersRootHandle = root->ChildHandleForId( menuManager, FoldersRootId );
	OVR_ASSERT( foldersRootHandle.IsValid() );
	rootComp->SetFoldersRootHandle( foldersRootHandle );

	OVR_ASSERT( scrollUpHintHandle.IsValid() );
	rootComp->SetScrollDownHintHandle( scrollDownHintHandle );

	OVR_ASSERT( scrollDownHintHandle.IsValid() );
	rootComp->SetScrollUpHintHandle( scrollUpHintHandle );
}

void OvrFolderBrowser::BuildDirtyMenu( OvrGuiSys & guiSys, OvrMetaData & metaData )
{
	OvrVRMenuMgr & menuManager = guiSys.GetVRMenuMgr();

	Array< VRMenuComponent* > comps;
	Array< const VRMenuObjectParms * > parms;
	
	const Array< OvrMetaData::Category > & categories = metaData.GetCategories();
	const int numCategories = categories.GetSizeI();

	// load folders and position
	for ( int catIndex = 0; catIndex < numCategories; ++catIndex )
	{
		// Load a category to build swipe folder
		OvrMetaData::Category & currentCategory = metaData.GetCategory( catIndex );
		if ( currentCategory.Dirty ) // Only build if dirty
		{
			LOG( "Loading folder %i named %s", catIndex, currentCategory.CategoryTag.ToCStr() );
			FolderView * folder = GetFolderView( currentCategory.CategoryTag );

			// if building for the first time
			if ( folder == NULL )
			{
				// Create internal folder struct
				String localizedCategoryName;

				// Get localized tag (folder title)
				localizedCategoryName = GetCategoryTitle( guiSys, currentCategory.CategoryTag.ToCStr(), currentCategory.LocaleKey.ToCStr() );
				
				folder = CreateFolderView( localizedCategoryName, currentCategory.CategoryTag );
				Folders.PushBack( folder );

				const int folderIndex = Folders.GetSizeI() - 1; // Can't rely on catIndex due to duplicate folders
	
				BuildFolderView( guiSys, currentCategory, folder, metaData, FoldersRootId, folderIndex );

				UpdateFolderTitle( guiSys, folder );
				folder->MaxRotation = CalcFolderMaxRotation( folder );
			}
			else // already have this folder - rebuild it with the new metadata
			{
				// Find the index we already have
				int existingIndex = 0;
				for ( ; existingIndex < numCategories; ++existingIndex )
				{
					// Load the category to build swipe folder
					const OvrMetaData::Category & cat = metaData.GetCategory( existingIndex );
					if ( cat.CategoryTag == currentCategory.CategoryTag )
					{
						Array< const OvrMetaDatum * > folderMetaData;
						if ( metaData.GetMetaData( cat, folderMetaData ) )
						{
							RebuildFolderView( guiSys, metaData, existingIndex, folderMetaData );
						}
						else
						{
							LOG( "Failed to get any metaData for folder %i named %s", existingIndex, currentCategory.CategoryTag.ToCStr() );
						}
						break;
					}
				}
			}

			folder->FolderIndex = catIndex;

			currentCategory.Dirty = false;

			// Set up initial positions - 0 in center, the rest ascending in order below it
			MediaCount += folder->Panels.GetSizeI();
			VRMenuObject * folderObject = menuManager.ToObject( folder->Handle );
			OVR_ASSERT( folderObject != NULL );
			folderObject->SetLocalPosition( ( DOWN * PanelHeight * static_cast<float>( catIndex ) ) + folderObject->GetLocalPosition() );
		}
	}

	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f );

	// Show no media menu if no media found
	if ( MediaCount == 0 )
	{
		String title;
		String imageFile;
		String message;
		OnMediaNotFound( guiSys, title, imageFile, message );

		// Create a folder if we didn't create at least one to display no media info
		if ( Folders.GetSizeI() < 1 )
		{
			const String noMediaTag( "No Media" );
			const_cast< OvrMetaData & >( metaData ).AddCategory( noMediaTag );
			OvrMetaData::Category & noMediaCategory = metaData.GetCategory( 0 );
			FolderView * noMediaView = new FolderView( noMediaTag, noMediaTag );
			BuildFolderView( guiSys, noMediaCategory, noMediaView, metaData, FoldersRootId, 0 );
			Folders.PushBack( noMediaView );
		}

		// Set title 
		const FolderView * folder = GetFolderView( 0 );
		OVR_ASSERT ( folder != NULL );
		VRMenuObject * folderTitleObject = menuManager.ToObject( folder->TitleHandle );
		OVR_ASSERT( folderTitleObject != NULL );
		folderTitleObject->SetText( title.ToCStr() );
		folderTitleObject->SetVisible( true );

		// Add no media panel
		const Vector3f dir( FWD );
		const Posef panelPose( Quatf(), dir * Radius );
		const Vector3f panelScale( 1.0f );
		const Posef textPose( Quatf(), Vector3f( 0.0f, -0.3f, 0.0f ) );

		VRMenuSurfaceParms panelSurfParms( "panelSurface",
			imageFile.ToCStr(), SURFACE_TEXTURE_DIFFUSE,
			NULL, SURFACE_TEXTURE_MAX,
			NULL, SURFACE_TEXTURE_MAX );

		VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_STATIC, Array< VRMenuComponent* >(),
			panelSurfParms, message.ToCStr(), panelPose, panelScale, textPose, Vector3f( 1.3f ), fontParms, VRMenuId_t(),
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		parms.PushBack( p );
		AddItems( guiSys, parms, folder->SwipeHandle, true ); // PARENT: folder.TitleRootHandle
		parms.Clear();

		NoMedia = true;

		// Hide scroll hints while no media
		VRMenuObject * scrollHintRootObject = menuManager.ToObject( ScrollSuggestionRootHandle );
		OVR_ASSERT( scrollHintRootObject  );
		scrollHintRootObject->SetVisible( false );
	}
}

void OvrFolderBrowser::BuildFolderView( OvrGuiSys & guiSys, OvrMetaData::Category & category, FolderView * const folder, const OvrMetaData & metaData, VRMenuId_t foldersRootId, int folderIndex )
{
	OVR_ASSERT( folder );

	OvrVRMenuMgr & menuManager = guiSys.GetVRMenuMgr();

	Array< const VRMenuObjectParms * > parms;
	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 1.0f );

	const int numPanels = category.DatumIndicies.GetSizeI();

	VRMenuObject * root = menuManager.ToObject( GetRootHandle() );
	OVR_ASSERT( root != NULL );
	menuHandle_t foldersRootHandle = root ->ChildHandleForId( menuManager, foldersRootId );

	// Create OvrFolderRootComponent for folder root
	const VRMenuId_t folderId( uniqueId.Get( 1 ) );
	LOG( "Building Folder %s id: %lld with %d panels", category.CategoryTag.ToCStr(), folderId.Get(), numPanels );
	Array< VRMenuComponent* > comps;
	comps.PushBack( new OvrFolderRootComponent( *this, folder ) );
	VRMenuObjectParms folderParms(
		VRMENU_CONTAINER,
		comps,
		VRMenuSurfaceParms(),
		( folder->LocalizedName + " root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &folderParms );
	AddItems( guiSys, parms, foldersRootHandle, false ); // PARENT: Root
	parms.Clear();

	// grab the folder handle and make sure it was created
	folder->Handle = HandleForId( menuManager, folderId );
	VRMenuObject * folderObject = menuManager.ToObject( folder->Handle );
	OVR_ASSERT( folderObject != NULL );

	// Add horizontal scrollbar to folder
	Posef scrollBarPose( Quatf(), FWD * Radius * ScrollBarRadiusScale );

	// Create unique ids for the scrollbar objects
	const VRMenuId_t scrollRootId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollXFormId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollBaseId( uniqueId.Get( 1 ) );
	const VRMenuId_t scrollThumbId( uniqueId.Get( 1 ) );

	// Set the border of the thumb image for 9-slicing
	const Vector4f scrollThumbBorder( 0.0f, 0.0f, 0.0f, 0.0f );	
	const Vector3f xFormPos = DOWN * static_cast<float>( ThumbHeight ) * VRMenuObject::DEFAULT_TEXEL_SCALE * ScrollBarSpacingScale;

	// Build the scrollbar
	OvrScrollBarComponent::GetScrollBarParms( guiSys, *this, SCROLL_BAR_LENGTH, folderId, scrollRootId, scrollXFormId, scrollBaseId, scrollThumbId,
		scrollBarPose, Posef( Quatf(), xFormPos ), 0, numPanels, false, scrollThumbBorder, parms );	
	AddItems( guiSys, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// Cache off the handle and verify successful creation
	folder->ScrollBarHandle = folderObject->ChildHandleForId( menuManager, scrollRootId );
	VRMenuObject * scrollBarObject = menuManager.ToObject( folder->ScrollBarHandle );
	OVR_ASSERT( scrollBarObject != NULL );
	OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByTypeName< OvrScrollBarComponent >();
	if ( scrollBar != NULL )
	{
		scrollBar->UpdateScrollBar( menuManager, scrollBarObject, numPanels );
		scrollBar->SetScrollFrac( menuManager, scrollBarObject, 0.0f );
		scrollBar->SetBaseColor( menuManager, scrollBarObject, Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ) );

		// Hide the scrollbar
		VRMenuObjectFlags_t flags = scrollBarObject->GetFlags();
		scrollBarObject->SetFlags( flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER ) );
	}

  	// Add OvrFolderSwipeComponent to folder - manages panel swiping
	VRMenuId_t swipeFolderId( uniqueId.Get( 1 ) );
	Array< VRMenuComponent* > swipeComps;
	swipeComps.PushBack( CreateFolderBrowserSwipeComponent( *this, folder ) );
	VRMenuObjectParms swipeParms(
		VRMENU_CONTAINER, 
		swipeComps,
		VRMenuSurfaceParms(), 
		( folder->LocalizedName + " swipe" ).ToCStr(),
		Posef(), 
		Vector3f( 1.0f ), 
		fontParms,
		swipeFolderId,
		VRMenuObjectFlags_t( VRMENUOBJECT_NO_GAZE_HILIGHT ), 
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) 
		);
	parms.PushBack( &swipeParms );
	AddItems( guiSys, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// grab the SwipeHandle handle and make sure it was created
	folder->SwipeHandle = folderObject->ChildHandleForId( menuManager, swipeFolderId );
	VRMenuObject * swipeObject = menuManager.ToObject( folder->SwipeHandle );
	OVR_ASSERT( swipeObject != NULL );
	
	// build a collision primitive that encompasses all of the panels for a raw (including the empty space between them)
	// so that we can always send our swipe messages to the correct row based on gaze.
	Array< Vector3f > vertices( CircumferencePanelSlots * 2 );
	Array< TriangleIndex > indices( CircumferencePanelSlots * 6 );
	int curIndex = 0;
	TriangleIndex curVertex = 0;
	for ( int i = 0; i < CircumferencePanelSlots; ++i )
	{
		float theta = ( i * MATH_FLOAT_TWOPI ) / CircumferencePanelSlots;
		float x = cosf( theta ) * Radius * 1.05f;
		float z = sinf( theta ) * Radius * 1.05f;
		Vector3f topVert( x, PanelHeight * 0.5f, z );
		Vector3f bottomVert( x, PanelHeight * -0.5f, z );

		vertices[curVertex + 0] = topVert;
		vertices[curVertex + 1] = bottomVert;
		if ( i > 0 )	// only set up indices if we already have one column's vertices
		{
			// first tri
			indices[curIndex + 0] = curVertex + 1;
			indices[curIndex + 1] = curVertex + 0;
			indices[curIndex + 2] = curVertex - 1;
			// second tri
			indices[curIndex + 3] = curVertex + 0;
			indices[curIndex + 4] = curVertex - 2;
			indices[curIndex + 5] = curVertex - 1;
			curIndex += 6;
		}

		curVertex += 2;
	}
	// connect the last vertices to the first vertices for the last sector
	indices[curIndex + 0] = 1;
	indices[curIndex + 1] = 0;
	indices[curIndex + 2] = curVertex - 1;
	indices[curIndex + 3] = 0;
	indices[curIndex + 4] = curVertex - 2;
	indices[curIndex + 5] = curVertex - 1;

	// create and set the collision primitive on the swipe object
	OvrTriCollisionPrimitive * cp = new OvrTriCollisionPrimitive( vertices, indices, Array< Vector2f >(), ContentFlags_t( CONTENT_SOLID ) );
	swipeObject->SetCollisionPrimitive( cp );

	if ( !category.DatumIndicies.IsEmpty() )
	{
        // Grab panel parms 
		LoadFolderViewPanels( guiSys, metaData, category, folderIndex, *folder, parms );
		if ( !parms.IsEmpty() )
		{
			// Add panels to swipehandle
			AddItems( guiSys, parms, folder->SwipeHandle, false );
			DeletePointerArray( parms );
			parms.Clear();
		}

		// Assign handles to panels
		for ( int i = 0; i < folder->Panels.GetSizeI(); ++i )
		{
			PanelView* panel = folder->Panels.At( i );
			panel->Handle = swipeObject->ChildHandleForId( menuManager, panel->MenuId );
		}
	}

	// Folder title
	VRMenuId_t folderTitleRootId( uniqueId.Get( 1 ) );
	VRMenuObjectParms titleRootParms(
		VRMENU_CONTAINER,
		Array< VRMenuComponent* >(),
		VRMenuSurfaceParms(),
		( folder->LocalizedName + " title root" ).ToCStr(),
		Posef(),
		Vector3f( 1.0f ),
		fontParms,
		folderTitleRootId,
		VRMenuObjectFlags_t(),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION )
		);
	parms.PushBack( &titleRootParms );
	AddItems( guiSys, parms, folder->Handle, false ); // PARENT: folder->Handle
	parms.Clear();

	// grab the title root handle and make sure it was created
	folder->TitleRootHandle = folderObject->ChildHandleForId( menuManager, folderTitleRootId );
	VRMenuObject * folderTitleRootObject = menuManager.ToObject( folder->TitleRootHandle );
	OVR_UNUSED( folderTitleRootObject );
	OVR_ASSERT( folderTitleRootObject != NULL );

	VRMenuId_t folderTitleId( uniqueId.Get( 1 ) );
	Posef titlePose( Quatf(), FWD * Radius + UP * PanelHeight * FolderTitleSpacingScale );
	VRMenuObjectParms titleParms( 
		VRMENU_STATIC, 
		Array< VRMenuComponent* >(),
		VRMenuSurfaceParms(),
		"no title",
		titlePose, 
		Vector3f( 1.25f ), 
		fontParms,
		folderTitleId,
		VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ), 
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &titleParms );
	AddItems( guiSys, parms, folder->TitleRootHandle, true ); // PARENT: folder->TitleRootHandle
	parms.Clear();

	// grab folder title handle and make sure it was created
	folder->TitleHandle = folderTitleRootObject->ChildHandleForId( menuManager, folderTitleId );
	VRMenuObject * folderTitleObject = menuManager.ToObject( folder->TitleHandle );
	OVR_UNUSED( folderTitleObject );
	OVR_ASSERT( folderTitleObject != NULL );
}

void OvrFolderBrowser::RebuildFolderView( OvrGuiSys & guiSys, OvrMetaData & metaData, const int folderIndex, const Array< const OvrMetaDatum * > & data )
{
	if ( folderIndex >= 0 && Folders.GetSizeI() > folderIndex )
	{		
		OvrVRMenuMgr & menuManager = guiSys.GetVRMenuMgr();

		FolderView * folder = GetFolderView( folderIndex );
		if ( folder == NULL )
		{
			LOG( "OvrFolderBrowser::RebuildFolder failed to Folder for folderIndex %d", folderIndex );
			return;
		}

		VRMenuObject * swipeObject = menuManager.ToObject( folder->SwipeHandle );
		OVR_ASSERT( swipeObject );

		swipeObject->FreeChildren( menuManager );
		folder->Panels.Clear();

		const int numPanels = data.GetSizeI();
		Array< const VRMenuObjectParms * > outParms;
		Array< int > newDatumIndicies;
		for ( int panelIndex = 0; panelIndex < numPanels; ++panelIndex )
		{
			const OvrMetaDatum * panelData = data.At( panelIndex );
			if ( panelData )
			{
				AddPanelToFolder( guiSys, data.At( panelIndex ), folderIndex, *folder, outParms );
				if ( !outParms.IsEmpty() )
				{
					AddItems( guiSys, outParms, folder->SwipeHandle, false );
					DeletePointerArray( outParms );
					outParms.Clear();
				}

				// Assign handle to panel
				PanelView* panel = folder->Panels.At( panelIndex );
				panel->Handle = swipeObject->ChildHandleForId( menuManager, panel->MenuId );
				newDatumIndicies.PushBack( panelData->Id );
			}
		}

		metaData.SetCategoryDatumIndicies( folderIndex, newDatumIndicies );

		OvrFolderSwipeComponent * swipeComp = swipeObject->GetComponentById< OvrFolderSwipeComponent >();
		OVR_ASSERT( swipeComp );		
		UpdateFolderTitle( guiSys, folder );

		// Recalculate accumulated rotation in the swipe component based on ratio of where user left off before adding/removing favorites
		const float currentMaxRotation = folder->MaxRotation > 0.0f ? folder->MaxRotation : 1.0f;
		const float positionRatio = Alg::Clamp( swipeComp->GetAccumulatedRotation() / currentMaxRotation, 0.0f, 1.0f );
		folder->MaxRotation = CalcFolderMaxRotation( folder );
		swipeComp->SetAccumulatedRotation( folder->MaxRotation * positionRatio );

		// Update the scroll bar on new element count
		VRMenuObject * scrollBarObject = menuManager.ToObject( folder->ScrollBarHandle );
		if ( scrollBarObject != NULL )
		{
			OvrScrollBarComponent * scrollBar = scrollBarObject->GetComponentByTypeName< OvrScrollBarComponent >();
			if ( scrollBar != NULL )
			{
				scrollBar->UpdateScrollBar( menuManager, scrollBarObject, numPanels );
			}
		}
	}
}

void OvrFolderBrowser::UpdateFolderTitle( OvrGuiSys & guiSys, const FolderView * folder )
{
	if ( folder != NULL )
	{
		const int numPanels = folder->Panels.GetSizeI();

		String folderTitle = folder->LocalizedName;
		VRMenuObject * folderTitleObject = guiSys.GetVRMenuMgr().ToObject( folder->TitleHandle );
		OVR_ASSERT( folderTitleObject != NULL );
		folderTitleObject->SetText( folderTitle.ToCStr() );

		VRMenuObjectFlags_t flags = folderTitleObject->GetFlags();
		if ( numPanels > 0 )
		{
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			flags &= ~VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}
		else
		{
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_RENDER );
			flags |= VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL );
		}

		folderTitleObject->SetFlags( flags );
	}
}

threadReturn_t OvrFolderBrowser::ThumbnailThread( Thread *thread, void * v )
{
	thread->SetThreadName( "FolderBrowser" );

	OvrFolderBrowser * folderBrowser = (OvrFolderBrowser *)v;

	for ( ;; )
	{
		folderBrowser->BackgroundCommands.SleepUntilMessage();

		switch ( folderBrowser->ThumbnailThreadState.GetState() )
		{
		case THUMBNAIL_THREAD_WORK:
			break;
		case THUMBNAIL_THREAD_PAUSE:
		{
			LOG( "OvrFolderBrowser::ThumbnailThread PAUSED" );
			folderBrowser->ThumbnailThreadMutex.DoLock();
			while ( folderBrowser->ThumbnailThreadState.GetState() == THUMBNAIL_THREAD_PAUSE )
			{
				folderBrowser->ThumbnailThreadCondition.Wait( &folderBrowser->ThumbnailThreadMutex );
			}
			folderBrowser->ThumbnailThreadMutex.Unlock();
			continue;
		}
		case THUMBNAIL_THREAD_SHUTDOWN:
			LOG( "OvrFolderBrowser::ThumbnailThread shutting down" );
			return NULL;
		}

		const char * msg = folderBrowser->BackgroundCommands.GetNextMessage();
		LOG( "BackgroundCommands: %s", msg );

		if ( MatchesHead( "load ", msg ) )
		{
			int folderId = -1;
			int panelId = -1;

			sscanf( msg, "load %i %i", &folderId, &panelId );
			OVR_ASSERT( folderId >= 0 && panelId >= 0 );

			// Do we still need to load this?
			const FolderView * folder = folderBrowser->GetFolderView( folderId );
			// ThumbnailsLoaded is set to false when the category goes out of view - do not load the thumbnail
			if ( folder && folder->Visible ) 
			{
				if ( panelId >= 0 && panelId < folder->Panels.GetSizeI() )
				{
					const PanelView * panel = folder->Panels.At( panelId );
					if ( panel && panel->Visible )
					{
						const char * fileName = strstr( msg, ":" ) + 1;

						const String fullPath( fileName );

						int		width;
						int		height;
						unsigned char * data = folderBrowser->LoadThumbnail( fileName, width, height );
						if ( data != NULL )
						{
							folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
								folderId, panelId, data, width, height );
						}
						else
						{
							WARN( "Thumbnail load fail for: %s", fileName );
						}
					}
				}
			}

		}
		else if ( MatchesHead( "httpThumb", msg ) )
		{
			int folderId = -1;
			int panelId = -1;
			char panoUrl[ 1024 ] = {};
			char cacheDestination[ 1024 ] = {};

			sscanf( msg, "httpThumb %s %s %d %d", panoUrl, cacheDestination, &folderId, &panelId );

			// Do we still need to load this?
			const FolderView * folder = folderBrowser->GetFolderView( folderId );
			// ThumbnailsLoaded is set to false when the category goes out of view - do not load the thumbnail
			if ( folder && folder->Visible )
			{
				if ( panelId >= 0 && panelId < folder->Panels.GetSizeI() )
				{
					const PanelView * panel = folder->Panels.At( panelId );
					if ( panel && panel->Visible )
					{
						int		width;
						int		height;
						unsigned char * data = folderBrowser->RetrieveRemoteThumbnail(
							panoUrl,
							cacheDestination,
							folderId,
							panelId,
							width,
							height );

						if ( data != NULL )
						{
							folderBrowser->TextureCommands.PostPrintf( "thumb %i %i %p %i %i",
								folderId, panelId, data, width, height );
						}
						else
						{
							WARN( "Thumbnail download fail for: %s", panoUrl );
						}
					}
				}
			}
		}
		else
		{
			LOG( "OvrFolderBrowser::ThumbnailThread received unhandled message: %s", msg );
			OVR_ASSERT( false );
		}

		free( ( void * )msg );
	}
}

// THUMBFIX: call this to load final thumbnail onto the panel
void OvrFolderBrowser::LoadThumbnailToTexture( OvrGuiSys & guiSys, const char * thumbnailCommand )
{	
	int folderId;
	int panelId;
	unsigned char * data;
	int width;
	int height;

	sscanf( thumbnailCommand, "thumb %i %i %p %i %i", &folderId, &panelId, &data, &width, &height );
	if ( folderId < 0 || panelId < 0 )
	{
		delete data;
		return;
	}

	FolderView * folder = GetFolderView( folderId );
	if ( folder == NULL )
	{
		WARN( "OvrFolderBrowser::LoadThumbnailToTexture failed to find FolderView at %i", folderId );
		free( data );
		return;
	}

	Array<PanelView*> * panels = &folder->Panels;
	if ( panels == NULL )
	{
		WARN( "OvrFolderBrowser::LoadThumbnailToTexture failed to get panels array from folder" );
		free( data );
		return;
	}

	PanelView * panel = NULL;

	// find panel using panelId
	const int numPanels = panels->GetSizeI();
	for ( int index = 0; index < numPanels; ++index )
	{
		PanelView* currentPanel = panels->At( index );
		if ( currentPanel->Id == panelId )
		{
			panel = currentPanel;
			break;
		}
	}

	if ( panel == NULL ) // Panel not found as it was moved. Delete data and bail
	{
		WARN( "OvrFolderBrowser::LoadThumbnailToTexture failed to find panel id %d in folder %d", panelId, folderId );
		free( data );
		return;
	}

	if ( !ApplyThumbAntialiasing( data, width, height ) )
	{
		WARN( "OvrFolderBrowser::LoadThumbnailToTexture Failed to apply AA to %s", thumbnailCommand );
	}

	// Grab the Panel from VRMenu
	menuHandle_t thumbHandle = panel->GetThumbnailHandle();
	VRMenuObject * panelObject = guiSys.GetVRMenuMgr().ToObject( thumbHandle );
	OVR_ASSERT( panelObject );

	GlTexture texId = LoadRGBATextureFromMemory( data, width, height, true /* srgb */ );
	free( data );

	if ( texId )
	{
		panelObject->SetSurfaceTexture( 0, 0, SURFACE_TEXTURE_DIFFUSE,
			texId, ThumbWidth, ThumbHeight );

		panel->TextureId = texId;

		BuildTextureMipmaps( texId );
		MakeTextureTrilinear( texId );
		MakeTextureClamped( texId );
	}
}

void OvrFolderBrowser::LoadFolderViewPanels( OvrGuiSys & guiSys, const OvrMetaData & metaData, const OvrMetaData::Category & category, const int folderIndex, FolderView & folder,
		Array< VRMenuObjectParms const * >& outParms )
{
	// Build panels 
	Array< const OvrMetaDatum * > categoryPanos;
	metaData.GetMetaData( category, categoryPanos );
	const int numPanos = categoryPanos.GetSizeI();
	LOG( "Building %d panels for %s", numPanos, category.CategoryTag.ToCStr() );
	for ( int panoIndex = 0; panoIndex < numPanos; panoIndex++ )
	{
		AddPanelToFolder( guiSys, const_cast< OvrMetaDatum * const >( categoryPanos.At( panoIndex ) ), folderIndex, folder, outParms );
	}
}

void OvrFolderBrowser::AddPanelToFolder( OvrGuiSys & guiSys, const OvrMetaDatum * panoData, const int folderIndex, FolderView & folder, Array< VRMenuObjectParms const * >& outParms )
{
	OVR_ASSERT( panoData );

	const int panelIndex = folder.Panels.GetSizeI();
	PanelView * panel = CreatePanelView( panelIndex );
    panel->MenuId = NextId();

	// This is the only place these indices are ever set. 
	panoData->FolderIndex = folderIndex;
	panoData->PanelId = panelIndex;

	// Panel placement - based on index which determines position within the circumference
	const float factor = ( float )panelIndex / ( float )CircumferencePanelSlots;
	Quatf rot( DOWN, ( MATH_FLOAT_TWOPI * factor ) );
	Vector3f dir( FWD * rot );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	VRMenuObject * swipeObject = guiSys.GetVRMenuMgr().ToObject( folder.SwipeHandle );
	OVR_ASSERT( swipeObject != NULL );

	// Add panel VRMenuObjectParms to be added to our menu
	AddPanelMenuObject( guiSys, panoData, swipeObject, panel->MenuId, &folder, folderIndex, panel, panelPose, panelScale, outParms );

	folder.Panels.PushBack( panel );
}

void OvrFolderBrowser::QueueAsyncThumbnailLoad( const OvrMetaDatum * panoData, const int folderIndex, const int panelId )
{
	// Verify input
	if ( panoData == NULL )
	{
		WARN( "OvrFolderBrowser::QueueAsyncThumbnailLoad error - NULL panoData" );
		return;
	}

	if ( folderIndex < 0 || folderIndex >= Folders.GetSizeI() )
	{
		WARN( "OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid folder index: %d", folderIndex );
		return;
	}

	FolderView * folder = GetFolderView( folderIndex );
	if ( folder == NULL )
	{
		WARN( "OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid folder index: %d", folderIndex );
		return;
	}
	else
	{
		const int numPanels = folder->Panels.GetSizeI();
		if ( panelId < 0 || panelId >= numPanels )
		{
			WARN( "OvrFolderBrowser::QueueAsyncThumbnailLoad error invalid panel id: %d", panelId );
			return;
		}
	}
	
	// Create or load thumbnail - request built up here to be processed ThumbnailThread
	const String panoUrl = ThumbUrl( panoData );
	const String thumbName = ThumbName( panoUrl );
	String finalThumb;
	char relativeThumbPath[ 1024 ];
	ToRelativePath( ThumbSearchPaths, panoUrl.ToCStr(), relativeThumbPath, sizeof( relativeThumbPath ) );

	char appCacheThumbPath[ 1024 ];
	OVR_sprintf( appCacheThumbPath, sizeof( appCacheThumbPath ), "%s%s", AppCachePath.ToCStr(), ThumbName( relativeThumbPath ).ToCStr() );

	// if this url doesn't exist locally
	if ( !FileExists( panoUrl.ToCStr() ) )
	{
		// Check app cache to see if we already downloaded it
		if ( FileExists( appCacheThumbPath ) )
		{
			finalThumb = appCacheThumbPath;
		}
		else // download and cache it 
		{
			BackgroundCommands.PostPrintf( "httpThumb %s %s %d %d", panoUrl.ToCStr(), appCacheThumbPath, folderIndex, panelId );
			return;
		}
	}
	
	if ( finalThumb.IsEmpty() )
	{
		// Try search paths next to image for retail photos
		if ( !GetFullPath( ThumbSearchPaths, thumbName.ToCStr(), finalThumb ) )
		{
			// Try app cache for cached user pano thumbs
			if ( FileExists( appCacheThumbPath ) )
			{
				finalThumb = appCacheThumbPath;
			}
			else
			{
				const String altThumbPath = AlternateThumbName( panoUrl );
				if ( altThumbPath.IsEmpty() || !GetFullPath( ThumbSearchPaths, altThumbPath.ToCStr(), finalThumb ) )
				{
					int pathLen = panoUrl.GetLengthI();
					if ( pathLen > 2 && OVR_stricmp( panoUrl.ToCStr() + pathLen - 2, ".x" ) == 0 )
					{
						WARN( "Thumbnails cannot be generated from encrypted images." );
						return; // No thumb & can't create 
					}
				}
			}
		}
	}

	if ( !finalThumb.IsEmpty() )
	{
		char cmd[ 1024 ];
		OVR_sprintf( cmd, 1024, "load %i %i:%s", folderIndex, panelId, finalThumb.ToCStr() );
		LOG( "Thumb cmd: %s", cmd );
		BackgroundCommands.PostString( cmd );
	}
	else
	{
		WARN( "Failed to find thumbnail for %s - will be created when selected", panoUrl.ToCStr() );
	}
}

void OvrFolderBrowser::AddPanelMenuObject(
		OvrGuiSys & guiSys,
		const OvrMetaDatum * panoData,
		VRMenuObject * parent,
		VRMenuId_t id,
		FolderView * folder,
		const int folderIndex,
		PanelView * panel,
		Posef panelPose,
		Vector3f panelScale,
		Array< VRMenuObjectParms const * >& outParms )
{
	// Load a panel
	Array< VRMenuComponent* > panelComps;
	panelComps.PushBack( new OvrPanel_OnUp( this, panoData, folderIndex, panel->Id ) );
	panelComps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.25f, Vector4f( 0.f ) ) );

	// single-pass multitexture
	VRMenuSurfaceParms panelSurfParms(
			"panelSurface",
			DefaultPanelTextureIds[ 0 ],
			ThumbWidth,
			ThumbHeight,
			SURFACE_TEXTURE_DIFFUSE,
			DefaultPanelTextureIds[ 1 ],
			ThumbWidth,
			ThumbHeight,
			SURFACE_TEXTURE_DIFFUSE,
			0,
			0,
			0,
			SURFACE_TEXTURE_MAX );

	// Title text placed below thumbnail
	const Posef textPose( Quatf(), Vector3f( 0.0f, -PanelHeight * PanelTextSpacingScale, 0.0f ) );

	String panelTitle = GetPanelTitle( guiSys, *panoData );
	// GetPanelTitle should ALWAYS return a localized string
	//guiSys.GetApp()->GetLocale()->GetString( panoTitle, panoTitle, panelTitle );

	const VRMenuFontParms fontParms( HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, true, 0.525f, 0.45f, 0.5f );
	VRMenuObjectParms * p = new VRMenuObjectParms(
			VRMENU_BUTTON,
			panelComps,
			panelSurfParms,
			panelTitle.ToCStr(),
			panelPose,
			panelScale,
			textPose,
			Vector3f( 1.0f ),
			fontParms,
			id,
			VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ) | VRMENUOBJECT_RENDER_HIERARCHY_ORDER,
			VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
    outParms.PushBack( p );	
}

bool OvrFolderBrowser::ApplyThumbAntialiasing( unsigned char * inOutBuffer, int width, int height ) const
{
	if ( inOutBuffer != NULL )
	{
		if ( ThumbPanelBG != NULL )
		{
			const unsigned thumbWidth = GetThumbWidth();
			const unsigned thumbHeight = GetThumbHeight();

			const int numBytes = width * height * 4;
			const int thumbPanelBytes = thumbWidth * thumbHeight * 4;
			if ( numBytes != thumbPanelBytes )
			{
				WARN( "OvrFolderBrowser::ApplyAA - Thumbnail image is the wrong size!" );
			}
			else
			{
				// Apply alpha from vrappframework/res/raw to alpha channel for anti-aliasing
				for ( int i = 3; i < thumbPanelBytes; i += 4 )
				{
					inOutBuffer[ i ] = ThumbPanelBG[ i ];
				}
				return true;
			}
		}
	}
	return false;
}

const OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( int index ) const
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	if ( index < 0 || index >= Folders.GetSizeI() )
	{
		return NULL;
	}

	return Folders.At( index );
}

OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( int index )
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	if ( index < 0 || index >= Folders.GetSizeI() )
	{
		return NULL;
	}

	return Folders.At( index );
}

OvrFolderBrowser::FolderView * OvrFolderBrowser::GetFolderView( const String & categoryTag )
{
	if ( Folders.IsEmpty() )
	{
		return NULL;
	}

	for ( int i = 0; i < Folders.GetSizeI(); ++i )
	{
		FolderView * currentFolder = Folders.At( i );
		if ( currentFolder->CategoryTag == categoryTag )
		{
			return currentFolder;
		}
	}

	return NULL;
}

bool OvrFolderBrowser::RotateCategory( OvrGuiSys & guiSys, const CategoryDirection direction )
{
	OvrFolderSwipeComponent * swipeComp = const_cast< OvrFolderSwipeComponent * >( GetSwipeComponentForActiveFolder( guiSys ) );
	return swipeComp->Rotate( direction );
}

void OvrFolderBrowser::SetCategoryRotation( OvrGuiSys & guiSys, const int folderIndex, const int panelIndex )
{
	OVR_ASSERT( folderIndex >= 0 && folderIndex < Folders.GetSizeI() );
	const FolderView * folder = GetFolderView( folderIndex );
	if ( folder != NULL )
	{
		VRMenuObject * swipe = guiSys.GetVRMenuMgr().ToObject( folder->SwipeHandle );
		OVR_ASSERT( swipe );

		OvrFolderSwipeComponent * swipeComp = swipe->GetComponentById< OvrFolderSwipeComponent >();
		OVR_ASSERT( swipeComp );

		swipeComp->SetRotationByIndex( panelIndex );
	}
}

void OvrFolderBrowser::OnPanelUp( OvrGuiSys & guiSys, const OvrMetaDatum * data )
{
	if ( AllowPanelTouchUp )
	{
		OnPanelActivated( guiSys, data );
	}
}

const OvrMetaDatum * OvrFolderBrowser::NextFileInDirectory( OvrGuiSys & guiSys, const int step )
{
	const OvrMetaDatum * datum = GetNextFileInCategory( guiSys, step );
	if ( datum )
	{
		const FolderView * folder = GetFolderView( GetActiveFolderIndex( guiSys ) );
		if ( folder == NULL )
		{
			return NULL;
		}

		const VRMenuObject * swipeObject = guiSys.GetVRMenuMgr().ToObject( folder->SwipeHandle );
		if ( swipeObject )
		{
			OvrFolderSwipeComponent * swipeComp = swipeObject->GetComponentById<OvrFolderSwipeComponent>();
			if ( swipeComp )
			{
				const int nextPanelIndex = swipeComp->GetNextPanelIndex( step );
				swipeComp->SetRotationByIndex( nextPanelIndex );
				return datum;
			}
		}
	}

	return NULL;
}

const OvrMetaDatum* OvrFolderBrowser::GetNextFileInCategory( OvrGuiSys & guiSys, const int step ) const
{
	const FolderView * folder = GetFolderView( GetActiveFolderIndex( guiSys ) );
	if ( folder == NULL )
	{
		return NULL;
	}

	const int numPanels = folder->Panels.GetSizeI();

	if ( numPanels == 0 )
	{
		return NULL;
	}

	const OvrFolderSwipeComponent * swipeComp = GetSwipeComponentForActiveFolder( guiSys );
	if ( swipeComp )
	{
		const int nextPanelIndex = swipeComp->GetNextPanelIndex( step );
	
		const PanelView * panel = folder->Panels.At( nextPanelIndex );
		VRMenuObject * panelObject = guiSys.GetVRMenuMgr().ToObject( panel->Handle );
		if ( panelObject )
		{
			const OvrPanel_OnUp * panelUpComp = panelObject->GetComponentById<OvrPanel_OnUp>();
			if ( panelUpComp )
			{
				return panelUpComp->GetData();
			}
		}
	}

	return NULL;
}

float OvrFolderBrowser::CalcFolderMaxRotation( const FolderView * folder ) const
{
	if ( folder == NULL )
	{
		OVR_ASSERT( false );
		return 0.0f;
	}
	int numPanels = Alg::Clamp( folder->Panels.GetSizeI() - 1, 0, INT_MAX );
	return static_cast< float >( numPanels );
}

const OvrFolderSwipeComponent * OvrFolderBrowser::GetSwipeComponentForActiveFolder( OvrGuiSys & guiSys ) const
{
	const FolderView * folder = GetFolderView( GetActiveFolderIndex( guiSys ) );
	if ( folder == NULL )
	{
		OVR_ASSERT( false );
		return NULL;
	}

	const VRMenuObject * swipeObject = guiSys.GetVRMenuMgr().ToObject( folder->SwipeHandle );
	if ( swipeObject )
	{
		return swipeObject->GetComponentById<OvrFolderSwipeComponent>();
	}

	return NULL;
}

bool OvrFolderBrowser::GazingAtMenu( Matrix4f const & view ) const
{
	if ( GetFocusedHandle().IsValid() )
	{
		Vector3f viewForwardFlat( view.M[ 2 ][ 0 ], 0.0f, view.M[ 2 ][ 2 ] );
		viewForwardFlat.Normalize();

		const float cosHalf = cosf( VisiblePanelsArcAngle );
		const float dot = viewForwardFlat.Dot( -FWD * GetMenuPose().Rotation );
		
		if ( dot >= cosHalf )
		{
			return true;
		}
	}
	return false;
}

int OvrFolderBrowser::GetActiveFolderIndex( OvrGuiSys & guiSys ) const
{
	VRMenuObject * rootObject = guiSys.GetVRMenuMgr().ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	return rootComp->GetCurrentIndex( guiSys, rootObject );
}

void OvrFolderBrowser::SetActiveFolder( OvrGuiSys & guiSys, int folderIdx )
{
	VRMenuObject * rootObject = guiSys.GetVRMenuMgr().ToObject( GetRootHandle() );
	OVR_ASSERT( rootObject );

	OvrFolderBrowserRootComponent * rootComp = rootObject->GetComponentById<OvrFolderBrowserRootComponent>();
	OVR_ASSERT( rootComp );

	rootComp->SetActiveFolder( folderIdx );
}

void OvrFolderBrowser::TouchDown()
{
	IsTouchDownPosistionTracked = false;
	TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchUp()
{
	IsTouchDownPosistionTracked = false;
	TouchDirectionLocked = NO_LOCK;
}

void OvrFolderBrowser::TouchRelative( Vector3f touchPos )
{
	if ( !IsTouchDownPosistionTracked )
	{
		IsTouchDownPosistionTracked = true;
		TouchDownPosistion = touchPos;
	}

	if ( TouchDirectionLocked == NO_LOCK )
	{
		float xAbsChange = fabsf( TouchDownPosistion.x - touchPos.x );
		float yAbsChange = fabsf( TouchDownPosistion.y - touchPos.y );

		if ( xAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE || yAbsChange >= SCROLL_DIRECTION_DECIDING_DISTANCE )
		{
			if ( xAbsChange >= yAbsChange )
			{
				TouchDirectionLocked = HORIZONTAL_LOCK;
			}
			else
			{
				TouchDirectionLocked = VERTICAL_LOCK;
			}
		}
	}
}

OvrFolderBrowser::FolderView::FolderView( const String & name, const String & tag ) 
	: CategoryTag( tag )
	, LocalizedName( name )
	, FolderIndex( -1 )
	, MaxRotation( 0.0f )
	, Visible( false )
{

}

OvrFolderBrowser::FolderView::~FolderView()
{
	// Thumbnails deallocated in FolderBrowser destructor
	DeletePointerArray( Panels );
}

void OvrFolderBrowser::FolderView::UnloadThumbnails( OvrGuiSys & guiSys, const GLuint defaultTextureId, const int thumbWidth, const int thumbHeight )
{
	FreeThumbnailTextures( defaultTextureId );

	for ( int i = 0; i < Panels.GetSizeI(); ++i )
	{
		PanelView * panel = Panels.At( i );
		if ( panel )
		{
			panel->LoadDefaultThumbnail( guiSys, defaultTextureId, thumbWidth, thumbHeight );
		}
	}
}

void OvrFolderBrowser::FolderView::FreeThumbnailTextures( const GLuint defaultTextureId )
{
	for ( int i = 0; i < Panels.GetSizeI(); ++i )
	{
		PanelView * panel = Panels.At( i );
		if ( panel && ( panel->TextureId != defaultTextureId ) )
		{
			glDeleteTextures( 1, &panel->TextureId  );
			panel->TextureId = 0;
		}
	}
}

void OvrFolderBrowser::PanelView::LoadDefaultThumbnail( OvrGuiSys & guiSys, const GLuint defaultTextureId, const int thumbWidth, const int thumbHeight )
{
	VRMenuObject * panelObject = guiSys.GetVRMenuMgr().ToObject( Handle );
	
	if ( panelObject )
	{
		LOG( "Setting thumb to default texture %d", defaultTextureId );
		panelObject->SetSurfaceTexture( 0, 0, SURFACE_TEXTURE_DIFFUSE, 
			defaultTextureId, thumbWidth, thumbHeight );
	}

	Visible = false;
}

} // namespace OVR
