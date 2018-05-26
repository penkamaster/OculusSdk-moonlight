/************************************************************************************

Filename    :   ControllerGUI.h
Content     :   
Created     :   3/8/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_ControllerGUI_h )
#define OVR_ControllerGUI_h

#include "VRMenu.h"

namespace OVR {

class ovrVrController;

class ovrControllerGUI : public OVR::VRMenu {
public:
	static char const *MENU_NAME;

	virtual ~ovrControllerGUI() {}

	static ovrControllerGUI *Create( ovrVrController &vrControllerApp );

private:
	ovrVrController &VrControllerApp;

private:
	ovrControllerGUI( ovrVrController &vrControllerApp )
			: VRMenu( MENU_NAME ), VrControllerApp( vrControllerApp ) {
	}

	ovrControllerGUI operator=( ovrControllerGUI & ) = delete;

	virtual void OnItemEvent_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame,
								   VRMenuId_t const itemId, VRMenuEvent const &event ) OVR_OVERRIDE;

	virtual bool OnKeyEvent_Impl( OvrGuiSys &guiSys, int const keyCode,
								  const int repeatCount,
								  KeyEventType const eventType ) OVR_OVERRIDE;

	virtual void PostInit_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame ) OVR_OVERRIDE;

	virtual void Open_Impl( OvrGuiSys &guiSys ) OVR_OVERRIDE;

	virtual void Frame_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame ) OVR_OVERRIDE;
};

} // namespace OVR

#endif // OVR_ControllerGUI_h
