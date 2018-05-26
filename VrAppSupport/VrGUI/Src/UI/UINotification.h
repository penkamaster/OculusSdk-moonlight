/************************************************************************************

Filename    :   UINotification.h
Content     :   A pop up Notification object, intended to be re-usable by many apps.   Lets you queue up and display info / warning messages
Created     :   Apr 23, 2015
Authors     :   Clint Brewer

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UINotification_h )
#define UINotification_h


#include "VRMenu.h"
#include "UI/UIObject.h"
#include "VRMenuComponent.h"
#include "UI/UITexture.h"


#include "UI/UILabel.h"
#include "UI/UIImage.h"
#include "UI/UIButton.h"

#include "Kernel/OVR_Deque.h"


namespace OVR {

class VrAppInterface;
class UINotification;

typedef Deque< String > NotificationsDequeue;

//==============================================================
// UINotificationComponent
class UINotificationComponent : public VRMenuComponent
{
public:
	static const int TYPE_ID = 439493;

	UINotificationComponent( UINotification &notification );

	virtual int		GetTypeId() const { return TYPE_ID; }


private:
	UINotification &		Notification;

private:
	// private assignment operator to prevent copying
	UINotificationComponent &	operator = ( UINotificationComponent & );

    virtual eMsgStatus      OnEvent_Impl( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                    VRMenuObject * self, VRMenuEvent const & event );

    eMsgStatus 				Frame( OvrGuiSys & guiSys, ovrFrameInput const & vrFrame,
                                        VRMenuObject * self, VRMenuEvent const & event );
};


//==============================================================
// UINotification

class UINotification : public UIObject
{
	friend class UINotificationComponent;
public:
						UINotification( OvrGuiSys &guiSys );
						~UINotification();

	void 				AddToMenu( UIMenu *menu, const char* iconTextureName,  const char* backgroundTintTextureName, UIObject *parent = NULL );
	void				QueueNotification( const String& description, bool showImmediately = false );//will interrupt any currently displaying notice


private:

	void 				SetDescription( const String &description );
	void 				Update( float deltaSeconds );

	NotificationsDequeue		NotificationsQueue;
	UINotificationComponent *	NotificationComponent;


	UITexture			BackgroundTintTexture;
	UIImage				BackgroundImage;

	UITexture			IconTexture;
	UIImage				IconImage;

	UILabel				DescriptionLabel;


	float				VisibleDuration; //how long should this notification be shown
	float				FadeInDuration; //how long does it take to fade in
	float				FadeOutDuration; //how long does it take to fade out

	float				TimeLeft; //how long until this notification is gone
};

}

#endif /* UINotification_h */
