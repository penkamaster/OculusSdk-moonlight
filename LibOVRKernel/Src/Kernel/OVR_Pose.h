/********************************************************************************//**
\file      OVR_Pose.h
\brief     Implementation of 3D primitives such as vectors, matrices.
\copyright Copyright 2014-2016 Oculus VR, LLC All Rights reserved.
*************************************************************************************/

#ifndef OVR_Pose_h
#define OVR_Pose_h


// This file is intended to be independent of the rest of LibOVRKernel and thus
// has no #include dependencies on either, other than OVR_Math.h which is also independent.

#include "OVR_Math.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4127) // conditional expression is constant
#endif

//------------------------------------------------------------------------------------//
// ***** C Compatibility Types

// These declarations are used to support conversion between C types used in
// LibOVRKernel C interfaces and their C++ versions. As an example, they allow passing
// Vector3f into a function that expects ovrVector3f.

typedef struct ovrRigidBodyPosef_ ovrRigidBodyPosef;

namespace OVR {

enum TrackingStatus
{
    TRACKING_STATUS_ORIENTATION_TRACKED	    = 0x0001,	// Orientation is currently tracked.
    TRACKING_STATUS_POSITION_TRACKED		= 0x0002,	// Position is currently tracked.
    TRACKING_STATUS_HMD_CONNECTED			= 0x0080	// HMD is available & connected.
};

// Forward-declare our templates.
template<class T> class RigidBodyPose;

// Specializations providing CompatibleTypes::Type value.
template<> struct CompatibleTypes<RigidBodyPose<float> >{ typedef ovrRigidBodyPosef Type; };

// RigidBodyPose describes the complete pose, or a rigid body configuration, at a
// point in time, including first and second derivatives. It is used to specify
// instantaneous location and movement of the headset.
template<class T>
class RigidBodyPose
{
public:
    typedef typename CompatibleTypes<RigidBodyPose<T> >::Type CompatibleType;

    RigidBodyPose() :
        TimeInSeconds(0.0),
        PredictionInSeconds(0.0) { }
    // float <-> double conversion constructor.
    explicit RigidBodyPose(const RigidBodyPose<typename Math<T>::OtherFloatType> &src)
        : Transform(src.Transform),
          AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds), PredictionInSeconds(src.PredictionInSeconds)
    { }

    // C-interop support: RigidBodyPosef <-> ovrRigidBodyPosef
    RigidBodyPose(const typename CompatibleTypes<RigidBodyPose<T> >::Type& src)
        : Transform(src.Pose),
          AngularVelocity(src.AngularVelocity), LinearVelocity(src.LinearVelocity),
          AngularAcceleration(src.AngularAcceleration), LinearAcceleration(src.LinearAcceleration),
          TimeInSeconds(src.TimeInSeconds), PredictionInSeconds(src.PredictionInSeconds)
    { }

    operator typename CompatibleTypes<RigidBodyPose<T> >::Type () const
    {
        CompatibleType result;
        result.Pose		            = Transform;
        result.AngularVelocity      = AngularVelocity;
        result.LinearVelocity       = LinearVelocity;
        result.AngularAcceleration  = AngularAcceleration;
        result.LinearAcceleration   = LinearAcceleration;
        result.TimeInSeconds        = TimeInSeconds;
        result.PredictionInSeconds  = PredictionInSeconds;
        return result;
    }

    Pose<T>     Transform;
    Vector3<T>  AngularVelocity;
    Vector3<T>  LinearVelocity;
    Vector3<T>  AngularAcceleration;
    Vector3<T>  LinearAcceleration;
    double      TimeInSeconds;				// Absolute time of this state sample.
    double      PredictionInSeconds;
};

typedef RigidBodyPose<float>  RigidBodyPosef;

} // Namespace OVR

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#endif
