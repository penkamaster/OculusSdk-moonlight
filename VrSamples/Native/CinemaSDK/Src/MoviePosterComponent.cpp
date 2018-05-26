/************************************************************************************

Filename    :   MoviePosterComponent.cpp
Content     :   Menu component for the movie selection menu.
Created     :   August 13, 2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "MoviePosterComponent.h"
//#include "MovieManager.h"
#include "UI/UIContainer.h"
#include "UI/UIImage.h"
#include "UI/UILabel.h"

namespace OculusCinema {

bool MoviePosterComponent::ShowShadows = true;

//==============================
//  MoviePosterComponent::
MoviePosterComponent::MoviePosterComponent() :
	CarouselItemComponent( VRMenuEventFlags_t() ),
	Movie( NULL ),
    Poster( NULL ),
	PosterImage( NULL ),
    Is3DIcon( NULL ),
    Shadow( NULL )
{
}

//==============================
//  MoviePosterComponent::OnEvent_Impl
eMsgStatus MoviePosterComponent::OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
        VRMenuObject * self, VRMenuEvent const & event )
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( vrFrame );
	OVR_UNUSED( self );
	OVR_UNUSED( event );

	return MSG_STATUS_ALIVE;
}

//==============================
//  MoviePosterComponent::SetMenuObjects
void MoviePosterComponent::SetMenuObjects( const int width, const int height, UIContainer * poster, UIImage * posterImage, UIImage * is3DIcon, UIImage * shadow )
{
	Width = width;
	Height = height;
	Poster = poster;
	PosterImage = posterImage;
    Is3DIcon = is3DIcon;
    Shadow = shadow;

    Movie = NULL;
    Is3DIcon->SetVisible( false );
	Shadow->SetVisible( false );
	PosterImage->SetVisible( false );
}

//==============================
//  MoviePosterComponent::SetItem
void MoviePosterComponent::SetItem( VRMenuObject * self, const CarouselItem * item, const PanelPose & pose )
{
	OVR_UNUSED( self );

	MovieDef * movie = ( item == NULL ) ? NULL : ( MovieDef * )item->UserData;

	Poster->SetLocalPose( pose.Orientation, pose.Position );
	PosterImage->SetColor( pose.Color );
	Is3DIcon->SetColor( pose.Color );
	Shadow->SetColor( pose.Color );

	if ( movie != Movie )
	{
		if ( movie != NULL )
		{
			PosterImage->SetImage( 0, SURFACE_TEXTURE_DIFFUSE, item->Texture, 
				static_cast<short>( Width ), static_cast<short>( Height ) );

			//Is3DIcon->SetVisible( movie->Is3D );
			Shadow->SetVisible( ShowShadows );
			PosterImage->SetVisible( true );
		}
		else
		{
			Is3DIcon->SetVisible( false );
			Shadow->SetVisible( false );
			PosterImage->SetVisible( false );
		}
		Movie = movie;
	}
}

} // namespace OculusCinema
