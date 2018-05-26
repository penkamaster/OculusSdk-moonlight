/************************************************************************************

Filename    :   OVR_Skeleton.h
Content     :   skeleton for arm model implementation
Created     :   2/20/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_Skeleton_h
#define OVR_Skeleton_h

#include "VrApi_Types.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"

namespace OVR {

template< typename T1, typename T2 = T1 >
struct ovrPairT 
{
	ovrPairT()
		: First()
		, Second()
	{
	}

	ovrPairT( T1 first, T2 second )
		: First( first )
		, Second( second )
	{
	}

	T1	First;
	T2	Second;
};

class ovrJoint
{
public:
	ovrJoint()
		: Name( nullptr )
		, Color( 1.0f )
		, ParentIndex( -1 )
	{
	}

	ovrJoint( const char * name, const Vector4f & color, const Posef & pose, const int parentIndex )
		: Name( name )
		, Color( color )
		, Pose( pose )
		, ParentIndex( parentIndex )		
	{
	}

	char const *	Name;			// name of the joint
	Vector4f		Color;
	Posef			Pose;			// pose of this joint
	int				ParentIndex;	// index of this joint's parent
};

class ovrJointMod
{
public:
	enum ovrJointModType
	{
		MOD_INVALID = -1,
		MOD_LOCAL,	// apply in joint local space
		MOD_WORLD	// apply in world space
	};

	ovrJointMod()
		: Type( MOD_INVALID )
		, JointIndex( -1 )
	{
	}

	ovrJointMod( const ovrJointModType type, const int jointIndex, const Posef & pose )
		: Type( type )
		, JointIndex( jointIndex )
		, Pose( pose )
	{
	}

	ovrJointModType	Type;
	int				JointIndex;
	Posef			Pose;
};

class ovrSkeleton
{
public:
	ovrSkeleton();

	const ovrJoint &			GetJoint( int const idx ) const;

	int							GetParentIndex( int const idx ) const;

	Array< ovrJoint > &			GetJoints() { return Joints;  }
	const Array< ovrJoint > &	GetJoints() const { return Joints;  }

	static void					Transform( const Posef & transformPose, const Posef & inPose, Posef & outPose );

	static void					TransformByParent( const Posef & parentPose, const int jointIndex, const Posef & inPose, 
										const Array< ovrJointMod > & jointMods, Posef & outPose );
	static void					Transform( const Posef & worldPose, const Array< ovrJoint > & inJoints, 
										const Array< ovrJointMod > & jointMods, Array< ovrJoint > & outJoints );

	static void					ApplyJointMods( const Array< ovrJointMod > & jointMods, Array< ovrJoint > & joints );

private:
	Array< ovrJoint >			Joints;
};

void InitArmSkeleton( ovrSkeleton & skel );

} // namespace OVR

#endif	// OVR_Skeleton_h
