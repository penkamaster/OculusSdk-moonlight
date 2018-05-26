/************************************************************************************

Filename    :   CarouselBrowserComponent.h
Content     :   A menu for browsing a hierarchy of folders with items represented by thumbnails.
Created     :   July 25, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( OVR_CarouselBrowser_h )
#define OVR_CarouselBrowser_h

#include "VRMenu.h"
#include "VRMenuComponent.h"

using namespace OVR;

namespace OculusCinema {

class CarouselItem
{
public:
	String		Name;
	GLuint		Texture;
	int			TextureWidth;
	int			TextureHeight;
	void *		UserData;

				CarouselItem() : Texture( 0 ), TextureWidth( 0 ), TextureHeight( 0 ), UserData( NULL ) {}
};

class PanelPose
{
public:
	Quatf    	Orientation;
	Vector3f 	Position;
	Vector4f	Color;

				PanelPose() {};
				PanelPose( Quatf orientation, Vector3f position, Vector4f color ) :
					Orientation( orientation ), Position( position ), Color( color ) {}
};

class CarouselItemComponent : public VRMenuComponent
{
public:
	explicit						CarouselItemComponent( VRMenuEventFlags_t const & eventFlags ) :
										VRMenuComponent( eventFlags )
									{
									}

	virtual							~CarouselItemComponent() { }

	virtual void 					SetItem( VRMenuObject * self, const CarouselItem * item, const PanelPose &pose ) = 0;
};

class CarouselBrowserComponent : public VRMenuComponent
{
public:
									CarouselBrowserComponent( const Array<CarouselItem *> &items, const Array<PanelPose> &panelPoses );

	void							SetPanelPoses( OvrVRMenuMgr & menuMgr, VRMenuObject * self, const Array<PanelPose> &panelPoses );
	void 							SetMenuObjects( const Array<VRMenuObject *> &menuObjs, const Array<CarouselItemComponent *> &menuComps );
	void							SetItems( const Array<CarouselItem *> &items );
	void							SetSelectionIndex( const int selectedIndex );
    int 							GetSelection() const;
	bool							HasSelection() const;
	bool							IsSwiping() const { return Swiping; }
	bool							CanSwipeBack() const;
	bool							CanSwipeForward() const;

	void 							CheckGamepad( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self );

private:
    virtual eMsgStatus 				OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );
    PanelPose 						GetPosition( const float t );
    void 							UpdatePanels( OvrVRMenuMgr & menuMgr, VRMenuObject * self );

    eMsgStatus 						Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus 						SwipeForward( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self );
    eMsgStatus 						SwipeBack( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self );
	eMsgStatus 						TouchDown( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus 						TouchUp( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus 						Opened( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );
	eMsgStatus 						Closed( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame, VRMenuObject * self, VRMenuEvent const & event );

public:
    bool							SelectPressed;

private:
	Vector3f						PositionScale;
    float							Position;
	double							TouchDownTime;			// the time in second when a down even was received, < 0 if touch is not down

    Array<CarouselItem *> 			Items;
    Array<VRMenuObject *> 			MenuObjs;
    Array<CarouselItemComponent *> 	MenuComps;
	Array<PanelPose>				PanelPoses;

	double 							StartTime;
	double 							EndTime;
	float							PrevPosition;
	float							NextPosition;

	bool							Swiping;
	bool							PanelsNeedUpdate;
};

} // namespace OculusCinema

#endif // OVR_CarouselBrowser_h
