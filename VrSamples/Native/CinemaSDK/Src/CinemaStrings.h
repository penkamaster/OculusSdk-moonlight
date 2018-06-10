/************************************************************************************

Filename    :   CinemaStrings.h
Content     :	Text strings used by app.  Located in one place to make translation easier.
Created     :	9/30/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( CinemaStrings_h )
#define CinemaStrings_h

#include "Kernel/OVR_String.h"

using namespace OVR;

namespace OculusCinema {

class CinemaApp;

class ovrCinemaStrings {
public:
	static ovrCinemaStrings *	Create( CinemaApp & cinema );
	static void					Destroy( CinemaApp & cinema, ovrCinemaStrings * & strings );

	void		OneTimeInit( CinemaApp &cinema );

	String		Category_Trailers;
	String		Category_MyVideos;
	String     Category_LimeLight;
    String     Category_RemoteDesktop;
    String     Category_VNC;
	

	String		MovieSelection_Resume;
	String		MovieSelection_Next;

	String		ResumeMenu_Title;
	String		ResumeMenu_Resume;
	String		ResumeMenu_Restart;

	String		TheaterSelection_Title;

	String		Error_NoVideosOnPhone;
	String		Error_NoVideosInMyVideos;
	String      Error_NoVideosInLimeLight;

	String	ButtonText_ButtonSaveApp;
	String	ButtonText_ButtonSaveDefault;
	String	ButtonText_ButtonResetSettings;
	String	ButtonText_ButtonSaveSettings1;
	String	ButtonText_ButtonSaveSettings2;
	String	ButtonText_ButtonSaveSettings3;
	String	ButtonText_ButtonLoadSettings1;
	String	ButtonText_ButtonLoadSettings2;
	String	ButtonText_ButtonLoadSettings3;

	String    ButtonText_ButtonMapKeyboard;
	String    ButtonText_ButtonSpeed;
	String    ButtonText_ButtonComfortMode;
	String    ButtonText_ButtonHostAudio;
	String    ButtonText_Button4k60;
	String    ButtonText_Button4k30;
	String    ButtonText_Button1080p60;
	String    ButtonText_Button1080p30;
	String    ButtonText_Button720p60;
	String    ButtonText_Button720p30;
	String    ButtonText_ButtonDistance;
	String    ButtonText_ButtonSize;
	String    ButtonText_ButtonSBS;
	String    ButtonText_ButtonChangeSeat;
	String    ButtonText_ButtonGaze;
	String    ButtonText_ButtonTrackpad;

	String    ButtonText_LabelTrackpadScale;


	String    ButtonText_ButtonOff;
	String    ButtonText_LabelGazeScale;

	String 	  Error_UnknownHost;
	String    Error_AddPCFailed;


	String		Error_UnableToPlayMovie;

	String		MoviePlayer_Reorient;
};

} // namespace OculusCinema

#endif // CinemaStrings_h
