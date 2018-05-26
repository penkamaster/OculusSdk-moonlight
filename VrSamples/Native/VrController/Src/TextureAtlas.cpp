/************************************************************************************

Filename    :   TextureAtlas.h
Content     :   A simple particle system for System Activities.
Created     :   October 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "TextureAtlas.h"

namespace OVR {

ovrTextureAtlas::ovrTextureAtlas()
	: TextureWidth( 0 )
	, TextureHeight( 0 )
{
}

ovrTextureAtlas::~ovrTextureAtlas()
{
	Shutdown();
}

bool ovrTextureAtlas::BuildSpritesFromGrid( const int numSpriteColumns, const int numSpriteRows, const int numSprites )
{
	// this should be called only after loading the texture because the width and height need to be known
	OVR_ASSERT( TextureWidth > 0 && TextureHeight > 0 );

	// dimensions must be evenly divisible by grid to avoid sub-pixel uv boundaries
	OVR_ASSERT( TextureWidth % numSpriteColumns == 0 );
	OVR_ASSERT( TextureHeight % numSpriteRows == 0 );

	Sprites.Resize( 0 );

	bool done = false;
	for ( int y = 0; y < numSpriteRows; ++y )
	{
		for ( int x = 0; x < numSpriteColumns; ++x )
		{
			if ( y * numSpriteColumns + x >= numSprites )
			{
				done = true;
				break;
			}
			char name[128];
			OVR_sprintf( name, sizeof( name ), "%ix%i", x, y );

			Vector2f uvMins;
			Vector2f uvMaxs;
			GetUVsForGridCell( x, y, numSpriteColumns, numSpriteRows, 
						TextureWidth, TextureHeight, 8, 8, uvMins, uvMaxs );
			Sprites.PushBack( ovrSpriteDef(	name, uvMins, uvMaxs ) );
		}
		if ( done )
		{
			break;
		}
	}

	return true;
}

// specify sprite locations as a custom list
bool ovrTextureAtlas::Init( ovrFileSys & fileSys, const char * atlasTextureName )
{
	MemBufferT< uint8_t > buffer;
	if ( fileSys.ReadFile( atlasTextureName, buffer ) )
	{
		GL_CheckErrors( "Pre atlas texture load" );

		AtlasTexture = LoadTextureFromBuffer( atlasTextureName, MemBuffer( buffer, static_cast< int >( buffer.GetSize() ) ),
									TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), TextureWidth, TextureHeight );
		GL_CheckErrors( "Post atlas texture load" );
		if ( AtlasTexture == 0 )
		{
			return false;
		}
	}

	TextureName = atlasTextureName;

	return true;
}

bool ovrTextureAtlas::SetSpriteDefs( const Array< ovrSpriteDef > & sprites )
{
	Sprites = sprites;
	return true;
}

void ovrTextureAtlas::SetSpriteName( const int index, const char * name )
{
	Sprites[index].Name = name;
}

void ovrTextureAtlas::Shutdown()
{
	FreeTexture( AtlasTexture );
}

const ovrTextureAtlas::ovrSpriteDef & ovrTextureAtlas::GetSpriteDef( const char * spriteName ) const
{
	for ( int i = 0; i < Sprites.GetSizeI(); ++i )
	{
		if ( OVR_stricmp( Sprites[i].Name.ToCStr(), spriteName ) == 0 )
		{
			return Sprites[i];
		}
	}
	static ovrSpriteDef sd;
	return sd;
}

void ovrTextureAtlas::GetUVsForGridCell( const int x, const int y, 
		const int numColumns, const int numRows,
		const int textureWidth, const int textureHeight,
		const int borderX, const int borderY,
		Vector2f & uvMins, Vector2f & uvMaxs ) 
{
	int px = ( x * textureWidth ) / numColumns + borderX;
	int py = ( y * textureHeight ) / numRows + borderY;
	int npx = ( ( x + 1 ) * textureWidth ) / numColumns - borderX;
	int npy = ( ( y + 1 ) * textureHeight ) / numRows - borderY;

	uvMins.x = px / (float)textureWidth;
	uvMins.y = py / (float)textureHeight;
	uvMaxs.x = npx / (float)textureWidth;
	uvMaxs.y = npy / (float)textureHeight;
}

} // namespace OVR
