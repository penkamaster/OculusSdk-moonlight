/************************************************************************************

Filename    :   MoviePosterComponent.h
Content     :   Menu component for the movie selection menu.
Created     :   August 13, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( MoviePosterComponent_h )
#define MoviePosterComponent_h

#include "CarouselBrowserComponent.h"
#include "UI/UIImage.h"
#include "UI/UIContainer.h"

using namespace OVR;

namespace OculusCinema {

class MovieDef;

//==============================================================
// MoviePosterComponent
class MoviePosterComponent : public CarouselItemComponent
{
public:
							MoviePosterComponent();

	static bool 			ShowShadows;

	void 					SetMenuObjects( const int width, const int height, UIContainer * poster, UIImage * posterImage, UIImage * is3DIcon, UIImage * shadow );
	virtual void 			SetItem( VRMenuObject * self, const CarouselItem * item, const PanelPose &pose );

private:
    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

    const MovieDef *	 	Movie;

    int						Width;
    int						Height;

    UIContainer * 			Poster;
    UIImage * 				PosterImage;
    UIImage * 				Is3DIcon;
    UIImage * 				Shadow;
};

} // namespace OculusCinema

#endif // MoviePosterComponent_h
