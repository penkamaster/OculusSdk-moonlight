/************************************************************************************

Filename    :   UIContainer.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UIContainer_h )
#define UIContainer_h

#include "VRMenu.h"
#include "UI/UIObject.h"

namespace OVR {

class VrAppInterface;

class UIContainer : public UIObject
{
public:
										UIContainer( OvrGuiSys &guiSys );
	virtual 							~UIContainer();

	void 								AddToMenu( UIMenu *menu, UIObject *parent = NULL );
};

} // namespace OVR

#endif // UIContainer_h
