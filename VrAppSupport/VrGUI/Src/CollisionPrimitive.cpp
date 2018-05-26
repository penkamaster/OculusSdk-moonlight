/************************************************************************************

Filename    :   CollisionPrimitive.cpp
Content     :   Generic collision class supporting ray / triangle intersection.
Created     :   September 10, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "CollisionPrimitive.h"

#include "OVR_Geometry.h"
#include "DebugLines.h"
#include "Kernel/OVR_LogUtils.h"

namespace OVR {

//==============================
// OvrCollisionPrimitive::IntersectRayBounds
bool OvrCollisionPrimitive::IntersectRayBounds( Vector3f const & start, Vector3f const & dir, 
		Vector3f const & scale, ContentFlags_t const testContents, float & t0, float & t1 ) const
{
	if ( !( testContents & GetContents() ) )
	{
		return false;
	}

	return IntersectRayBounds( start, dir, scale, t0, t1 );
}

//==============================
// OvrCollisionPrimitive::IntersectRayBounds
bool OvrCollisionPrimitive::IntersectRayBounds( Vector3f const & start, Vector3f const & dir, 
		Vector3f const & scale, float & t0, float & t1 ) const
{
	Bounds3f scaledBounds = Bounds * scale;
	if (scaledBounds.Contains( start, 0.1f ) )
	{
		return true;
	}

	Intersect_RayBounds( start, dir, scaledBounds.GetMins(), scaledBounds.GetMaxs(), t0, t1 );

	return t0 >= 0.0f && t1 >= 0.0f && t1 >= t0;	
}

//==============================================================================================
// OvrCollisionPrimitive

//==============================
// OvrCollisionPrimitive::OvrCollisionPrimitive
OvrCollisionPrimitive::~OvrCollisionPrimitive()
{
}

//==============================================================================================
// OvrTriCollisionPrimitive

//==============================
// OvrTriCollisionPrimitive::OvrTriCollisionPrimitive
OvrTriCollisionPrimitive::OvrTriCollisionPrimitive() 
{ 
}

//==============================
// OvrTriCollisionPrimitive::OvrTriCollisionPrimitive
OvrTriCollisionPrimitive::OvrTriCollisionPrimitive( Array< Vector3f > const & vertices, 
		Array< TriangleIndex > const & indices, Array< Vector2f > const & uvs, ContentFlags_t const contents ) :
	OvrCollisionPrimitive( contents )
{
	Init( vertices, indices, uvs, contents );
}

//==============================
// OvrTriCollisionPrimitive::~OvrTriCollisionPrimitive
OvrTriCollisionPrimitive::~OvrTriCollisionPrimitive()
{
}

//==============================
// OvrTriCollisionPrimitive::Init
void OvrTriCollisionPrimitive::Init( Array< Vector3f > const & vertices, Array< TriangleIndex > const & indices, 
		Array< Vector2f > const & uvs, ContentFlags_t const contents )
{
	Vertices = vertices;
	Indices = indices;

	if ( uvs.GetSizeI() != vertices.GetSizeI() )
	{
		UVs.Resize( vertices.GetSizeI() );
		for ( int i = 0; i < vertices.GetSizeI(); ++i )
		{
			UVs[i] = Vector2f( 0.0f );
		}
	}
	else
	{
		UVs = uvs;
	}

	SetContents( contents );

	// calculate the bounds
	Bounds3f b;
	b.Clear();
	for ( int i = 0; i < vertices.GetSizeI(); ++i )
	{
		b.AddPoint( vertices[i] );
	}

	SetBounds( b );
}

//==============================
// OvrTriCollisionPrimitive::IntersectRay
bool OvrTriCollisionPrimitive::IntersectRay( Vector3f const & start, Vector3f const & dir, Posef const & pose,
		Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const
{
	if ( !( testContents & GetContents() ) )
	{
		result = OvrCollisionResult();
		return false;
	}

	// transform the ray into local space
	Vector3f localStart = ( start - pose.Translation ) * pose.Rotation.Inverted();
	Vector3f localDir = dir * pose.Rotation.Inverted();

	return IntersectRay( localStart, localDir, scale, testContents, result );
}

//==============================
// OvrTriCollisionPrimitive::IntersectRay
// the ray should already be in local space
bool OvrTriCollisionPrimitive::IntersectRay( Vector3f const & localStart, Vector3f const & localDir,
		Vector3f const & scale, ContentFlags_t const testContents, OvrCollisionResult & result ) const
{
	if ( !( testContents & GetContents() ) )
	{
		result = OvrCollisionResult();
		return false;
	}

	// test vs bounds
	float t0;
	float t1;
	if ( !IntersectRayBounds( localStart, localDir, scale, t0, t1 ) )
	{
		return false;
	}

	result.TriIndex = -1;
	for ( int i = 0; i < Indices.GetSizeI(); i += 3 )
	{
		float t_;
		float u_;
		float v_;
		Vector3f verts[3];
		verts[0] = Vertices[Indices[i]] * scale;
		verts[1] = Vertices[Indices[i + 1]] * scale;
		verts[2] = Vertices[Indices[i + 2]] * scale;

		float diff = fabsf( localDir.LengthSq() - 1.0f );
		if ( diff > Mathf::Tolerance() )
		{
			LOG( "!rayDir.IsNormalized() - ( %.4f, %.4f, %.4f ), len = %.8f, diff = %.8f", localDir.x, localDir.y, localDir.z , localDir.Length(), diff );
			OVR_ASSERT( !"IsNormalized()" );
		}

		if ( Intersect_RayTriangle( localStart, localDir, verts[0], verts[1], verts[2], t_, u_, v_ ) )
		{
			if ( t_ < result.t )
			{
				result.t = t_;

				result.TriIndex = i / 3;
				result.uv = UVs[Indices[i + 0]] * ( 1.0f - u_ - v_ ) +
							UVs[Indices[i + 1]] * u_ +
							UVs[Indices[i + 2]] * v_;				

				result.Barycentric = Vector2f( u_, v_ );
			}
		}
	}
	return result.TriIndex >= 0;
}

//==============================
// OvrTriCollisionPrimitive::DebugRender
void OvrTriCollisionPrimitive::DebugRender( OvrDebugLines & debugLines, Posef & pose, Vector3f const & scale, 
		bool const showNormals ) const
{
	Bounds3f bounds = GetBounds() * scale;
	debugLines.AddBounds( pose, bounds, Vector4f( 1.0f, 0.5f, 0.0f, 1.0f ) );

	Vector4f color( 1.0f, 0.0f, 1.0f, 1.0f );
	for ( int i = 0; i < Indices.GetSizeI(); i += 3 )
	{
		int i1 = Indices[i + 0];
		int i2 = Indices[i + 1];
		int i3 = Indices[i + 2];
		Vector3f v1 = pose.Translation + ( pose.Rotation * Vertices[i1] * scale );
		Vector3f v2 = pose.Translation + ( pose.Rotation * Vertices[i2] * scale );
		Vector3f v3 = pose.Translation + ( pose.Rotation * Vertices[i3] * scale );
		debugLines.AddLine( v1, v2, color, color, 0, true );
		debugLines.AddLine( v2, v3, color, color, 0, true );
		debugLines.AddLine( v3, v1, color, color, 0, true );

		if ( showNormals )
		{
			Vector3f edge1 = v2 - v1;
			Vector3f edge2 = v3 - v1;
			Vector3f normal = edge1.Cross( edge2 ).Normalized();
			Vector3f edgeCenter = v1 + ( edge1 * 0.5f );
			Vector3f bisection = v3 - edgeCenter;
			Vector3f center = edgeCenter + bisection * 0.5f;
			debugLines.AddLine( center, center + normal * 0.08f, Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), 0, false );		
		}
	}
}

} // namespace OVR
