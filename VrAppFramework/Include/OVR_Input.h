/************************************************************************************

Filename    :   OVR_Input.h
Content     :   Data passed to VrAppInterface::Frame().
Created     :   February 6, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_Input_h
#define OVR_Input_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_Math.h"
#include "VrApi.h"

namespace OVR
{

// The key code for joypad key events will be the Android key code plus this bit set.
static const int BUTTON_JOYPAD_FLAG = 0x10000;

enum ovrInputDevice 
{
	OVR_INPUT_DEVICE_KEYBOARD,
	OVR_INPUT_DEVICE_JOYPAD,
	OVR_INPUT_DEVICE_REMOTE,
	OVR_INPUT_DEVICE_MAX
};

enum JoyButton
{
	BUTTON_A				= 1<<0,
	BUTTON_B				= 1<<1,
	BUTTON_X				= 1<<2,
	BUTTON_Y				= 1<<3,
	BUTTON_START			= 1<<4,
	BUTTON_BACK				= 1<<5,
	BUTTON_SELECT			= 1<<6,
	BUTTON_MENU				= 1<<7,	// currently not used or mapped to any key
	BUTTON_RIGHT_TRIGGER	= 1<<8,
	BUTTON_LEFT_TRIGGER		= 1<<9,
	BUTTON_DPAD_UP			= 1<<10,
	BUTTON_DPAD_DOWN		= 1<<11,
	BUTTON_DPAD_LEFT		= 1<<12,
	BUTTON_DPAD_RIGHT		= 1<<13,

	// The analog sticks can also be treated as binary buttons
	BUTTON_LSTICK_UP		= 1<<14,
	BUTTON_LSTICK_DOWN		= 1<<15,
	BUTTON_LSTICK_LEFT		= 1<<16,
	BUTTON_LSTICK_RIGHT		= 1<<17,

	BUTTON_RSTICK_UP		= 1<<18,
	BUTTON_RSTICK_DOWN		= 1<<19,
	BUTTON_RSTICK_LEFT		= 1<<20,
	BUTTON_RSTICK_RIGHT		= 1<<21,

	BUTTON_TOUCH			= 1<<22,		// finger on touchpad

	// Touch gestures recognized by app
	BUTTON_SWIPE_UP			= 1<<23,
	BUTTON_SWIPE_DOWN		= 1<<24,
	BUTTON_SWIPE_FORWARD	= 1<<25,
	BUTTON_SWIPE_BACK		= 1<<26,

	// After a swipe, we still have a touch release coming when the user stops touching the touchpad.
	// To allow apps to ignore the touch release, this bit is set until the frame following the touch release.
	BUTTON_TOUCH_WAS_SWIPE	= 1<<27,

	BUTTON_TOUCH_SINGLE		= 1<<28,
	BUTTON_TOUCH_DOUBLE		= 1<<29,
	BUTTON_TOUCH_LONGPRESS  = 1<<30

	/// FIXME: these extra joypad buttons aren't supported yet because the whole input pipeline 
	/// (including java) will need to be changed to use a 64-bit flag
/*
	BUTTON_C				= 1ULL<<31,
	BUTTON_Z				= 1ULL<<32,
	BUTTON_LEFT_SHOULDER	= 1ULL<<33,
	BUTTON_RIGHT_SHOULDER	= 1ULL<<34,
	BUTTON_LEFT_THUMB		= 1ULL<<35,
	BUTTON_RIGHT_THUMB		= 1ULL<<36
*/
};

enum KeyEventType
{
	KEY_EVENT_NONE,
	KEY_EVENT_SHORT_PRESS,
	KEY_EVENT_DOWN,
	KEY_EVENT_UP,
	KEY_EVENT_MAX
};

extern const char * KeyEventNames[KEY_EVENT_MAX];

static const int MAX_KEY_EVENTS_PER_FRAME = 16;
static const int MAX_SYSTEM_EVENT_MESSAGE_SIZE = 4096;

// on many keyboards, some keys will map to the same scan code
enum ovrKeyCode
{
	OVR_KEY_NONE,			// nothing

