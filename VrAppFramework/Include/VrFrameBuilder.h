/************************************************************************************

Filename    :   VrFrameBuilder.h
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrFrameBuilder_h
#define OVR_VrFrameBuilder_h

#include "OVR_Input.h"
#include "KeyState.h"

namespace OVR {

static const int MAX_INPUT_KEY_EVENTS = 16;

struct ovrInputEvents
{
	ovrInputEvents() :
		JoySticks(),
		TouchPosition(),
		TouchAction( -1 ),
		NumKeyEvents( 0 ),
		KeyEvents() {}

	float JoySticks[2][2];
	float TouchPosition[2];
	int TouchAction;
	int NumKeyEvents;
	struct ovrKeyEvents_t
	{
		ovrKeyEvents_t()
			: KeyCode( OVR_KEY_NONE )
			, RepeatCount( 0 )
			, Down( false ) 
			, IsJoypadButton( false )
		{
		}

		ovrKeyCode	KeyCode;
		int			RepeatCount;
		bool		Down : 1;
		bool		IsJoypadButton : 1;
	} KeyEvents[MAX_INPUT_KEY_EVENTS];
};

class VrFrameBuilder
{
public:
						VrFrameBuilder();

	void				Init( ovrJava * java );

	void				UpdateNetworkState( JNIEnv * jni, jclass activityClass, jobject activityObject );

	void				AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
										const ovrJava & java,
										const ovrTrackingTransform trackingTransform,
										const long long enteredVrModeFrameNumber );
	const ovrFrameInput &		Get() const { return vrFrame; }

private:
	ovrFrameInput vrFrame;
	KeyState	BackKeyState;

	double		lastTouchpadTime;
	double		touchpadTimer;
	bool		lastTouchDown;
	int			touchState;
	Vector2f	touchOrigin;

	void 		InterpretTouchpad( VrInput & input, const double currentTime );
	void		AddKeyEventToFrame( ovrKeyCode const keyCode, KeyEventType const eventType, int const repeatCount );
};

}	// namespace OVR

#endif // OVR_VrFrameBuilder_h
