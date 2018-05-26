/************************************************************************************

Filename    :   ControllerGUI.cpp
Content     :   
Created     :   3/8/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrController.h"
#include "ControllerGUI.h"

namespace OVR {

const char *ovrControllerGUI::MENU_NAME = "controllerGUI";

ovrControllerGUI *ovrControllerGUI::Create( ovrVrController &vrControllerApp ) 
{
	char const *menuFiles[] =
			{
					"apk:///assets/controllergui.txt",
					nullptr
			};

	ovrControllerGUI *menu = new ovrControllerGUI( vrControllerApp );
	if ( !menu->InitFromReflectionData( vrControllerApp.GetGuiSys(),
										vrControllerApp.app->GetFileSys(),
										vrControllerApp.GetGuiSys().GetReflection(),
										vrControllerApp.GetLocale(), menuFiles,
										2.0f, VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ))) 
	{
		delete menu;
		return nullptr;
	}
	return menu;
}

void ovrControllerGUI::OnItemEvent_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame,
		VRMenuId_t const itemId, VRMenuEvent const &event ) 
{
}

bool ovrControllerGUI::OnKeyEvent_Impl( OvrGuiSys &guiSys, int const keyCode,
		const int repeatCount, KeyEventType const eventType ) 
{
	return ( keyCode == OVR_KEY_BACK );
}

void ovrControllerGUI::PostInit_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame ) 
{
}

void ovrControllerGUI::Open_Impl( OvrGuiSys &guiSys ) 
{
}

void ovrControllerGUI::Frame_Impl( OvrGuiSys &guiSys, ovrFrameInput const &vrFrame ) 
{
	OVR_UNUSED( VrControllerApp );
}

} // namespace OVR
