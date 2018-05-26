/************************************************************************************

Filename    :   MovieCategoryComponent.h
Content     :   Menu component for the movie category menu.
Created     :   August 13, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VRMenuComponent.h"
#include "PcManager.h"
#include "SelectionView.h"

#if !defined( MovieCategoryComponent_h )
#define MovieCategoryComponent_h

using namespace OVR;

namespace OculusCinema {

class MovieSelectionView;

//==============================================================
// MovieCategoryComponent
class PcCategoryComponent : public VRMenuComponent
{
public:
							PcCategoryComponent( SelectionView *view, PcCategory category );

	static const Vector4f	HighlightColor;
	static const Vector4f	FocusColor;
	static const Vector4f	NormalColor;

private:
    ovrSoundLimiter			Sound;

	bool					HasFocus;

	PcCategory			Category;

    SelectionView *	CallbackView;

private:
    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

	void					UpdateColor( VRMenuObject * self );

    eMsgStatus              Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusGained( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
    eMsgStatus              FocusLost( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );
};

} // namespace OculusCinema

#endif // MovieCategoryComponent_h
