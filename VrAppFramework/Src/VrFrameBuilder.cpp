/************************************************************************************

Filename    :   VrFrameBuilder.cpp
Content     :   Builds the input for VrAppInterface::Frame()
Created     :   April 26, 2015
Authors     :   John Carmack

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrFrameBuilder.h"
#include "Kernel/OVR_LogUtils.h"
#include "Android/JniUtils.h"
#include "Kernel/OVR_Alg.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "Kernel/OVR_String.h"
#include "OVR_Input.h"

#if defined ( OVR_OS_ANDROID )
#include <android/input.h>
#elif defined ( OVR_OS_WIN32 )
#include <wininet.h>
#include <BluetoothAPIs.h>
#endif


namespace OVR
{

static struct
{
	ovrKeyCode	KeyCode;
	int			ButtonBit;
} buttonMappings[] = {
	{ OVR_KEY_BUTTON_A, 			BUTTON_A },
	{ OVR_KEY_BUTTON_B,				BUTTON_B },
	{ OVR_KEY_BUTTON_X, 			BUTTON_X },
	{ OVR_KEY_BUTTON_Y,				BUTTON_Y },
	{ OVR_KEY_BUTTON_START, 		BUTTON_START },
	{ OVR_KEY_ESCAPE,				BUTTON_BACK },
	{ OVR_KEY_BUTTON_SELECT, 		BUTTON_SELECT },
	{ OVR_KEY_MENU,					BUTTON_MENU },				// not really sure if left alt is the same as android OVR_KEY_MENU, but this is unused
	{ OVR_KEY_RIGHT_TRIGGER,		BUTTON_RIGHT_TRIGGER },
	{ OVR_KEY_LEFT_TRIGGER, 		BUTTON_LEFT_TRIGGER },
	{ OVR_KEY_DPAD_UP,				BUTTON_DPAD_UP },
	{ OVR_KEY_DPAD_DOWN,			BUTTON_DPAD_DOWN },
	{ OVR_KEY_DPAD_LEFT,			BUTTON_DPAD_LEFT },
	{ OVR_KEY_DPAD_RIGHT,			BUTTON_DPAD_RIGHT },
	{ OVR_KEY_LSTICK_UP,			BUTTON_LSTICK_UP },
	{ OVR_KEY_LSTICK_DOWN,			BUTTON_LSTICK_DOWN },
	{ OVR_KEY_LSTICK_LEFT,			BUTTON_LSTICK_LEFT },
	{ OVR_KEY_LSTICK_RIGHT,			BUTTON_LSTICK_RIGHT },
	{ OVR_KEY_RSTICK_UP,			BUTTON_RSTICK_UP },
	{ OVR_KEY_RSTICK_DOWN,			BUTTON_RSTICK_DOWN },
	{ OVR_KEY_RSTICK_LEFT,			BUTTON_RSTICK_LEFT },
	{ OVR_KEY_RSTICK_RIGHT,			BUTTON_RSTICK_RIGHT },
	/// FIXME: the xbox controller doesn't generate OVR_KEY_RIGHT_TRIGGER / OVR_KEY_LEFT_TRIGGER
	// because they are analog axis instead of buttons.  For now, map shoulders to our triggers.
	{ OVR_KEY_BUTTON_LEFT_SHOULDER, BUTTON_LEFT_TRIGGER },
	{ OVR_KEY_BUTTON_RIGHT_SHOULDER, BUTTON_RIGHT_TRIGGER },
	/// FIXME: the following joypad buttons are not mapped yet because they would require extending the
	/// bit flags to 64 bits.
	{ OVR_KEY_BUTTON_C,				0 },
	{ OVR_KEY_BUTTON_Z,				0 },
	{ OVR_KEY_BUTTON_LEFT_THUMB,	0 },
	{ OVR_KEY_BUTTON_RIGHT_THUMB,	0 },

	{ OVR_KEY_MAX, 0 }
};

static ovrHeadSetPluggedState HeadPhonesPluggedState = OVR_HEADSET_PLUGGED_UNKNOWN;

VrFrameBuilder::VrFrameBuilder() :
	BackKeyState( 0.25f ),	// NOTE: This is the default value. System specified value will be set on init.
	lastTouchpadTime( 0.0 ),
	touchpadTimer( 0.0 ),
	lastTouchDown( false ),
	touchState( 0 )
{
}

void VrFrameBuilder::Init( ovrJava * java )
{
	OVR_ASSERT( java != NULL );

	const float BackButtonShortPressTimeInSec = vrapi_GetSystemPropertyFloat( java, VRAPI_SYS_PROP_BACK_BUTTON_SHORTPRESS_TIME );

	BackKeyState.SetShortPressTime( BackButtonShortPressTimeInSec );
}

void VrFrameBuilder::UpdateNetworkState( JNIEnv * jni, jclass activityClass, jobject activityObject )
{
#if defined( OVR_OS_ANDROID )
	const jmethodID isWififConnectedMethodId = ovr_GetStaticMethodID( jni, activityClass, "isWifiConnected", "(Landroid/app/Activity;)Z" );

	// NOTE: make sure android.permission.ACCESS_NETWORK_STATE is set in the manifest for isWifiConnected().
	vrFrame.DeviceStatus.WifiIsConnected		= jni->CallStaticBooleanMethod( activityClass, isWififConnectedMethodId, activityObject );
#elif defined ( OVR_OS_WIN32 )
	DWORD dwState = 0;
	vrFrame.DeviceStatus.WifiIsConnected		= InternetGetConnectedState( &dwState, 0 ) ? true : false;
#else
	vrFrame.DeviceStatus.WifiIsConnected		= false;
#endif
}

void VrFrameBuilder::InterpretTouchpad( VrInput & input, const double currentTime )
{
	// 1) Down -> Up w/ Motion = Slide
	// 1) Down -> Timeout w/o Motion = Long Press
	// 2) Down -> Up w/out Motion -> Timeout = Single Tap
	// 3) Down -> Up w/out Motion -> Down -> Timeout = Nothing
	// 4) Down -> Up w/out Motion -> Down -> Up = Double Tap
	static const double timer_finger_down = 0.3;
	static const double timer_finger_up = 0.3;

	static const float min_swipe_distance = 100.0f;

	const double deltaTime = currentTime - lastTouchpadTime;
	lastTouchpadTime = currentTime;
	touchpadTimer = touchpadTimer + deltaTime;

	const bool currentTouchDown = ( input.buttonState & BUTTON_TOUCH ) != 0;

	bool down = false;
	if ( currentTouchDown && !lastTouchDown )
	{
		//LOG( "DOWN" );
		down = true;
		touchOrigin = input.touch;
	}

	bool up = false;
	if ( !currentTouchDown && lastTouchDown )
	{
		//LOG( "UP" );
		up = true;
	}

	lastTouchDown = currentTouchDown;

	input.touchRelative = input.touch - touchOrigin;
	float touchMagnitude = input.touchRelative.Length();
	input.swipeFraction = touchMagnitude / min_swipe_distance;

	switch ( touchState )
	{
	case 0:
		if ( down )
		{
			touchState = 1;
			touchpadTimer = 0.0;
		}
		break;
	case 1:
		if ( touchMagnitude >= min_swipe_distance )
		{
			int dir = 0;
			if ( fabs( input.touchRelative[0] ) > fabs( input.touchRelative[1] ) )
			{
				if ( input.touchRelative[0] < 0 )
				{
					//LOG( "SWIPE FORWARD" );
					dir = BUTTON_SWIPE_FORWARD | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//LOG( "SWIPE BACK" );
					dir = BUTTON_SWIPE_BACK | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			else
			{
				if ( input.touchRelative[1] > 0 )
				{
					//LOG( "SWIPE DOWN" );
					dir = BUTTON_SWIPE_DOWN | BUTTON_TOUCH_WAS_SWIPE;
				}
				else
				{
					//LOG( "SWIPE UP" );
					dir = BUTTON_SWIPE_UP | BUTTON_TOUCH_WAS_SWIPE;
				}
			}
			input.buttonPressed |= dir;
			input.buttonReleased |= dir & ~BUTTON_TOUCH_WAS_SWIPE;
			input.buttonState |= dir;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			if ( touchpadTimer < timer_finger_down )
			{
				touchState = 2;
				touchpadTimer = 0.0;
			}
			else
			{
				input.buttonPressed |= BUTTON_TOUCH_SINGLE;
				input.buttonReleased |= BUTTON_TOUCH_SINGLE;
				input.buttonState |= BUTTON_TOUCH_SINGLE;
				touchState = 0;
				touchpadTimer = 0.0;
			}
		}
		else if ( touchpadTimer > 0.75f ) // TODO: BUTTON_TOUCH_LONGPRESS actually used?
		{
			input.buttonPressed |= BUTTON_TOUCH_LONGPRESS;
			input.buttonReleased |= BUTTON_TOUCH_LONGPRESS;
			input.buttonState |= BUTTON_TOUCH_LONGPRESS;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	case 2:
		if ( touchpadTimer >= timer_finger_up )
		{
			input.buttonPressed |= BUTTON_TOUCH_SINGLE;
			input.buttonReleased |= BUTTON_TOUCH_SINGLE;
			input.buttonState |= BUTTON_TOUCH_SINGLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( down )
		{
			touchState = 3;
			touchpadTimer = 0.0;
		}
		break;
	case 3:
		if ( touchpadTimer >= timer_finger_down )
		{
			touchState = 0;
			touchpadTimer = 0.0;
		}
		else if ( up )
		{
			input.buttonPressed |= BUTTON_TOUCH_DOUBLE;
			input.buttonReleased |= BUTTON_TOUCH_DOUBLE;
			input.buttonState |= BUTTON_TOUCH_DOUBLE;
			touchState = 0;
			touchpadTimer = 0.0;
		}
		break;
	}
}

void VrFrameBuilder::AddKeyEventToFrame( ovrKeyCode const keyCode, KeyEventType const eventType, int const repeatCount )
{
	if ( eventType == KEY_EVENT_NONE )
	{
		return;
	}

	LOG( "AddKeyEventToFrame: keyCode = %i, eventType = %i", keyCode, eventType );

	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = keyCode;
	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = repeatCount;
	vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = eventType;
	vrFrame.Input.NumKeyEvents++;
}

void VrFrameBuilder::AdvanceVrFrame( const ovrInputEvents & inputEvents, ovrMobile * ovr,
									const ovrJava & java,
									const ovrTrackingTransform trackingTransform,
									const long long enteredVrModeFrameNumber )
{
	const VrInput lastVrInput = vrFrame.Input;

	// check before incrementing FrameNumber because it will be the previous frame's number
	vrFrame.EnteredVrMode = vrFrame.FrameNumber == enteredVrModeFrameNumber;
	if ( vrFrame.EnteredVrMode )
	{
		// clear any state that may be left over from the pause.
		BackKeyState.Reset();
	}

	// Copy JoySticks and TouchPosition.
	for ( int i = 0; i < 2; i++ )
	{
		for ( int j = 0; j < 2; j++ )
		{
			vrFrame.Input.sticks[i][j] = inputEvents.JoySticks[i][j];
		}
		vrFrame.Input.touch[i] = inputEvents.TouchPosition[i];
	}

#if defined( OVR_OS_ANDROID )
	// Translate touch action into a touch button.
	if ( inputEvents.TouchAction == AMOTION_EVENT_ACTION_DOWN )
	{
		vrFrame.Input.buttonState |= BUTTON_TOUCH;
	}
	else if ( inputEvents.TouchAction == AMOTION_EVENT_ACTION_UP )
	{
		vrFrame.Input.buttonState &= ~BUTTON_TOUCH;
	}
#endif

	// Clear the key events.
	vrFrame.Input.NumKeyEvents = 0;

	// Handle the back key.
	const double currentTime = vrapi_GetTimeInSeconds();
	for ( int i = 0; i < inputEvents.NumKeyEvents; i++ )
	{
		// The back key is special because it has to handle short-press and long-press
		if ( inputEvents.KeyEvents[i].KeyCode == OVR_KEY_ESCAPE )
		{
			BackKeyState.HandleEvent( currentTime, inputEvents.KeyEvents[i].Down, inputEvents.KeyEvents[i].RepeatCount );
			continue;
		}
	}
	const KeyEventType backKeyEventType = BackKeyState.Update( currentTime );
	AddKeyEventToFrame( OVR_KEY_ESCAPE, backKeyEventType, 0 );

	// Copy the key events.
	for ( int i = 0; i < inputEvents.NumKeyEvents && vrFrame.Input.NumKeyEvents < MAX_KEY_EVENTS_PER_FRAME; i++ )
	{
		// The back key is already handled.
		if ( inputEvents.KeyEvents[i].KeyCode == OVR_KEY_ESCAPE )
		{
			continue;
		}
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].KeyCode = inputEvents.KeyEvents[i].KeyCode;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].RepeatCount = inputEvents.KeyEvents[i].RepeatCount;
		vrFrame.Input.KeyEvents[vrFrame.Input.NumKeyEvents].EventType = inputEvents.KeyEvents[i].Down ? KEY_EVENT_DOWN : KEY_EVENT_UP;
		vrFrame.Input.NumKeyEvents++;
	}

	// Clear previously set swipe buttons.
	vrFrame.Input.buttonState &= ~(	BUTTON_SWIPE_UP |
									BUTTON_SWIPE_DOWN |
									BUTTON_SWIPE_FORWARD |
									BUTTON_SWIPE_BACK |
									BUTTON_TOUCH_WAS_SWIPE |
									BUTTON_TOUCH_SINGLE |
									BUTTON_TOUCH_DOUBLE |
									BUTTON_TOUCH_LONGPRESS );

	// Update the joypad buttons using the key events.
	for ( int i = 0; i < inputEvents.NumKeyEvents; i++ )
	{
		const ovrKeyCode keyCode = inputEvents.KeyEvents[i].KeyCode;
		const bool down = inputEvents.KeyEvents[i].Down;

		// Keys always map to joystick buttons right now.
        for ( int j = 0; buttonMappings[j].KeyCode != OVR_KEY_MAX; j++ )
		{
			if ( keyCode == buttonMappings[j].KeyCode )
			{
				if ( down )
				{
					vrFrame.Input.buttonState |= buttonMappings[j].ButtonBit;
				}
				else
				{
					vrFrame.Input.buttonState &= ~buttonMappings[j].ButtonBit;
				}
				break;
			}
		}
		if ( down && 0 /* keyboard swipes */ )
		{
			if ( keyCode == OVR_KEY_CLOSE_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_FORWARD;
			} 
			else if ( keyCode == OVR_KEY_OPEN_BRACKET )
			{
				vrFrame.Input.buttonState |= BUTTON_SWIPE_BACK;
			}
		}
	}
	// Note joypad button transitions.
	vrFrame.Input.buttonPressed = vrFrame.Input.buttonState & ( ~lastVrInput.buttonState );
	vrFrame.Input.buttonReleased = ~vrFrame.Input.buttonState & ( lastVrInput.buttonState & ~BUTTON_TOUCH_WAS_SWIPE );

	if ( lastVrInput.buttonState & BUTTON_TOUCH_WAS_SWIPE )
	{
		if ( lastVrInput.buttonReleased & BUTTON_TOUCH )
		{
			vrFrame.Input.buttonReleased |= BUTTON_TOUCH_WAS_SWIPE;
		}
		else
		{
			// keep it around this frame
			vrFrame.Input.buttonState |= BUTTON_TOUCH_WAS_SWIPE;
		}
	}

	// Synthesize swipe gestures as buttons.
	InterpretTouchpad( vrFrame.Input, currentTime );

	// This is the only place FrameNumber gets incremented,
	// right before calling vrapi_GetPredictedDisplayTime().
	vrFrame.FrameNumber++;

	// Get the latest head tracking state, predicted ahead to the midpoint of the display refresh
	// at which the next frame will be displayed.  It will always be corrected to the real values
	// by the time warp, but the closer we get, the less black will be pulled in at the edges.
	double predictedDisplayTime = vrapi_GetPredictedDisplayTime( ovr, vrFrame.FrameNumber );

	// Make sure time always moves forward.
	if ( predictedDisplayTime <= vrFrame.PredictedDisplayTimeInSeconds )
	{
		predictedDisplayTime = vrFrame.PredictedDisplayTimeInSeconds + 0.001;
	}

	vrFrame.Tracking = vrapi_GetPredictedTracking2( ovr, predictedDisplayTime );
	vrFrame.DeltaSeconds = Alg::Clamp( (float)( predictedDisplayTime - vrFrame.PredictedDisplayTimeInSeconds ), 0.0f, 0.1f );
	vrFrame.PredictedDisplayTimeInSeconds = predictedDisplayTime;

	const ovrPosef trackingPose = vrapi_GetTrackingTransform( ovr, trackingTransform );
	const ovrPosef eyeLevelTrackingPose = vrapi_GetTrackingTransform( ovr, VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL );
	vrFrame.EyeHeight = vrapi_GetEyeHeight( &eyeLevelTrackingPose, &trackingPose );

	vrFrame.IPD = vrapi_GetInterpupillaryDistance( &vrFrame.Tracking );

	// Update device status.
	vrFrame.DeviceStatus.HeadPhonesPluggedState		= HeadPhonesPluggedState;
	vrFrame.DeviceStatus.DeviceIsDocked				= ( vrapi_GetSystemStatusInt( &java, VRAPI_SYS_STATUS_DOCKED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.HeadsetIsMounted			= ( vrapi_GetSystemStatusInt( &java, VRAPI_SYS_STATUS_MOUNTED ) != VRAPI_FALSE );
	vrFrame.DeviceStatus.PowerLevelStateThrottled	= ( vrapi_GetSystemStatusInt( &java, VRAPI_SYS_STATUS_THROTTLED ) != VRAPI_FALSE );
}

}	// namespace OVR

#if defined( OVR_OS_ANDROID )
extern "C"
{
JNIEXPORT void Java_com_oculus_vrappframework_HeadsetReceiver_headsetStateChanged( JNIEnv * jni, jclass clazz, jint state )
{
	LOG( "nativeHeadsetEvent(%i)", state );
	OVR::HeadPhonesPluggedState = ( state == 1 ) ? OVR::OVR_HEADSET_PLUGGED : OVR::OVR_HEADSET_UNPLUGGED;
}
}	// extern "C"
#endif
