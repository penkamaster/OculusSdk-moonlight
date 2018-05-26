/************************************************************************************

Filename    :   GlGeometry.cpp
Content     :   OpenGL geometry setup.
Created     :   October 8, 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "GlGeometry.h"

#include "Kernel/OVR_Alg.h"
#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Array.h"
#include "OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

/*
 * These are all built inside VertexArrayObjects, so no GL state other
 * than the VAO binding should be disturbed.
 *
 */
namespace OVR
{

unsigned GlGeometry::IndexType = ( sizeof( TriangleIndex ) == 2 ) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

template< typename _attrib_type_ >
void PackVertexAttribute( Array< uint8_t > & packed, const Array< _attrib_type_ > & attrib,
				const int glLocation, const int glType, const int glComponents )
{
	if ( attrib.GetSize() > 0 )
	{
		const size_t offset = packed.GetSize();
		const size_t size = attrib.GetSize() * sizeof( attrib[0] );

		packed.Resize( offset + size );
		memcpy( &packed[offset], attrib.GetDataPtr(), size );

		glEnableVertexAttribArray( glLocation );
		glVertexAttribPointer( glLocation, glComponents, glType, false, sizeof( attrib[0] ), (void *)( offset ) );
	}
	else
	{
		glDisableVertexAttribArray( glLocation );
	}
}

void GlGeometry::Create( const VertexAttribs & attribs, const Array< TriangleIndex > & indices )
{
	vertexCount = attribs.position.GetSizeI();
	indexCount = indices.GetSizeI();

	glGenBuffers( 1, &vertexBuffer );
	glGenBuffers( 1, &indexBuffer );
	glGenVertexArrays( 1, &vertexArrayObject );
	glBindVertexArray( vertexArrayObject );
	glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );

	Array< uint8_t > packed;
	PackVertexAttribute( packed, attribs.position,		VERTEX_ATTRIBUTE_LOCATION_POSITION,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.normal,		VERTEX_ATTRIBUTE_LOCATION_NORMAL,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.tangent,		VERTEX_ATTRIBUTE_LOCATION_TANGENT,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.binormal,		VERTEX_ATTRIBUTE_LOCATION_BINORMAL,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.color,			VERTEX_ATTRIBUTE_LOCATION_COLOR,			GL_FLOAT,	4 );
	PackVertexAttribute( packed, attribs.uv0,			VERTEX_ATTRIBUTE_LOCATION_UV0,				GL_FLOAT,	2 );
	PackVertexAttribute( packed, attribs.uv1,			VERTEX_ATTRIBUTE_LOCATION_UV1,				GL_FLOAT,	2 );
	PackVertexAttribute( packed, attribs.jointIndices,	VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES,	GL_INT,		4 );
	PackVertexAttribute( packed, attribs.jointWeights,	VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS,	GL_FLOAT,	4 );

	glBufferData( GL_ARRAY_BUFFER, packed.GetSize() * sizeof( packed[0] ), packed.GetDataPtr(), GL_STATIC_DRAW );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.GetSizeI() * sizeof( indices[0] ), indices.GetDataPtr(), GL_STATIC_DRAW );

	glBindVertexArray( 0 );

	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_POSITION );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_NORMAL );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_TANGENT );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_BINORMAL );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_COLOR );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV0 );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_UV1 );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES );
	glDisableVertexAttribArray( VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS );

	localBounds.Clear();
	for ( int i = 0; i < vertexCount; i++ )
	{
		localBounds.AddPoint( attribs.position[i] );
	}
}

