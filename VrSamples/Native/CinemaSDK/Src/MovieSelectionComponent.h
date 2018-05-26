/************************************************************************************

Filename    :   MovieSelectionComponent.h
Content     :   Menu component for the movie selection menu.
Created     :   October 8, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VRMenuComponent.h"
#include "SelectionView.h"


#if !defined( MovieSelectionComponent_h )
#define MovieSelectionComponent_h

using namespace OVR;

namespace OculusCinema {

class MovieSelectionView;

//==============================================================
// MovieSelectionComponent
class MovieSelectionComponent : public VRMenuComponent
{
public:
							MovieSelectionComponent( SelectionView *view );

private:
    ovrSoundLimiter			Sound;
    SelectionView *	CallbackView;

private:
    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

    eMsgStatus              Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
};

} // namespace OculusCinema

#endif // MovieSelectionComponent_h
