/************************************************************************************

Filename    :   OVR_ArmModel.cpp
Content     :   An arm model for the tracked remote
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_ArmModel.h"
#include "Kernel/OVR_Alg.h"

namespace OVR {

ovrArmModel::ovrArmModel()
	: TorsoYaw( 0.0f )
	, TorsoTracksHead( false )
	, ForceRecenter( false )
	, ClavicleJointIdx( -1 )
	, ShoulderJointIdx( -1 )
	, ElbowJointIdx( -1 )
	, WristJointIdx( -1 ) 
{
}

void ovrArmModel::InitSkeleton()
{
	Array< ovrJoint > & joints = Skeleton.GetJoints();

	joints.PushBack( ovrJoint( "neck",		Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.0f, -0.2032f, 0.0f ) ), joints.GetSizeI() - 1 ) );
	ClavicleJointIdx = joints.GetSizeI();
	joints.PushBack( ovrJoint( "clavicle",	Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.2286f, 0.0f, 0.0f ) ), joints.GetSizeI() - 1 ) );
	ShoulderJointIdx = joints.GetSizeI();
	joints.PushBack( ovrJoint( "shoulder",	Vector4f( 1.0f, 0.0f, 1.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.0f, -0.2441f, 0.02134f ) ), joints.GetSizeI() - 1 ) );
	ElbowJointIdx = joints.GetSizeI();
	joints.PushBack( ovrJoint( "elbow",		Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.0f, 0.0f, -0.3048f ) ), joints.GetSizeI() - 1 ) );
	WristJointIdx = joints.GetSizeI();
	joints.PushBack( ovrJoint( "wrist",		Vector4f( 1.0f, 1.0f, 0.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.0f, 0.0f, -0.0762f ) ), joints.GetSizeI() - 1 ) );
	joints.PushBack( ovrJoint( "hand",		Vector4f( 1.0f, 1.0f, 0.0f, 1.0f ), Posef( Quatf(), Vector3f( 0.0f, 0.0381f, -0.0381f ) ), joints.GetSizeI() - 1 ) );

	TransformedJoints.Resize( joints.GetSizeI() );
	// copy over names and parent indices, etc.
	TransformedJoints = joints;
}

void ovrArmModel::Update( const Posef & headPose, const Posef & remotePose, 
		const ovrHandedness handedness, const bool recenteredController, 
		Posef & outPose )
{
	Matrix4f eyeMatrix( headPose );

	float eyeYaw;
	float eyePitch;
	float eyeRoll;	// ya... like, seriously???
	eyeMatrix.ToEulerAngles< Axis_Y, Axis_X, Axis_Z, Rotate_CCW, Handed_R >( &eyeYaw, &eyePitch, &eyeRoll );

	auto ConstrainTorsoYaw = []( const Quatf & headRot, const float torsoYaw )
	{
		const Vector3f worldUp( 0.0f, 1.0f, 0.0f );
		const Vector3f worldFwd( 0.0f, 0.0f, -1.0f );
	
		const Vector3f projectedHeadFwd = ( headRot * worldFwd ).ProjectToPlane( worldUp );
		if ( projectedHeadFwd.LengthSq() < 0.001f )
		{
			return torsoYaw;
		}

		// calculate the world rotation of the head on the horizon plane
		const Vector3f headFwd = projectedHeadFwd.Normalized();
		
		// calculate the world rotation of the torso
		const Quatf torsoRot( worldUp, torsoYaw );
		const Vector3f torsoFwd = torsoRot * worldFwd;

		// find the angle between the torso and head
		const float torsoMaxYawOffset = MATH_FLOAT_DEGREETORADFACTOR * 30.0f;
		const float torsoDot = torsoFwd.Dot( headFwd );
		if ( torsoDot >= cosf( torsoMaxYawOffset ) )
		{
			return torsoYaw;
		}

		// calculate the rotation of the torso when it's constrained in that direction
		const Vector3f headRight( -headFwd.z, 0.0f, headFwd.x );
		const Quatf projectedHeadRot = Quatf::FromBasisVectors( headFwd, headRight, worldUp );

		const float offsetDir = headRight.Dot( torsoFwd ) < 0.0f ? 1.0f : -1.0f;
		const float offsetYaw = torsoMaxYawOffset * offsetDir;
		const Quatf constrainedTorsoRot = projectedHeadRot * Quatf( worldUp, offsetYaw );

		// slerp torso towards the constrained rotation
		float const slerpFactor = 1.0f / 15.0f;
		const Quatf slerped = torsoRot.Slerp( constrainedTorsoRot, slerpFactor );

		float y;
		float p;
		float r;
		slerped.GetYawPitchRoll( &y, &p, &r );
		return y;
	};

	TorsoYaw = ConstrainTorsoYaw( headPose.Rotation, TorsoYaw );

	if ( ForceRecenter || TorsoTracksHead || recenteredController )
	{
		ForceRecenter = false;
		TorsoYaw = eyeYaw;
	}

	FootPose = Posef( Quatf( Vector3f( 0.0f, 1.0f, 0.0f ), TorsoYaw ), eyeMatrix.GetTranslation() );

	const float handSign = ( handedness == HAND_LEFT ? -1.0f : 1.0f );
	Skeleton.GetJoints()[ClavicleJointIdx].Pose.Translation.x = fabsf( Skeleton.GetJoints()[ClavicleJointIdx].Pose.Translation.x ) * handSign;

	Array< ovrJointMod > jointMods;

	Quatf remoteRot( remotePose.Rotation );

	const float MAX_ROLL = MATH_FLOAT_PIOVER2;
	const float MIN_PITCH = MATH_FLOAT_PIOVER2 * 0.825f;
	float remoteYaw;
	float remotePitch;
	float remoteRoll;
	remoteRot.GetYawPitchRoll( &remoteYaw, &remotePitch, &remoteRoll );
	if ( ( remoteRoll >= -MAX_ROLL && remoteRoll <= MAX_ROLL ) || ( remotePitch <= -MIN_PITCH || remotePitch >= MIN_PITCH ) )
	{
		LastUnclampedRoll = remoteRoll;
	}
	else
	{
		remoteRoll = LastUnclampedRoll;
	}

	Matrix4f m =
			Matrix4f::RotationY( remoteYaw )
			* Matrix4f::RotationX( remotePitch ) 
			* Matrix4f::RotationZ( remoteRoll );
	remoteRot = Quatf( m );

	Quatf localRemoteRot( FootPose.Rotation.Inverted() * remoteRot );

	Quatf shoulderRot = Quatf().Slerp( localRemoteRot, 0.0f );
	Quatf elbowRot = Quatf().Slerp( localRemoteRot, 0.6f );
	Quatf wristRot = Quatf().Slerp( localRemoteRot, 0.4f );

	jointMods.PushBack( ovrJointMod( ovrJointMod::MOD_LOCAL, ShoulderJointIdx, Posef( shoulderRot, Vector3f( 0.0f ) ) ) );
	jointMods.PushBack( ovrJointMod( ovrJointMod::MOD_LOCAL, ElbowJointIdx, Posef( elbowRot, Vector3f( 0.0f ) ) ) );
	jointMods.PushBack( ovrJointMod( ovrJointMod::MOD_LOCAL, WristJointIdx, Posef( wristRot, Vector3f( 0.0f ) ) ) );

	ovrSkeleton::Transform( FootPose, Skeleton.GetJoints(), jointMods, TransformedJoints );

	outPose = TransformedJoints[TransformedJoints.GetSizeI() - 1].Pose;
}

} // namespace OVR
