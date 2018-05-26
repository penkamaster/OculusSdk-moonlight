/************************************************************************************

Filename    :   VRMenuFrame.h
Content     :   Menu component for handling hit tests and dispatching events.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_VRMenuFrame_h )
#define OVR_VRMenuFrame_h

#include "VRMenuObject.h"
#include "VRMenuEvent.h"
#include "GazeCursor.h"
#include "SoundLimiter.h"

namespace OVR {

class ovrFrameInput;
class App;

//==============================================================
// VRMenuEventHandler
class VRMenuEventHandler
{
public:
	VRMenuEventHandler();
	~VRMenuEventHandler();

	void			Frame( OvrGuiSys & guiSys, const ovrFrameInput & vrFrame,
                            menuHandle_t const & rootHandle, Posef const & menuPose, Matrix4f const & traceMat,
							Array< VRMenuEvent > & events );

	void			HandleEvents( OvrGuiSys & guiSys, const ovrFrameInput & vrFrame,
							menuHandle_t const rootHandle, Array< VRMenuEvent > const & events ) const;

	void			InitComponents( Array< VRMenuEvent > & events );
	void			Opening( Array< VRMenuEvent > & events );
	void			Opened( Array< VRMenuEvent > & events );
	void			Closing( Array< VRMenuEvent > & events );
	void			Closed( Array< VRMenuEvent > & events );

	menuHandle_t	GetFocusedHandle() const { return FocusedHandle; }

private:
	menuHandle_t	FocusedHandle;

	ovrSoundLimiter	GazeOverSoundLimiter;
	ovrSoundLimiter	DownSoundLimiter;
	ovrSoundLimiter	UpSoundLimiter;

private:
    bool            DispatchToComponents( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                            VRMenuEvent const & event, VRMenuObject * receiver ) const;
    bool            DispatchToPath( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                            VRMenuEvent const & event, Array< menuHandle_t > const & path, bool const log ) const;
	bool            BroadcastEvent( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                            VRMenuEvent const & event, VRMenuObject * receiver ) const;
};

} // namespace OVR

#endif // OVR_VRMenuFrame_h
