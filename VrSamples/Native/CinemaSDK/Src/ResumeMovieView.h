/************************************************************************************

Filename    :   ResumeMovieView.h
Content     :
Created     :	9/3/2014
Authors     :   Jim Dosé

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( ResumeMovieView_h )
#define ResumeMovieView_h

#include "View.h"
#include "VRMenu.h"

namespace OculusCinema {

class CinemaApp;

class ResumeMovieView : public View
{
public:
						ResumeMovieView( CinemaApp &cinema );
	virtual 			~ResumeMovieView();

	virtual void 		OneTimeInit( const char * launchIntent );
	virtual void		OneTimeShutdown();

	virtual void 		OnOpen();
	virtual void 		OnClose();

	virtual bool 		OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType );
	virtual void 		Frame( const ovrFrameInput & vrFrame );

	void 				ResumeChoice( int itemNum );

private:
	CinemaApp &			Cinema;

	static VRMenuId_t  	ID_CENTER_ROOT;
	static VRMenuId_t  	ID_TITLE;
	static VRMenuId_t  	ID_OPTIONS;
	static VRMenuId_t  	ID_OPTION_ICONS;

	VRMenu *			Menu;

private:
	ResumeMovieView &	operator=( const ResumeMovieView & );

	void				SetPosition( OvrVRMenuMgr & menuMgr, const Vector3f &pos );
	void 				CreateMenu( OvrGuiSys & guiSys );
};

} // namespace OculusCinema

#endif // ResumeMovieView_h