void GlGeometry::Update( const VertexAttribs & attribs, const bool updateBounds )
{
	vertexCount = attribs.position.GetSizeI();

	glBindVertexArray( vertexArrayObject );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );

	Array< uint8_t > packed;
	PackVertexAttribute( packed, attribs.position,		VERTEX_ATTRIBUTE_LOCATION_POSITION,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.normal,		VERTEX_ATTRIBUTE_LOCATION_NORMAL,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.tangent,		VERTEX_ATTRIBUTE_LOCATION_TANGENT,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.binormal,		VERTEX_ATTRIBUTE_LOCATION_BINORMAL,			GL_FLOAT,	3 );
	PackVertexAttribute( packed, attribs.color,			VERTEX_ATTRIBUTE_LOCATION_COLOR,			GL_FLOAT,	4 );
	PackVertexAttribute( packed, attribs.uv0,			VERTEX_ATTRIBUTE_LOCATION_UV0,				GL_FLOAT,	2 );
	PackVertexAttribute( packed, attribs.uv1,			VERTEX_ATTRIBUTE_LOCATION_UV1,				GL_FLOAT,	2 );
	PackVertexAttribute( packed, attribs.jointIndices,	VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES,	GL_INT,		4 );
	PackVertexAttribute( packed, attribs.jointWeights,	VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS,	GL_FLOAT,	4 );

	glBufferData( GL_ARRAY_BUFFER, packed.GetSize() * sizeof( packed[0] ), packed.GetDataPtr(), GL_STATIC_DRAW );

	if ( updateBounds )
	{
		localBounds.Clear();
		for ( int i = 0; i < vertexCount; i++ )
		{
			localBounds.AddPoint( attribs.position[i] );
		}
	}
}

void GlGeometry::Free()
{
	glDeleteVertexArrays( 1, &vertexArrayObject );
	glDeleteBuffers( 1, &indexBuffer );
	glDeleteBuffers( 1, &vertexBuffer );

	indexBuffer = 0;
	vertexBuffer = 0;
	vertexArrayObject = 0;
	vertexCount = 0;
	indexCount = 0;

	localBounds.Clear();
}

