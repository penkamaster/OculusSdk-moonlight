/************************************************************************************

Filename    :   CinemaStrings.cpp
Content     :	Text strings used by app.  Located in one place to make translation easier.
Created     :	9/30/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "CinemaStrings.h"
#include "OVR_Locale.h"
#include "CinemaApp.h"

namespace OculusCinema
{

void ovrCinemaStrings::OneTimeInit( CinemaApp & cinema )
{
	LOG( "ovrCinemaStrings::OneTimeInit" );

	cinema.GetLocale().GetString( "@string/Category_Trailers", 		"@string/Category_Trailers", 		Category_Trailers );
	cinema.GetLocale().GetString( "@string/Category_MyVideos", 		"@string/Category_MyVideos", 		Category_MyVideos );	
	
	cinema.GetLocale().GetString( "@string/Category_LimeLight", 	"@string/Category_LimeLight", 		Category_LimeLight );
	cinema.GetLocale().GetString( "@string/Category_RemoteDesktop", 		"@string/Category_RemoteDesktop", 		Category_RemoteDesktop );
	cinema.GetLocale().GetString( "@string/Category_VNC", 		"@string/Category_VNC", 		Category_VNC );
	
	cinema.GetLocale().GetString( "@string/MovieSelection_Resume",	"@string/MovieSelection_Resume",	MovieSelection_Resume );
	cinema.GetLocale().GetString( "@string/MovieSelection_Next", 	"@string/MovieSelection_Next", 		MovieSelection_Next );
	cinema.GetLocale().GetString( "@string/ResumeMenu_Title", 		"@string/ResumeMenu_Title", 		ResumeMenu_Title );
	cinema.GetLocale().GetString( "@string/ResumeMenu_Resume", 		"@string/ResumeMenu_Resume", 		ResumeMenu_Resume );
	cinema.GetLocale().GetString( "@string/ResumeMenu_Restart", 	"@string/ResumeMenu_Restart", 		ResumeMenu_Restart );
	cinema.GetLocale().GetString( "@string/TheaterSelection_Title", "@string/TheaterSelection_Title", 	TheaterSelection_Title );

	cinema.GetLocale().GetString( "@string/Error_NoVideosOnPhone", 	"@string/Error_NoVideosOnPhone", 	Error_NoVideosOnPhone );

	cinema.GetLocale().GetString( "@string/Error_NoVideosInMyVideos", "@string/Error_NoVideosInMyVideos", Error_NoVideosInMyVideos );

cinema.GetLocale().GetString( "@string/Error_NoVideosInLimeLight", "@string/Error_NoVideosInLimeLight", Error_NoVideosInLimeLight );

	//cinema.GetLocale().GetString( "@string/Error_UnableToPlayMovie", "@string/Error_UnableToPlayMovie",	Error_UnableToPlayMovie );

	cinema.GetLocale().GetString( "@string/MoviePlayer_Reorient", 	"@string/MoviePlayer_Reorient", 	MoviePlayer_Reorient );

	cinema.GetLocale().GetString( "@string/ButtonText_ButtonMapKeyboard", 	"@string/ButtonText_ButtonMapKeyboard", 	ButtonText_ButtonMapKeyboard );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonSpeed", 	    "@string/ButtonText_ButtonSpeed", 	        ButtonText_ButtonSpeed );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonComfortMode", 	"@string/ButtonText_ButtonComfortMode", 	ButtonText_ButtonComfortMode );

	cinema.GetLocale().GetString( "@string/ButtonText_ButtonHostAudio", 	"@string/ButtonText_ButtonHostAudio", 	ButtonText_ButtonHostAudio );
	cinema.GetLocale().GetString( "@string/ButtonText_Button4k30", 	"@string/ButtonText_Button4k30", 	ButtonText_Button4k30 );
	cinema.GetLocale().GetString( "@string/ButtonText_Button4k60", 	"@string/ButtonText_Button4k60", 	ButtonText_Button4k60 );
	cinema.GetLocale().GetString( "@string/ButtonText_Button1080p30", 	"@string/ButtonText_Button1080p30", 	ButtonText_Button1080p30 );
	cinema.GetLocale().GetString( "@string/ButtonText_Button1080p60", 	"@string/ButtonText_Button1080p60", 	ButtonText_Button1080p60 );
	cinema.GetLocale().GetString( "@string/ButtonText_Button720p30", 	"@string/ButtonText_Button720p30", 	    ButtonText_Button720p30 );
	cinema.GetLocale().GetString( "@string/ButtonText_Button720p60", 	"@string/ButtonText_Button720p60", 	    ButtonText_Button720p60 );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonDistance", 	"@string/ButtonText_ButtonDistance", 	ButtonText_ButtonDistance );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonSize", 	"@string/ButtonText_ButtonSize", 	ButtonText_ButtonSize);
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonSBS", 	"@string/ButtonText_ButtonSBS", 	ButtonText_ButtonSBS );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonChangeSeat", 	"@string/ButtonText_ButtonChangeSeat", 	ButtonText_ButtonChangeSeat );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonGaze", 	"@string/ButtonText_ButtonGaze", 	ButtonText_ButtonGaze );
	cinema.GetLocale().GetString( "@string/ButtonText_ButtonTrackpad", 	"@string/ButtonText_ButtonTrackpad", 	ButtonText_ButtonTrackpad );

	cinema.GetLocale().GetString( "@string/ButtonText_ButtonOff", 	"@string/ButtonText_ButtonOff", 	ButtonText_ButtonOff );
	cinema.GetLocale().GetString( "@string/ButtonText_LabelGazeScale", 	"@string/ButtonText_LabelGazeScale", 	ButtonText_LabelGazeScale );
	cinema.GetLocale().GetString( "@string/ButtonText_LabelTrackpadScale", 	"@string/ButtonText_LabelTrackpadScale", 	ButtonText_LabelTrackpadScale );







}

ovrCinemaStrings *	ovrCinemaStrings::Create( CinemaApp & cinema )
{
	ovrCinemaStrings * cs = new ovrCinemaStrings;
	cs->OneTimeInit( cinema );
	return cs;
}

void ovrCinemaStrings::Destroy( CinemaApp & cinema, ovrCinemaStrings * & strings )
{
	OVR_UNUSED( cinema );

	delete strings;
	strings = NULL;
}

} // namespace OculusCinema