	OVR_KEY_LCONTROL,
	OVR_KEY_RCONTROL,
	OVR_KEY_LSHIFT,
	OVR_KEY_RSHIFT,
	OVR_KEY_LALT,
	OVR_KEY_RALT,
	OVR_KEY_MENU,
	OVR_KEY_MARKER_1,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_UP,				// TODO: You never get these from an android bluetooth keyboard, problem in our code somewhere though, we translate arrow keys to dpad, so looks the same as dpad on a joystick.
	OVR_KEY_DOWN,
	OVR_KEY_LEFT,
	OVR_KEY_RIGHT,
	OVR_KEY_MARKER_2,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_F1,
	OVR_KEY_F2,
	OVR_KEY_F3,
	OVR_KEY_F4,
	OVR_KEY_F5,
	OVR_KEY_F6,
	OVR_KEY_F7,
	OVR_KEY_F8,
	OVR_KEY_F9,
	OVR_KEY_F10,
	OVR_KEY_F11,
	OVR_KEY_F12,
	OVR_KEY_MARKER_3,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_RETURN,			// return (not on numeric keypad)
	OVR_KEY_SPACE,			// space bar
	OVR_KEY_INSERT,
	OVR_KEY_DELETE,
	OVR_KEY_HOME,
	OVR_KEY_END,
	OVR_KEY_PAGEUP,
	OVR_KEY_PAGEDOWN,
	OVR_KEY_SCROLL_LOCK,
	OVR_KEY_PAUSE,
	OVR_KEY_PRINT_SCREEN,
	OVR_KEY_NUM_LOCK,
	OVR_KEY_CAPSLOCK,
	OVR_KEY_ESCAPE,
	OVR_KEY_BACK = OVR_KEY_ESCAPE,	// escape and back are synonomous
	OVR_KEY_SYS_REQ,
	OVR_KEY_BREAK,
	OVR_KEY_MARKER_4,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_KP_DIVIDE,		// / (forward slash) on numeric keypad
	OVR_KEY_KP_MULTIPLY,	// * on numeric keypad
	OVR_KEY_KP_ADD,			// + on numeric keypad
	OVR_KEY_KP_SUBTRACT,	// - on numeric keypad
	OVR_KEY_KP_ENTER,		// enter on numeric keypad
	OVR_KEY_KP_DECIMAL,		// delete on numeric keypad
	OVR_KEY_KP_0,
	OVR_KEY_KP_1,
	OVR_KEY_KP_2,
	OVR_KEY_KP_3,
	OVR_KEY_KP_4,
	OVR_KEY_KP_5,
	OVR_KEY_KP_6,
	OVR_KEY_KP_7,
	OVR_KEY_KP_8,
	OVR_KEY_KP_9,
	OVR_KEY_MARKER_5,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_TAB,
	OVR_KEY_COMMA,			// ,
	OVR_KEY_PERIOD,			// .
	OVR_KEY_LESS,			// <
	OVR_KEY_GREATER,		// >
	OVR_KEY_FORWARD_SLASH,	// /
	OVR_KEY_BACK_SLASH,		/* \ */
	OVR_KEY_QUESTION_MARK,	// ?
	OVR_KEY_SEMICOLON,		// ;
	OVR_KEY_COLON,			// :
	OVR_KEY_APOSTROPHE,		// '
	OVR_KEY_QUOTE,			// "
	OVR_KEY_OPEN_BRACKET,	// [
	OVR_KEY_CLOSE_BRACKET,	// ]
	OVR_KEY_CLOSE_BRACE,	// {
	OVR_KEY_OPEN_BRACE,		// }
	OVR_KEY_BAR,			// |
	OVR_KEY_TILDE,			// ~
	OVR_KEY_GRAVE,			// `
	OVR_KEY_MARKER_6,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_1,
	OVR_KEY_2,
	OVR_KEY_3,
	OVR_KEY_4,
	OVR_KEY_5,
	OVR_KEY_6,
	OVR_KEY_7,
	OVR_KEY_8,
	OVR_KEY_9,
	OVR_KEY_0,
	OVR_KEY_EXCLAMATION,	// !
	OVR_KEY_AT,				// @
	OVR_KEY_POUND,			// #
	OVR_KEY_DOLLAR,			// $
	OVR_KEY_PERCENT,		// %
	OVR_KEY_CARET,			// ^
	OVR_KEY_AMPERSAND,		// &
	OVR_KEY_ASTERISK,		// *
	OVR_KEY_OPEN_PAREN,		// (
	OVR_KEY_CLOSE_PAREN,	// )
	OVR_KEY_MINUS,			// -
	OVR_KEY_UNDERSCORE,		// _
	OVR_KEY_PLUS,			// +
	OVR_KEY_EQUALS,			// =
	OVR_KEY_BACKSPACE,		//
	OVR_KEY_MARKER_7,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_A,
	OVR_KEY_B,
	OVR_KEY_C,
	OVR_KEY_D,
	OVR_KEY_E,
	OVR_KEY_F,
	OVR_KEY_G,
	OVR_KEY_H,
	OVR_KEY_I,
	OVR_KEY_J,
	OVR_KEY_K,
	OVR_KEY_L,
	OVR_KEY_M,
	OVR_KEY_N,
	OVR_KEY_O,
	OVR_KEY_P,
	OVR_KEY_Q,
	OVR_KEY_R,
	OVR_KEY_S,
	OVR_KEY_T,
	OVR_KEY_U,
	OVR_KEY_V,
	OVR_KEY_W,
	OVR_KEY_X,
	OVR_KEY_Y,
	OVR_KEY_Z,
	OVR_KEY_MARKER_8,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_VOLUME_MUTE,
	OVR_KEY_VOLUME_UP,
	OVR_KEY_VOLUME_DOWN,
	OVR_KEY_MEDIA_NEXT_TRACK,
	OVR_KEY_MEDIA_PREV_TRACK,
	OVR_KEY_MEDIA_STOP,
	OVR_KEY_MEDIA_PLAY_PAUSE,
	OVR_KEY_LAUNCH_APP1,
	OVR_KEY_LAUNCH_APP2,
	OVR_KEY_MARKER_9,		// DO NOT USE: just a marker to catch mismatches in the key map