GlGeometry BuildTesselatedQuad( const TriangleIndex horizontal, const TriangleIndex vertical,
		const bool twoSided )
{
	const int vertexCount = ( horizontal + 1 ) * ( vertical + 1 );

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y <= vertical; y++ )
	{
		const float yf = (float) y / (float) vertical;
		for ( int x = 0; x <= horizontal; x++ )
		{
			const float xf = (float) x / (float) horizontal;
			const int index = y * ( horizontal + 1 ) + x;
			attribs.position[index].x = -1.0f + xf * 2.0f;
			attribs.position[index].y = -1.0f + yf * 2.0f;
			attribs.position[index].z = 0.0f;
			attribs.uv0[index].x = xf;
			attribs.uv0[index].y = 1.0f - yf;
			for ( int i = 0; i < 4; i++ )
			{
				attribs.color[index][i] = 1.0f;
			}
			// fade to transparent on the outside
			if ( x == 0 || x == horizontal || y == 0 || y == vertical )
			{
				attribs.color[index][3] = 0.0f;
			}
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 * ( twoSided ? 2 : 1 ) );

	// If this is to be used to draw a linear format texture, like
	// a surface texture, it is better for cache performance that
	// the triangles be drawn to follow the side to side linear order.
	int index = 0;
	for ( TriangleIndex y = 0; y < vertical; y++ )
	{
		for ( TriangleIndex x = 0; x < horizontal; x++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
		// fix the quads in the upper right and lower left corners so that the triangles in the 
		// quads share the edges going from the center of the tesselated quad to it's corners.
		const int upperLeftIndexStart = 0;
		indices[upperLeftIndexStart + 1] = indices[upperLeftIndexStart + 5];
		indices[upperLeftIndexStart + 3] = indices[upperLeftIndexStart + 0];

		const int lowerRightIndexStart = ( horizontal * ( vertical - 1 ) * 6 ) + ( horizontal - 1 ) * 6;
		indices[lowerRightIndexStart + 1] = indices[lowerRightIndexStart + 5];
		indices[lowerRightIndexStart + 3] = indices[lowerRightIndexStart + 0];
	}
	if ( twoSided )
	{
		for ( TriangleIndex y = 0; y < vertical; y++ )
		{
			for ( TriangleIndex x = 0; x < horizontal; x++ )
			{
				indices[index + 5] = y * (horizontal + 1) + x;
				indices[index + 4] = y * (horizontal + 1) + x + 1;
				indices[index + 3] = (y + 1) * (horizontal + 1) + x;
				indices[index + 2] = (y + 1) * (horizontal + 1) + x;
				indices[index + 1] = y * (horizontal + 1) + x + 1;
				indices[index + 0] = (y + 1) * (horizontal + 1) + x + 1;
				index += 6;
			}
		}
	}
	return GlGeometry( attribs, indices );
}

GlGeometry BuildTesselatedCylinder( const float radius, const float height, const TriangleIndex horizontal, const TriangleIndex vertical, const float uScale, const float vScale )
{
	const int vertexCount = ( horizontal + 1 ) * ( vertical + 1 );

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y <= vertical; ++y )
	{
		const float yf = (float) y / (float) vertical;
		for ( int x = 0; x <= horizontal; ++x )
		{
			const float xf = (float) x / (float) horizontal;
			const int index = y * ( horizontal + 1 ) + x;
			attribs.position[index].x = cosf( MATH_FLOAT_PI * 2 * xf ) * radius;
			attribs.position[index].y = sinf( MATH_FLOAT_PI * 2 * xf ) * radius;
			attribs.position[index].z = -height + yf * 2 * height;
			attribs.uv0[index].x = xf * uScale;
			attribs.uv0[index].y = ( 1.0f - yf ) * vScale;
			for ( int i = 0; i < 4; ++i )
			{
				attribs.color[index][i] = 1.0f;
			}
			// fade to transparent on the outside
			if ( y == 0 || y == vertical )
			{
				attribs.color[index][3] = 0.0f;
			}
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 );

	// If this is to be used to draw a linear format texture, like
	// a surface texture, it is better for cache performance that
	// the triangles be drawn to follow the side to side linear order.
	int index = 0;
	for ( TriangleIndex y = 0; y < vertical; y++ )
	{
		for ( TriangleIndex x = 0; x < horizontal; x++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
	}

	return GlGeometry( attribs, indices );
}

// To guarantee that the edge pixels are completely black, we need to
// have a band of solid 0.  Just interpolating to 0 at the edges will
// leave some pixels with low color values.  This stuck out as surprisingly
// visible smears from the distorted edges of the eye renderings in
// some cases.
GlGeometry BuildVignette( const float xFraction, const float yFraction )
{
	// Leave 25% of the vignette as solid black
	const float posx[] = { -1.001f, -1.0f + xFraction * 0.25f, -1.0f + xFraction, 1.0f - xFraction, 1.0f - xFraction * 0.25f, 1.001f };
	const float posy[] = { -1.001f, -1.0f + yFraction * 0.25f, -1.0f + yFraction, 1.0f - yFraction, 1.0f - yFraction * 0.25f, 1.001f };

	const int vertexCount = 6 * 6;

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y < 6; y++ )
	{
		for ( int x = 0; x < 6; x++ )
		{
			const int index = y * 6 + x;
			attribs.position[index].x = posx[x];
			attribs.position[index].y = posy[y];
			attribs.position[index].z = 0.0f;
			attribs.uv0[index].x = 0.0f;
			attribs.uv0[index].y = 0.0f;
			// the outer edges will have 0 color
			const float c = ( y <= 1 || y >= 4 || x <= 1 || x >= 4 ) ? 0.0f : 1.0f;
			for ( int i = 0; i < 3; i++ )
			{
				attribs.color[index][i] = c;
			}
			attribs.color[index][3] = 1.0f;	// solid alpha
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( 24 * 6 );

	int index = 0;
	for ( TriangleIndex x = 0; x < 5; x++ )
	{
		for ( TriangleIndex y = 0; y < 5; y++ )
		{
			if ( x == 2 && y == 2 )
			{
				continue;	// the middle is open
			}
			// flip triangulation at corners
			if ( x == y )
			{
				indices[index + 0] = y * 6 + x;
				indices[index + 1] = (y + 1) * 6 + x + 1;
				indices[index + 2] = (y + 1) * 6 + x;
				indices[index + 3] = y * 6 + x;
				indices[index + 4] = y * 6 + x + 1;
				indices[index + 5] = (y + 1) * 6 + x + 1;
			}
			else
			{
				indices[index + 0] = y * 6 + x;
				indices[index + 1] = y * 6 + x + 1;
				indices[index + 2] = (y + 1) * 6 + x;
				indices[index + 3] = (y + 1) * 6 + x;
				indices[index + 4] = y * 6 + x + 1;
				indices[index + 5] = (y + 1) * 6 + x + 1;
			}
			index += 6;
		}
	}

	return GlGeometry( attribs, indices );
}

GlGeometry BuildDome( const float latRads, const float uScale, const float vScale )
{
	const int horizontal = 64;
	const int vertical = 32;
	const float radius = 100.0f;

	const int vertexCount = ( horizontal + 1 ) * ( vertical + 1 );

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y <= vertical; y++ )
	{
		const float yf = (float) y / (float) vertical;
		const float lat = MATH_FLOAT_PI - yf * latRads - 0.5f * MATH_FLOAT_PI;
		const float cosLat = cosf( lat );
		for ( int x = 0; x <= horizontal; x++ )
		{
			const float xf = (float) x / (float) horizontal;
			const float lon = ( 0.5f + xf ) * MATH_FLOAT_PI * 2;
			const int index = y * ( horizontal + 1 ) + x;

			if ( x == horizontal )
			{
				// Make sure that the wrap seam is EXACTLY the same
				// xyz so there is no chance of pixel cracks.
				attribs.position[index] = attribs.position[y * ( horizontal + 1 ) + 0];
			}
			else
			{
				attribs.position[index].x = radius * cosf( lon ) * cosLat;
				attribs.position[index].y = radius * sinf( lat );
				attribs.position[index].z = radius * sinf( lon ) * cosLat;
			}

			attribs.uv0[index].x = xf * uScale;
			attribs.uv0[index].y = ( 1.0f - yf ) * vScale;
			for ( int i = 0; i < 4; i++ )
			{
				attribs.color[index][i] = 1.0f;
			}
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 );

	int index = 0;
	for ( TriangleIndex x = 0; x < horizontal; x++ )
	{
		for ( TriangleIndex y = 0; y < vertical; y++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
	}

	return GlGeometry( attribs, indices );
}

// Build it with the equirect center down -Z
GlGeometry BuildGlobe( const float uScale /*= 1.0f*/, const float vScale /*= 1.0f*/, const float radius /*= 100.0f*/ )
{
	// Make four rows at the polar caps in the place of one
	// to diminish the degenerate triangle issue.
	const int poleVertical = 3;
	const int uniformVertical = 64;
	const int horizontal = 128;
	const int vertical = uniformVertical + poleVertical*2;

	const int vertexCount = ( horizontal + 1 ) * ( vertical + 1 );

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y <= vertical; y++ )
	{
		float yf;
		if ( y <= poleVertical )
		{
			yf = (float)y / (poleVertical+1) / uniformVertical;
		}
		else if ( y >= vertical - poleVertical )
		{
			yf = (float) (uniformVertical - 1 + ( (float)( y - (vertical - poleVertical - 1) ) / ( poleVertical+1) ) ) / uniformVertical;
		}
		else
		{
			yf = (float) ( y - poleVertical ) / uniformVertical;
		}
		const float lat = ( yf - 0.5f ) * MATH_FLOAT_PI;
		const float cosLat = cosf( lat );
		for ( int x = 0; x <= horizontal; x++ )
		{
			const float xf = (float) x / (float) horizontal;
			const float lon = ( 0.25f + xf ) * MATH_FLOAT_PI * 2;
			const int index = y * ( horizontal + 1 ) + x;

			if ( x == horizontal )
			{
				// Make sure that the wrap seam is EXACTLY the same
				// xyz so there is no chance of pixel cracks.
				attribs.position[index] = attribs.position[y * ( horizontal + 1 ) + 0];
			}
			else
			{
				attribs.position[index].x = radius * cosf( lon ) * cosLat;
				attribs.position[index].y = radius * sinf( lat );
				attribs.position[index].z = radius * sinf( lon ) * cosLat;
			}

			// With a normal mapping, half the triangles degenerate at the poles,
			// which causes seams between every triangle.  It is better to make them
			// a fan, and only get one seam.
			if ( y == 0 || y == vertical )
			{
				attribs.uv0[index].x = 0.5f;
			}
			else
			{
				attribs.uv0[index].x = xf * uScale;
			}
			attribs.uv0[index].y = ( 1.0f - yf ) * vScale;
			for ( int i = 0; i < 4; i++ )
			{
				attribs.color[index][i] = 1.0f;
			}
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 );

	int index = 0;
	for ( TriangleIndex x = 0; x < horizontal; x++ )
	{
		for ( TriangleIndex y = 0; y < vertical; y++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
	}

	return GlGeometry( attribs, indices );
}

GlGeometry BuildSpherePatch( const float fov )
{
	const int horizontal = 64;
	const int vertical = 64;
	const float radius = 100.0f;

	const int vertexCount = (horizontal + 1) * (vertical + 1);

	VertexAttribs attribs;
	attribs.position.Resize( vertexCount );
	attribs.uv0.Resize( vertexCount );
	attribs.color.Resize( vertexCount );

	for ( int y = 0; y <= vertical; y++ )
	{
		const float yf = (float) y / (float) vertical;
		const float lat = ( yf - 0.5f ) * fov;
		const float cosLat = cosf(lat);
		for ( int x = 0; x <= horizontal; x++ )
		{
			const float xf = (float) x / (float) horizontal;
			const float lon = ( xf - 0.5f ) * fov;
			const int index = y * ( horizontal + 1 ) + x;

			attribs.position[index].x = radius * cosf( lon ) * cosLat;
			attribs.position[index].y = radius * sinf( lat );
			attribs.position[index].z = radius * sinf( lon ) * cosLat;

			// center in the middle of the screen for roll rotation
			attribs.uv0[index].x = xf - 0.5f;
			attribs.uv0[index].y = ( 1.0f - yf ) - 0.5f;

			for ( int i = 0 ; i < 4 ; i++ )
			{
				attribs.color[index][i] = 1.0f;
			}
		}
	}

	Array< TriangleIndex > indices;
	indices.Resize( horizontal * vertical * 6 );

	int index = 0;
	for ( TriangleIndex x = 0; x < horizontal; x++ )
	{
		for ( TriangleIndex y = 0; y < vertical; y++ )
		{
			indices[index + 0] = y * (horizontal + 1) + x;
			indices[index + 1] = y * (horizontal + 1) + x + 1;
			indices[index + 2] = (y + 1) * (horizontal + 1) + x;
			indices[index + 3] = (y + 1) * (horizontal + 1) + x;
			indices[index + 4] = y * (horizontal + 1) + x + 1;
			indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
			index += 6;
		}
	}

	return GlGeometry( attribs, indices );
}

GlGeometry BuildUnitCubeLines()
{
	VertexAttribs attribs;
	attribs.position.Resize( 8 );

	for ( int i = 0; i < 8; i++) {
		attribs.position[i][0] = static_cast<float>( i & 1 );
		attribs.position[i][1] = static_cast<float>( ( i & 2 ) >> 1 );
		attribs.position[i][2] = static_cast<float>( ( i & 4 ) >> 2 );
	}

	const TriangleIndex staticIndices[24] = { 0,1, 1,3, 3,2, 2,0, 4,5, 5,7, 7,6, 6,4, 0,4, 1,5, 3,7, 2,6 };

	Array< TriangleIndex > indices;
	indices.Resize( 24 );
	memcpy( &indices[0], staticIndices, 24 * sizeof( indices[0] ) );

	GlGeometry g = GlGeometry( attribs, indices );
	g.primitiveType = GL_LINES;
	return g;
}

}	// namespace OVR
