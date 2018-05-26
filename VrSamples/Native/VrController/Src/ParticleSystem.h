/************************************************************************************

Filename    :   ParticleSystem.h
Content     :   A simple particle system for System Activities.
Created     :   October 12, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#if !defined( OVR_PARTICLESYSTEM_H )
#define OVR_PARTICLESYSTEM_H

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "OVR_Input.h"
#include "SurfaceRender.h"
#include "GlProgram.h"
#include "OVR_FileSys.h"
#include "EaseFunctions.h"

#define USE_STD_VECTOR
//#define USE_SIMPLE_ARRAY

#if defined( USE_STD_VECTOR )

#include <vector>

#elif defined( USE_SIMPLE_ARRAY )

#include "OVR_SimpleArray.h"
#define USE_SIMPLE_ATTRIBS

#endif

namespace OVR {

// typedef float (*ovrAlphaFunc_t)( const double t );

#if defined( USE_STD_VECTOR )

#define USE_SIMPLE_ARRAY
#define USE_SIMPLE_ATTRIBS

template< typename T >
class ovrSimpleArray
{
public:
	ovrSimpleArray()
	{
	}

	void		PushBack( T const & item )
	{
		Vector.push_back( item );
	}

	void		PopBack()
	{
		Vector.pop_back();
	}

	void		Reserve( int const newCapacity )
	{
		Vector.reserve( newCapacity );
	}

	void		Resize( int const newCount )
	{
		Vector.resize( newCount );
	}

	void		RemoveAtUnordered( int const index )
	{
		Vector[index] = Vector[Vector.size() - 1];
		Vector.pop_back();
	}

	int			GetSizeI() const { return static_cast< int >( Vector.size() ); }
	size_t		GetSize() const { return Vector.size(); }
	int			GetCapacity() const { return Vector.capacity(); }

	T const *	GetDataPtr() const { return &Vector[0]; }
	T *			GetDataPtr() { return &Vector[0]; }

	T const & operator[] ( int const index ) const
	{ 
		return Vector[index]; 
	}
	T & operator[] ( int const index ) 
	{ 
		return Vector[index]; 
	}

private:
	std::vector< T >	Vector;
};

#endif

#if defined( USE_SIMPLE_ATTRIBS )
struct ovrVertexAttribs
{
	OVR::ovrSimpleArray< OVR::Vector3f > position;
	OVR::ovrSimpleArray< OVR::Vector3f > normal;
	OVR::ovrSimpleArray< OVR::Vector3f > tangent;
	OVR::ovrSimpleArray< OVR::Vector3f > binormal;
	OVR::ovrSimpleArray< OVR::Vector4f > color;
	OVR::ovrSimpleArray< OVR::Vector2f > uv0;
	OVR::ovrSimpleArray< OVR::Vector2f > uv1;
	OVR::ovrSimpleArray< OVR::Vector4i > jointIndices;
	OVR::ovrSimpleArray< OVR::Vector4f > jointWeights;
};
#endif

class ovrTextureAtlas;

struct particleDerived_t {
	Vector3f 	Pos;
	Vector4f	Color;
	float		Orientation;	// roll angle in radians
	float		Scale;
	uint16_t	SpriteIndex;
};

struct particleSort_t {
	int		ActiveIndex;
	float	DistanceSq;
};

//==============================================================
// ovrParticleSystem
class ovrParticleSystem {
public:
	enum ovrParticleIndex
	{
		INVALID_PARTICLE_INDEX = -1
	};

	typedef TypesafeNumberT< int32_t, ovrParticleIndex, INVALID_PARTICLE_INDEX > handle_t;

	ovrParticleSystem();
	virtual ~ovrParticleSystem();

	// specify sprite locations as a regular grid
	void				Init( const int maxParticles, const ovrTextureAtlas & atlas, const ovrGpuState & gpuState, 
								bool const sortParticles );

	void				Frame( const ovrFrameInput & frame, const ovrTextureAtlas & textureAtlas, 
								const Matrix4f & centerEyeViewMatrix );

	void				Shutdown();

	void				RenderEyeView( Matrix4f const & viewMatrix, Matrix4f const & projectionMatrix,
								Array< ovrDrawSurface > & surfaceList ) const;

	handle_t		 	AddParticle( const ovrFrameInput & frame, 
								const Vector3f & initialPosition, const float initialOrientation, 
								const Vector3f & initialVelocity, const Vector3f & acceleration, 
								const Vector4f & initialColor, const ovrEaseFunc easeFunc, const float rotationRate, 
								const float scale, const float lifeTime, const uint16_t spriteIndex );
	
	void				UpdateParticle( const ovrFrameInput & frame, const handle_t handle,
								const Vector3f & position, const float orientation, 
								const Vector3f & velocity, const Vector3f & acceleration, 
								const Vector4f & color, const ovrEaseFunc easeFunc, const float rotationRate, 
								const float scale, const float lifeTime, const uint16_t spriteIndex );

	void				RemoveParticle( const handle_t handle );

	static ovrGpuState	GetDefaultGpuState();

private:
	void			CreateGeometry( const int maxParticles );

	int				GetMaxParticles() const { return SurfaceDef.geo.vertexCount / 4; }

	class ovrParticle {
	public:
		// empty constructor so we don't pay the price for double initialization
		ovrParticle() { }

		double			StartTime;			// time particle was created, negative means this particle is invalid
		float			LifeTime;			// time particle should die
		Vector3f		InitialPosition;	// initial position of the particle
		float			InitialOrientation;	// initial orientation of the particle
		Vector3f		InitialVelocity;	// initial velocity of the particle
		Vector3f		HalfAcceleration;	// 1/2 the initial acceleration of the particle
		Vector4f		InitialColor;		// initial color of the particle
		float			RotationRate;		// rotation of the particle
		float			InitialScale;		// initial scale of the particle
		uint16_t		SpriteIndex;		// index of the sprite for this particle
		ovrEaseFunc		EaseFunc;			// parametric function used to compute alpha
	};

	int						MaxParticles;	// maximum allowd particles

#if defined( USE_SIMPLE_ARRAY )
	ovrSimpleArray< ovrParticle >		Particles;		// all active particles
	ovrSimpleArray< handle_t >	FreeParticles;	// indices of free particles
	ovrSimpleArray< handle_t >	ActiveParticles;// indices of active particles
	ovrSimpleArray< particleDerived_t >	Derived;
	ovrSimpleArray< particleSort_t >	SortIndices;

#if defined( USE_SIMPLE_ATTRIBS )
	ovrSimpleArray< uint8_t >			PackedAttr;
	ovrVertexAttribs					Attr;
#else
	VertexAttribs				Attr;
#endif

#else
	ArrayPOD< ovrParticle >			Particles;		// all active particles
	ArrayPOD< handle_t >	FreeParticles;	// indices of free particles
	ArrayPOD< handle_t >	ActiveParticles;// indices of active particles
	ArrayPOD< particleDerived_t >	Derived;
	ArrayPOD< particleSort_t >		SortIndices;
	VertexAttribs					Attr;
#endif

	GlProgram 				Program;
	ovrSurfaceDef			SurfaceDef;
	Matrix4f				ModelMatrix;
	
	bool					SortParticles;
};

} // namespace OVR

#endif // OVR_PARTICLESYSTEM_H
