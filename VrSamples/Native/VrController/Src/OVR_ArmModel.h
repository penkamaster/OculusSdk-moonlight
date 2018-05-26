/************************************************************************************

Filename    :   OVR_ArmModel.h
Content     :   An arm model for the tracked remote
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#if !defined( OVR_ArmModel_h )
#define OVR_ArmModel_h

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi_Types.h"
#include "OVR_Skeleton.h"

namespace OVR {

class ovrArmModel
{
public:
	enum ovrHandedness
	{
		HAND_UNKNOWN = -1,
		HAND_LEFT,
		HAND_RIGHT,
		HAND_MAX
	};

	ovrArmModel();

	void				InitSkeleton();

	void				Update( const Posef & headPose, const Posef & remotePose, const ovrHandedness handedness, 
								const bool recenteredController, Posef & outPose );

	const ovrSkeleton &			GetSkeleton() const { return Skeleton; }
	const Array< ovrJoint > &	GetTransformedJoints() const { return TransformedJoints; }

private:
	ovrSkeleton			Skeleton;
	Posef				FootPose;
	Array< ovrJoint >	TransformedJoints;

	float				LastUnclampedRoll;
	
	float				TorsoYaw;			// current yaw of the torso
	bool				TorsoTracksHead;	// true to make the torso track the head
	bool				ForceRecenter;		// true to force the torso to the head yaw

	int					ClavicleJointIdx;
	int					ShoulderJointIdx;
	int					ElbowJointIdx;
	int					WristJointIdx;
};

} // namespace OVR

#endif