	OVR_KEY_BUTTON_A,
	OVR_KEY_BUTTON_B,
	OVR_KEY_BUTTON_C,
	OVR_KEY_BUTTON_X,
	OVR_KEY_BUTTON_Y,
	OVR_KEY_BUTTON_Z,
	OVR_KEY_BUTTON_START,
	OVR_KEY_BUTTON_SELECT,
	OVR_KEY_LEFT_TRIGGER,
	OVR_KEY_BUTTON_L1 = OVR_KEY_LEFT_TRIGGER,	// FIXME: this is a poor name, but we're maintaining it for ease of conversion
	OVR_KEY_RIGHT_TRIGGER,
	OVR_KEY_BUTTON_R1 = OVR_KEY_RIGHT_TRIGGER,	// FIXME: this is a poor name, but we're maintaining it for eash of conversion
	OVR_KEY_DPAD_UP,
	OVR_KEY_DPAD_DOWN,
	OVR_KEY_DPAD_LEFT,
	OVR_KEY_DPAD_RIGHT,
	OVR_KEY_LSTICK_UP,
	OVR_KEY_LSTICK_DOWN,
	OVR_KEY_LSTICK_LEFT,
	OVR_KEY_LSTICK_RIGHT,
	OVR_KEY_RSTICK_UP,
	OVR_KEY_RSTICK_DOWN,
	OVR_KEY_RSTICK_LEFT,
	OVR_KEY_RSTICK_RIGHT,

	OVR_KEY_BUTTON_LEFT_SHOULDER,	// the button above the left trigger on MOGA / XBox / PS joypads
	OVR_KEY_BUTTON_L2 = OVR_KEY_BUTTON_LEFT_SHOULDER,
	OVR_KEY_BUTTON_RIGHT_SHOULDER,	// the button above ther right trigger on MOGA / XBox / PS joypads
	OVR_KEY_BUTTON_R2 = OVR_KEY_BUTTON_RIGHT_SHOULDER,
	OVR_KEY_BUTTON_LEFT_THUMB,		// click of the left thumbstick
	OVR_KEY_BUTTON_THUMBL = OVR_KEY_BUTTON_LEFT_THUMB,
	OVR_KEY_BUTTON_RIGHT_THUMB,		// click of the left thumbstick
	OVR_KEY_BUTTON_THUMBR = OVR_KEY_BUTTON_RIGHT_THUMB,
	OVR_KEY_MARKER_10,

	OVR_KEY_OCULUS_HOME,			// Takes the user to oculus home from anywhere

	OVR_KEY_MAX
};

char const * GetNameForKeyCode( ovrKeyCode const keyCode );
char GetAsciiForKeyCode( ovrKeyCode const keyCode, bool const shiftDown );

// Note that joypad "key events" map to joypad buttons, and will also be in the array of KeyEvents.
struct VrInput
{
		VrInput() :
			sticks(),
			touch(),
			touchRelative( 0 ),
			swipeFraction( 0.0f ),
			buttonState( 0 ),
			buttonPressed( 0 ),
			buttonReleased( 0 ),
			NumKeyEvents( 0 ),
			KeyEvents() {}

