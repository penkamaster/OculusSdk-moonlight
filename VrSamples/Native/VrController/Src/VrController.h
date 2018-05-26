/************************************************************************************

Filename    :   VrController.h
Content     :   Trivial use of the application framework.
Created     :   2/9/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef VRCONTROLLER_H
#define VRCONTROLLER_H

#include "App.h"
#include "SceneView.h"
#include "SoundEffectContext.h"
#include "GuiSys.h"
#include "VrApi_Input.h"
#include "TextureAtlas.h"
#include "BeamRenderer.h"
#include "ParticleSystem.h"
#include "OVR_ArmModel.h"
#include <vector>
#include "Ribbon.h"

namespace OVR {

class ovrLocale;
class ovrTextureAtlas;
class ovrParticleSystem;
class ovrTextureAtlas;
class ovrBeamRenderer;


typedef Array< ovrPairT< ovrParticleSystem::handle_t, ovrBeamRenderer::handle_t > > jointHandles_t;

//==============================================================
// ovrInputDeviceBase
// Abstract base class for generically tracking controllers of different types.
class ovrInputDeviceBase
{
public:
	ovrInputDeviceBase()
	{
	}

	virtual	~ovrInputDeviceBase()
	{
	}

	virtual const ovrInputCapabilityHeader *	GetCaps() const = 0;	
	virtual ovrControllerType 					GetType() const = 0;
	virtual ovrDeviceID							GetDeviceID() const = 0;
	virtual const char *						GetName() const = 0;
};

//==============================================================
// ovrInputDevice_TrackedRemote
class ovrInputDevice_TrackedRemote : public ovrInputDeviceBase
{
public:
	ovrInputDevice_TrackedRemote( const ovrInputTrackedRemoteCapabilities & caps, 
			const uint8_t lastRecenterCount )
		: ovrInputDeviceBase()
		, Caps( caps )
		, LastRecenterCount( lastRecenterCount )
	{
	}

	virtual ~ovrInputDevice_TrackedRemote()
	{
	}

	static ovrInputDeviceBase * 				Create( App & app, OvrGuiSys & guiSys, 
														VRMenu & menu, 
														const ovrInputTrackedRemoteCapabilities & capsHeader );
	virtual const ovrInputCapabilityHeader *	GetCaps() const OVR_OVERRIDE { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const OVR_OVERRIDE { return Caps.Header.Type; }
	virtual ovrDeviceID 						GetDeviceID() const OVR_OVERRIDE { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const OVR_OVERRIDE { return "TrackedRemote"; }

	ovrArmModel::ovrHandedness					GetHand() const
	{
		return ( Caps.ControllerCapabilities & ovrControllerCaps_LeftHand ) != 0 ? ovrArmModel::HAND_LEFT : ovrArmModel::HAND_RIGHT;
	}

	const ovrTracking &							GetTracking() const { return Tracking; }
	void										SetTracking( const ovrTracking & tracking ) { Tracking = tracking; }

	uint8_t										GetLastRecenterCount() const { return LastRecenterCount; }
	void										SetLastRecenterCount( const uint8_t c ) { LastRecenterCount = c; }

	const ovrDrawSurface &						GetSurface() { return Surface; };
	ovrArmModel & 								GetArmModel() { return ArmModel; }
	jointHandles_t & 							GetJointHandles() { return JointHandles; }
	ovrDrawSurface &							GetControllerSurface() { return Surface; }
	const ovrInputTrackedRemoteCapabilities &	GetTrackedRemoteCaps() const { return Caps; }

private:
	ovrInputTrackedRemoteCapabilities	Caps;
	ovrDrawSurface						Surface;
	uint8_t								LastRecenterCount;
	ovrArmModel							ArmModel;
	jointHandles_t						JointHandles;
	ovrTracking							Tracking;
};

//==============================================================
// ovrInputDevice_Headset
class ovrInputDevice_Headset : public ovrInputDeviceBase
{
public:
	ovrInputDevice_Headset( const ovrInputHeadsetCapabilities & caps )
		: ovrInputDeviceBase()
		, Caps( caps )
	{
	}

	virtual ~ovrInputDevice_Headset()
	{
	}

	static ovrInputDeviceBase * 				Create( App & app, OvrGuiSys & guiSys, 
														VRMenu & menu, 
														const ovrInputHeadsetCapabilities & headsetCapabilities );

	virtual const ovrInputCapabilityHeader *	GetCaps() const OVR_OVERRIDE { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const OVR_OVERRIDE { return Caps.Header.Type; }
	virtual ovrDeviceID  						GetDeviceID() const OVR_OVERRIDE { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const OVR_OVERRIDE { return "Headset"; }

	const ovrInputHeadsetCapabilities &			GetHeadsetCaps() const { return Caps; }

private:
	ovrInputHeadsetCapabilities	Caps;
};

//==============================================================
// ovrInputDevice_Gamepad
class ovrInputDevice_Gamepad : public ovrInputDeviceBase
{
public:
	ovrInputDevice_Gamepad( const ovrInputGamepadCapabilities & caps )
		: ovrInputDeviceBase()
		, Caps( caps )
	{
	}

	virtual ~ovrInputDevice_Gamepad()
	{
	}

	static ovrInputDeviceBase * 				Create( App & app, OvrGuiSys & guiSys,
		VRMenu & menu,
		const ovrInputGamepadCapabilities & gamepadCapabilities );

	virtual const ovrInputCapabilityHeader *	GetCaps() const OVR_OVERRIDE { return &Caps.Header; }
	virtual ovrControllerType 					GetType() const OVR_OVERRIDE { return Caps.Header.Type; }
	virtual ovrDeviceID  						GetDeviceID() const OVR_OVERRIDE { return Caps.Header.DeviceID; }
	virtual const char *						GetName() const OVR_OVERRIDE { return "Gamepad"; }

	const ovrInputGamepadCapabilities &			GetGamepadCaps() const { return Caps; }

private:
	ovrInputGamepadCapabilities	Caps;
};

//==============================================================
// ovrControllerRibbon
class ovrControllerRibbon
{
public:
	ovrControllerRibbon() = delete;
	ovrControllerRibbon( const int numPoints, const float width, const float length, const Vector4f & color );
	~ovrControllerRibbon();

	void Update( const Matrix4f & centerViewMatrix, const Vector3f & anchorPoint, const float deltaSeconds );

	ovrRibbon *		Ribbon = nullptr;
	ovrPointList *	Points = nullptr;
	ovrPointList *	Velocities = nullptr;
	int 			NumPoints = 0;
	float 			Length = 1.0f;
};

//==============================================================
// ovrVrController
class ovrVrController : public VrAppInterface
{
public:
							ovrVrController();
	virtual					~ovrVrController();

	//===============================
	// overrides
	virtual void 			Configure( ovrSettings & settings );
	virtual void			EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI );
	virtual void			LeavingVrMode();
	virtual bool 			OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType );
	virtual ovrFrameResult 	Frame( const ovrFrameInput & vrFrame );

	class OvrGuiSys &		GetGuiSys() { return *GuiSys; }
	class ovrLocale &		GetLocale() { return *Locale; }

private:
	ovrSoundEffectContext * 	SoundEffectContext;
	OvrGuiSys::SoundEffectPlayer * SoundEffectPlayer;
	OvrGuiSys *					GuiSys;
	ovrLocale *					Locale;

	ModelFile *					SceneModel;
	OvrSceneView				Scene;

	ovrTextureAtlas *			SpriteAtlas;
	ovrParticleSystem *			ParticleSystem;
	ovrTextureAtlas *			BeamAtlas;
	ovrBeamRenderer *			RemoteBeamRenderer;

	ovrBeamRenderer::handle_t	LaserPointerBeamHandle;
	ovrParticleSystem::handle_t	LaserPointerParticleHandle;
	bool						LaserHit;

	GlProgram					ProgSingleTexture;
	GlProgram					ProgLitController;
	GlProgram					ProgLitSpecularWithHighlight;
	GlProgram					ProgLitSpecular;

	ModelFile *					ControllerModelGear;
	ModelFile *					ControllerModelGearPreLit;

	ModelFile *					ControllerModelOculusGo;
	ModelFile *					ControllerModelOculusGoPreLit;

	Vector3f					SpecularLightDirection;
	Vector3f					SpecularLightColor;
	Vector3f					AmbientLightColor;
	Vector4f					HighLightMask;
	Vector3f					HighLightColor;

	double						LastGamepadUpdateTimeInSeconds;

	VRMenu *					Menu;

	// because a single Gear VR controller can be a left or right controller dependent on the
	// user's handedness (dominant hand) setting, we can't simply track controllers using a left
	// or right hand slot look up, because on any frame a Gear VR controller could change from
	// left handed to right handed and switch slots.
	std::vector< ovrInputDeviceBase* >	InputDevices;

	ovrControllerRibbon *		Ribbons[ovrArmModel::HAND_MAX];

private:
	void					ClearAndHideMenuItems();
	ovrResult				PopulateRemoteControllerInfo( ovrInputDevice_TrackedRemote & trDevice, bool recenteredController );
	void					ResetLaserPointer();

	int 					FindInputDevice( const ovrDeviceID deviceID ) const;
	void 					RemoveDevice( const ovrDeviceID deviceID );
	bool 					IsDeviceTracked( const ovrDeviceID deviceID ) const;

	void					EnumerateInputDevices();

	void 					OnDeviceConnected( const ovrInputCapabilityHeader & capsHeader );	
	void					OnDeviceDisconnected( ovrDeviceID const disconnectedDeviceID );
};

} // namespace OVR

#endif	// VRCONTROLLER_H
