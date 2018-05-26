/************************************************************************************

Filename    :   TheaterSelectionComponent.h
Content     :   Menu component for the movie theater selection menu.
Created     :   August 15, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Fader.h"
#include "CarouselBrowserComponent.h"

#if !defined( TheaterSelectionComponent_h )
#define TheaterSelectionComponent_h

using namespace OVR;

namespace OculusCinema {

class TheaterSelectionView;

//==============================================================
// TheaterSelectionComponent
class TheaterSelectionComponent : public CarouselItemComponent
{
public:
	TheaterSelectionComponent( TheaterSelectionView *view );

	virtual void 			SetItem( VRMenuObject * self, const CarouselItem * item, const PanelPose &pose );

private:
    ovrSoundLimiter    		Sound;

    TheaterSelectionView * 	CallbackView;

    SineFader       		HilightFader;
    double          		StartFadeInTime;
    double          		StartFadeOutTime;
    float           		HilightScale;
    float           		FadeDuration;

private:
    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

    eMsgStatus              FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus 				Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
            						VRMenuObject * self, VRMenuEvent const & event );
};

} // namespace OculusCinema

#endif // TheaterSelectionComponent_h
