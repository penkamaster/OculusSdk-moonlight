/************************************************************************************

Filename    :   OVR_Skeleton.cpp
Content     :   skeleton for arm model implementation
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Skeleton.h"

namespace OVR {

ovrSkeleton::ovrSkeleton()
{
}

const ovrJoint & ovrSkeleton::GetJoint( int const idx ) const 
{ 
	OVR_ASSERT( idx >= 0 && idx < Joints.GetSizeI() );
	return Joints[idx]; 
}

int ovrSkeleton::GetParentIndex( int const idx ) const
{
	if ( idx < 0 || idx >= Joints.GetSizeI() )
	{
		return -1;
	}
	return Joints[idx].ParentIndex;
}

void ovrSkeleton::Transform( const Posef & transformPose, const Posef & inPose, Posef & outPose )
{
	outPose.Translation = transformPose.Translation + ( transformPose.Rotation * inPose.Translation );
	outPose.Rotation = transformPose.Rotation * inPose.Rotation;
}

void ovrSkeleton::TransformByParent( const Posef & parentPose, const int jointIndex, const Posef & inPose, 
		const Array< ovrJointMod > & jointMods, Posef & outPose )
{
	OVR_ASSERT( &inPose != &outPose );

	bool appliedJointMod = false;
	for ( int i = 0; i < jointMods.GetSizeI(); ++i )
	{
		const ovrJointMod & mod = jointMods[i];
		if ( mod.Type == ovrJointMod::MOD_LOCAL && mod.JointIndex == jointIndex )
		{
			Transform( mod.Pose, inPose, outPose );
			appliedJointMod = true;
		}
	}

	if ( appliedJointMod )
	{
		Transform( parentPose, outPose, outPose );
	}
	else
	{
		Transform( parentPose, inPose, outPose );
	}

	for ( int i = 0; i < jointMods.GetSizeI(); ++i )
	{
		const ovrJointMod & mod = jointMods[i];
		if ( mod.Type == ovrJointMod::MOD_WORLD && mod.JointIndex == jointIndex )
		{
			outPose.Rotation = mod.Pose.Rotation;
		}
	}
}

void ovrSkeleton::Transform( const Posef & worldPose, const Array< ovrJoint > & inJoints, const Array< ovrJointMod > & jointMods,
		Array< ovrJoint > & outJoints )
{
	if ( outJoints.GetSizeI() != inJoints.GetSizeI() )
	{
		OVR_ASSERT( outJoints.GetSizeI() == inJoints.GetSizeI() );
		return;
	}

	TransformByParent( worldPose, 0, inJoints[0].Pose, jointMods, outJoints[0].Pose );

	for ( int i = 1; i < inJoints.GetSizeI(); ++i )
	{
		const ovrJoint & inJoint = inJoints[i];
		const ovrJoint & parentJoint = outJoints[inJoint.ParentIndex];
		ovrJoint & outJoint = outJoints[i];
		TransformByParent( parentJoint.Pose, i, inJoint.Pose, jointMods, outJoint.Pose );
	}
}

void ovrSkeleton::ApplyJointMods( const Array< ovrJointMod > & jointMods, Array< ovrJoint > & joints )
{
	for ( int i = 0; i < jointMods.GetSizeI(); ++i )
	{
		const ovrJointMod & mod = jointMods[i];
		if ( mod.Type == ovrJointMod::MOD_WORLD )
		{
			ovrJoint & joint = joints[mod.JointIndex];
			Transform( mod.Pose, joint.Pose, joint.Pose );
		}
	}
}

} // namespace OVR