	// [0][] = left pad
	// [1][] = right pad
	// [][0] : -1 = left, 1 = right
	// [][1] = -1 = up, 1 = down
	float			sticks[2][2];

	// Most recent touchpad position, check BUTTON_TOUCH
	Vector2f		touch;
	// The relative touchpad position to when BUTTON_TOUCH started
	Vector2f		touchRelative;

	// Ranges from 0.0 - 1.0 during a swipe action.
	// Applications can provide visual feedback that a swipe
	// is being recognized.
	float			swipeFraction;

	// Bits are set for the buttons that are currently pressed down.
	unsigned int	buttonState;

	// These represent changes from the last ovrFrameInput.
	unsigned int	buttonPressed;
	unsigned int	buttonReleased;

	// Should we add button impulses, for when a button changed
	// state twice inside one frame?  For 30hz games it isn't
	// all that rare.

	// Key events.
	int				NumKeyEvents;
	struct
	{
		ovrKeyCode		KeyCode;		// Android key code from <android/keycodes.h>
		int				RepeatCount;	// > 0 if down event from a key that is held down.
		KeyEventType	EventType;		// up, down, short-press, long-press
	}	KeyEvents[MAX_KEY_EVENTS_PER_FRAME];


	// Should we add mouse input / touchpad input?
};

enum ovrHeadSetPluggedState
{
	OVR_HEADSET_PLUGGED_UNKNOWN,
	OVR_HEADSET_PLUGGED,
	OVR_HEADSET_UNPLUGGED
};

struct VrDeviceStatus
{
	VrDeviceStatus() :
		HeadPhonesPluggedState( OVR_HEADSET_PLUGGED_UNKNOWN ),
		DeviceIsDocked( false ),
		HeadsetIsMounted( false ),
		WifiIsConnected( false ),
		PowerLevelStateThrottled( false )
	{
	}

	ovrHeadSetPluggedState HeadPhonesPluggedState;					// headphones plugged state (unknown, plugged, unplugged)
	bool				DeviceIsDocked;								// true if device is docked in headset
	bool				HeadsetIsMounted;							// true if headset is mounted
	bool				WifiIsConnected;							// true if there is an active WiFi connection
	bool				PowerLevelStateThrottled;					// true if in power save mode, 30 FPS etc.
};

// Passed to an application each frame.
class ovrFrameInput
{
public:
		ovrFrameInput() :
			PredictedDisplayTimeInSeconds( 0.0 ),
			DeltaSeconds( 0.0f ),
			FrameNumber( 0 ),
			FovX( 0.0f ),
			FovY( 0.0f ),
			// FIXME: ideally EyeHeight and IPD properties would default initialize to 0.0f, but there are a handful of
			// menus which cache these values before a frame has had a chance to run.
			EyeHeight( 1.6750f ),	// average eye height above the ground when standing
			IPD( 0.0640f ),			// average interpupillary distance
			ColorTextureSwapChain(),
			TextureSwapChainIndex( 0 ),
			EnteredVrMode( false ) {}

	// Predicted absolute time in seconds this frame will be displayed.
	// To make accurate journal playback possible, applications should
	// use this time instead of geting system time directly.
	double			PredictedDisplayTimeInSeconds;

	// The amount of time in seconds that has passed since the last frame,
	// usable for movement scaling.
	// This will be clamped to no more than 0.1 seconds to prevent
	// excessive movement after pauses for loading or initialization.
	float			DeltaSeconds;

	// Incremented once for every frame.
	long long		FrameNumber;

	// Head tracking.
	// The time warp will transform from the tracking head pose to whatever
	// the current pose is when displaying the latest eye buffers.
	ovrTracking2	Tracking;		// head pose with head model applied

	// Various different joypad button combinations are mapped to
	// standard positions.
	VrInput			Input;

	// State of the device.
	VrDeviceStatus	DeviceStatus;

	float			FovX;
	float			FovY;
	float			EyeHeight;
	float			IPD;

	ovrMatrix4f		TexCoordsFromTanAngles;

	ovrTextureSwapChain *	ColorTextureSwapChain[2];

	// Index to the texture from the set that should be displayed.
	int						TextureSwapChainIndex;

	// this is true only on the first frame after entering vr mode
	bool					EnteredVrMode;
};

extern void InitInput();
extern void ShutdownInput();
extern ovrKeyCode OSKeyToKeyCode( int const osKey );

}	// namespace OVR

#endif	// OVR_Input
