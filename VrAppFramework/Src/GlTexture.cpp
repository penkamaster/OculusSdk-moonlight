/************************************************************************************

Filename    :   GlTexture.cpp
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

// Make sure we get PRIu64
#define __STDC_FORMAT_MACROS 1

#include "GlTexture.h"

#include "OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

#include "VrApi.h"

#include "stb_image.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"

//#define OVR_USE_PERF_TIMER
#include "OVR_PerfTimer.h"

#include <algorithm>

#if defined( OVR_OS_ANDROID )
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR   0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR   0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR   0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR   0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR   0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR   0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR   0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR   0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR  0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR  0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR  0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD
#endif

namespace OVR {

// Not declared inline in the header to avoid having to use GL_TEXTURE_2D
GlTexture::GlTexture( const unsigned texture_, const int w, const int h )
	: texture( texture_ )
	, target( GL_TEXTURE_2D )
	, Width( w )
	, Height( h )
{
}

static int RoundUpToPow2( int i )
{
	if ( i == 0 )
	{
		return 0;
	}
	return static_cast<int>( pow( 2, ceil( log( double( i ) ) / log( 2 ) ) ) );
}

static int IntegerLog2( int i )
{
	if ( i == 0 )
	{
		return 0;
	}
	return static_cast<int>( log( double( i ) ) / log( 2.0 ) );
}

int ComputeFullMipChainNumLevels( const int width, const int height )
{
	return IntegerLog2( RoundUpToPow2( std::max( width, height ) ) );
}

static bool IsCompressedFormat( const eTextureFormat format )
{
	switch( format )
	{
		case Texture_None:
		case Texture_R:
		case Texture_RGB:
		case Texture_RGBA:
			return false;
		case Texture_DXT1:
		case Texture_DXT3:
		case Texture_DXT5:
		case Texture_PVR4bRGB:
		case Texture_PVR4bRGBA:
		case Texture_ATC_RGB:
		case Texture_ATC_RGBA:
		case Texture_ETC1:
		case Texture_ETC2_RGB:
		case Texture_ETC2_RGBA:
		case Texture_ASTC_4x4:
		case Texture_ASTC_5x4:
		case Texture_ASTC_5x5:
		case Texture_ASTC_6x5:
		case Texture_ASTC_6x6:
		case Texture_ASTC_8x5:
		case Texture_ASTC_8x6:
		case Texture_ASTC_8x8:
		case Texture_ASTC_10x5:
		case Texture_ASTC_10x6:
		case Texture_ASTC_10x8:
		case Texture_ASTC_10x10:
		case Texture_ASTC_12x10:
		case Texture_ASTC_12x12:
		case Texture_ASTC_SRGB_4x4:
		case Texture_ASTC_SRGB_5x4:
		case Texture_ASTC_SRGB_5x5:
		case Texture_ASTC_SRGB_6x5:
		case Texture_ASTC_SRGB_6x6:
		case Texture_ASTC_SRGB_8x5:
		case Texture_ASTC_SRGB_8x6:
		case Texture_ASTC_SRGB_8x8:
		case Texture_ASTC_SRGB_10x5:
		case Texture_ASTC_SRGB_10x6:
		case Texture_ASTC_SRGB_10x8:
		case Texture_ASTC_SRGB_10x10:
		case Texture_ASTC_SRGB_12x10:
		case Texture_ASTC_SRGB_12x12:
			return true;
		default:
			OVR_ASSERT( false );
			return false;
	}
}

static int GetASTCIndex( const eTextureFormat format )
{
	int const formatType = format & Texture_TypeMask;
	int const index = ( formatType - Texture_ASTC_Start ) >> 8;
	return index;
}

GLenum GetASTCInternalFormat( eTextureFormat const format )
{
	int const NUM_ASTC_FORMATS = ( Texture_ASTC_End - Texture_ASTC_Start ) >> 8;

	int const index = GetASTCIndex( format );

	GLenum internalFormats[NUM_ASTC_FORMATS] =
	{
		GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
		GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
		GL_COMPRESSED_RGBA_ASTC_5x5_KHR,
		GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
		GL_COMPRESSED_RGBA_ASTC_6x6_KHR,
		GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
		GL_COMPRESSED_RGBA_ASTC_8x6_KHR,
		GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
		GL_COMPRESSED_RGBA_ASTC_10x5_KHR,
		GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
		GL_COMPRESSED_RGBA_ASTC_10x8_KHR,
		GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
		GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
		GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
		GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR
	};
	return internalFormats[index];
}


static int GetASTCTextureSize( const eTextureFormat format, const int w, const int h, const int depth )
{
	struct blockDims_t
	{
		int x;
		int	y;
		int z;
	};

	int const NUM_ASTC_FORMATS = ( Texture_ASTC_End - Texture_ASTC_Start ) >> 8;
	blockDims_t const blockDims[NUM_ASTC_FORMATS] = 
	{
		{ 4, 4, 1 },
		{ 5, 4, 1 },
		{ 5, 5, 1 },
		{ 6, 5, 1 },
		{ 6, 6, 1 },
		{ 8, 5, 1 },
		{ 8, 6, 1 },
		{ 8, 8, 1 },
		{ 10, 5, 1 },
		{ 10, 6, 1 },
		{ 10, 8, 1 },
		{ 10, 10, 1 },
		{ 12, 10, 1 },
		{ 12, 12, 1 },
		{ 4, 4, 1 },
		{ 5, 4, 1 },
		{ 5, 5, 1 },
		{ 6, 5, 1 },
		{ 6, 6, 1 },
		{ 8, 5, 1 },
		{ 8, 6, 1 },
		{ 8, 8, 1 },
		{ 10, 5, 1 },
		{ 10, 6, 1 },
		{ 10, 8, 1 },
		{ 10, 10, 1 },
		{ 12, 10, 1 },
		{ 12, 12, 1 }
	};


	int const index = GetASTCIndex( format );

	blockDims_t const & dims = blockDims[index];

	// Compute number of blocks in each direction
	int const xblocks = ( w + dims.x - 1 ) / dims.x;
	int const yblocks = ( h + dims.y - 1 ) / dims.y;
	int const zblocks = ( depth + dims.z - 1 ) / dims.z;

	// Each block is encoded on 16 bytes, so calculate total compressed image data size.
	int const numBytes = xblocks * yblocks * zblocks * 16;
	return numBytes;
}

static int32_t GetOvrTextureSize( const eTextureFormat format, const int w, const int h )
{
    switch ( format & Texture_TypeMask )
    {
    	case Texture_R:            return w*h;
        case Texture_RGB:          return w*h*3;
        case Texture_RGBA:         return w*h*4;
        case Texture_ATC_RGB:
        case Texture_ETC1:
		case Texture_ETC2_RGB:
        case Texture_DXT1:
		{
            int bw = (w+3)/4, bh = (h+3)/4;
            return bw * bh * 8;
        }
        case Texture_ATC_RGBA:
		case Texture_ETC2_RGBA:
        case Texture_DXT3:
        case Texture_DXT5:
		{
            int bw = (w+3)/4, bh = (h+3)/4;
            return bw * bh * 16;
        }
        case Texture_PVR4bRGB:
        case Texture_PVR4bRGBA:
		{
            unsigned int width = (unsigned int)w;
            unsigned int height = (unsigned int)h;
            unsigned int min_width = 8;
	        unsigned int min_height = 8;

            // pad the dimensions
	        width = width + ((-1*width) % min_width);
	        height = height + ((-1*height) % min_height);
            unsigned int depth = 1;

            unsigned int bpp = 4;
            unsigned int bits = bpp * width * height * depth;
            return (int)(bits / 8);
        }
		case Texture_ASTC_4x4:
		case Texture_ASTC_5x4:
		case Texture_ASTC_5x5:
		case Texture_ASTC_6x5:
		case Texture_ASTC_6x6:
		case Texture_ASTC_8x5:
		case Texture_ASTC_8x6:
		case Texture_ASTC_8x8:
		case Texture_ASTC_10x5:
		case Texture_ASTC_10x6:
		case Texture_ASTC_10x8:
		case Texture_ASTC_10x10:
		case Texture_ASTC_12x10:
		case Texture_ASTC_12x12:
		case Texture_ASTC_SRGB_4x4:
		case Texture_ASTC_SRGB_5x4:
		case Texture_ASTC_SRGB_5x5:
		case Texture_ASTC_SRGB_6x5:
		case Texture_ASTC_SRGB_6x6:
		case Texture_ASTC_SRGB_8x5:
		case Texture_ASTC_SRGB_8x6:
		case Texture_ASTC_SRGB_8x8:
		case Texture_ASTC_SRGB_10x5:
		case Texture_ASTC_SRGB_10x6:
		case Texture_ASTC_SRGB_10x8:
		case Texture_ASTC_SRGB_10x10:
		case Texture_ASTC_SRGB_12x10:
		case Texture_ASTC_SRGB_12x12:
		{
			return GetASTCTextureSize( format, w, h, 1 );
		}
        default:
		{
            OVR_ASSERT( false );
            break;
		}
    }
    return 0;
}

static GlTexture CreateGlTexture( const char * fileName, const eTextureFormat format, const int width, const int height,
						const void * data, const size_t dataSize,
						const int mipcount, const bool useSrgbFormat, const bool imageSizeStored )
{
#if defined( OVR_USE_PERF_TIMER )
	LOG( "Loading '%s', w = %i, h = %i, mipcount = %i", fileName, width, height, mipcount );
#endif

	GL_CheckErrors( "pre-CreateGlTexture" );

	OVR_PERF_TIMER( CreateGlTexture );

	// LOG( "CreateGLTexture(): format %s", NameForTextureFormat( static_cast< TextureFormat >( format ) ) );

	GLenum glFormat;
	GLenum glInternalFormat;
	if ( !TextureFormatToGlFormat( format, useSrgbFormat, glFormat, glInternalFormat ) )
	{
		return GlTexture( 0, 0, 0 );
	}

	if ( mipcount <= 0 )
	{
		LOG( "%s: Invalid mip count %d", fileName, mipcount );
		return GlTexture( 0, 0, 0 );
	}

	// larger than this would require mipSize below to be a larger type
	if ( width <= 0 || width > 32768 || height <= 0 || height > 32768 )
	{
		LOG( "%s: Invalid texture size (%dx%d)", fileName, width, height );
		return GlTexture( 0, 0, 0 );
	}

	GLuint texId;
	glGenTextures( 1, &texId );
	glBindTexture( GL_TEXTURE_2D, texId );

	const unsigned char * level = (const unsigned char*)data;
	const unsigned char * endOfBuffer = level + dataSize;

	int w = width;
	int h = height;
	for ( int i = 0; i < mipcount; i++ )
	{
		int32_t mipSize = GetOvrTextureSize( format, w, h );
		if ( imageSizeStored )
		{
			mipSize = static_cast<int32_t>( *(const size_t *)level );

			level += 4;
			if ( level > endOfBuffer )
			{
				LOG( "%s: Image data exceeds buffer size", fileName );
				glBindTexture( GL_TEXTURE_2D, 0 );
				return GlTexture( texId, GL_TEXTURE_2D, width, height );
			}
		}

		if ( mipSize <= 0 || mipSize > endOfBuffer - level )
		{
			LOG( "%s: Mip level %d exceeds buffer size (%d > %td)", fileName, i, mipSize, ptrdiff_t( endOfBuffer - level ) );
			glBindTexture( GL_TEXTURE_2D, 0 );
			return GlTexture( texId, GL_TEXTURE_2D, width, height );
		}
		
		if ( IsCompressedFormat( format ) )
		{
			OVR_PERF_TIMER( CreateGlTexture_CompressedTexImage2D );
			glCompressedTexImage2D( GL_TEXTURE_2D, i, glInternalFormat, w, h, 0, mipSize, level );
			GL_CheckErrors( "Texture_Compressed" );
		}
		else
		{
			OVR_PERF_TIMER( CreateGlTexture_TexImage2D );
			glTexImage2D( GL_TEXTURE_2D, i, glInternalFormat, w, h, 0, glFormat, GL_UNSIGNED_BYTE, level );
		}

		level += mipSize;
		if ( imageSizeStored )
		{
			level += 3 - ( ( mipSize + 3 ) % 4 );
			if ( level > endOfBuffer )
			{
				LOG( "%s: Image data exceeds buffer size", fileName );
				glBindTexture( GL_TEXTURE_2D, 0 );
				return GlTexture( texId, GL_TEXTURE_2D, width, height );
			}
		}

		w >>= 1;
		h >>= 1;
		if ( w < 1 ) { w = 1; }
		if ( h < 1 ) { h = 1; }
	}

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	// Surfaces look pretty terrible without trilinear filtering
	if ( mipcount <= 1 )
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	GL_CheckErrors( "Texture load" );

	glBindTexture( GL_TEXTURE_2D, 0 );

	return GlTexture( texId, GL_TEXTURE_2D, width, height );
}

static GlTexture CreateGlCubeTexture( const char * fileName, const eTextureFormat format, const int width, const int height,
						const void * data, const size_t dataSize,
						const int mipcount, const bool useSrgbFormat, const bool imageSizeStored )
{
	OVR_ASSERT( width == height );

	GL_CheckErrors("Pre Cube Texture load");

	if ( mipcount <= 0 )
	{
		LOG( "%s: Invalid mip count %d", fileName, mipcount );
		return GlTexture( 0, 0, 0 );
	}

	// larger than this would require mipSize below to be a larger type
	if ( width <= 0 || width > 32768 || height <= 0 || height > 32768 )
	{
		LOG( "%s: Invalid texture size (%dx%d)", fileName, width, height );
		return GlTexture( 0, 0, 0 );
	}

	GLenum glFormat;
	GLenum glInternalFormat;
	if ( !TextureFormatToGlFormat( format, useSrgbFormat, glFormat, glInternalFormat ) )
	{
		LOG( "%s: TextureFormatToGlFormat 0x%x %s failed", fileName, (int)format, useSrgbFormat ? "true" : "false");
		return GlTexture( 0, 0, 0 );
	}

	GLuint texId;
	glGenTextures( 1, &texId );
	glBindTexture( GL_TEXTURE_CUBE_MAP, texId );

	const unsigned char * level = (const unsigned char*)data;
	const unsigned char * endOfBuffer = level + dataSize;

	for ( int i = 0; i < mipcount; i++ )
	{
		const int w = width >> i;
		int32_t mipSize = GetOvrTextureSize( format, w, w );
		if ( imageSizeStored )
		{
			mipSize = static_cast<int32_t>( *(const size_t *)level );
			level += 4;
			if ( level > endOfBuffer )
			{
				LOG( "%s: Image data exceeds buffer size: %p > %p", fileName, level, endOfBuffer );
				glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
				return GlTexture( texId, GL_TEXTURE_CUBE_MAP, width, height );
			}
		}

		for ( int side = 0; side < 6; side++ )
		{
			if ( mipSize <= 0 || mipSize > endOfBuffer - level )
			{
				LOG( "%s: Mip level %d exceeds buffer size (%u > %td)", fileName, i, mipSize, ptrdiff_t( endOfBuffer - level ) );
				glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
				return GlTexture( texId, GL_TEXTURE_CUBE_MAP, width, height );
			}

			if ( IsCompressedFormat( format ) )
			{
				glCompressedTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, i, glInternalFormat, w, w, 0, mipSize, level );
			}
			else
			{
				glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, i, glInternalFormat, w, w, 0, glFormat, GL_UNSIGNED_BYTE, level );
			}

			level += mipSize;
			if ( imageSizeStored )
			{
				level += 3 - ( ( mipSize + 3 ) % 4 );
				if ( level > endOfBuffer )
				{
					LOG( "%s: Image data exceeds buffer size", fileName );
					glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
					return GlTexture( texId, GL_TEXTURE_CUBE_MAP, width, height );
				}
			}
		}
	}

	// Surfaces look pretty terrible without trilinear filtering
	if ( mipcount <= 1 )
	{
		glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}
	else
	{
		glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	}
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	GL_CheckErrors( "Cube Texture load" );

	glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

	return GlTexture( texId, GL_TEXTURE_CUBE_MAP, width, height );
}

GlTexture LoadRGBATextureFromMemory( const uint8_t * texture, const int width, const int height, const bool useSrgbFormat )
{
	const size_t dataSize = GetOvrTextureSize( Texture_RGBA, width, height );
	return CreateGlTexture( "memory-RGBA", Texture_RGBA, width, height, texture, dataSize, 1, useSrgbFormat, false );
}

GlTexture LoadRGBACubeTextureFromMemory( const uint8_t * texture, const int dim, const bool useSrgbFormat )
{
	const size_t dataSize = GetOvrTextureSize( Texture_RGBA, dim, dim ) * 6;
	return CreateGlCubeTexture( "memory-CubeRGBA", Texture_RGBA, dim, dim, texture, dataSize, 1, useSrgbFormat, false );
}

GlTexture LoadRGBTextureFromMemory( const uint8_t * texture, const int width, const int height, const bool useSrgbFormat )
{
	const size_t dataSize = GetOvrTextureSize( Texture_RGB, width, height );
	return CreateGlTexture( "memory-RGB", Texture_RGB, width, height, texture, dataSize, 1, useSrgbFormat, false );
}

GlTexture LoadRTextureFromMemory( const uint8_t * texture, const int width, const int height )
{
	const size_t dataSize = GetOvrTextureSize( Texture_R, width, height );
	return CreateGlTexture( "memory-R", Texture_R, width, height, texture, dataSize, 1, false, false );
}


// .astc files are created by the reference Mali compression tool.
// As of 9/17/2016, it appears to automatically flip y, which is a good
// reason to avoid it.
struct astcHeader
{
	unsigned char		magic[4];
	unsigned char		blockDim_x;
	unsigned char		blockDim_y;
	unsigned char		blockDim_z;
	unsigned char		xsize[3];
	unsigned char		ysize[3];
	unsigned char		zsize[3];
};

GlTexture LoadASTCTextureFromMemory( uint8_t const * buffer, const size_t bufferSize, const int numPlanes, const bool useSrgbFormat )
{
	astcHeader const * header = reinterpret_cast< astcHeader const * >( buffer );

	int const w = ( (int)header->xsize[2] << 16 ) | ( (int)header->xsize[1] << 8 ) | ( (int)header->xsize[0] );
	int const h = ( (int)header->ysize[2] << 16 ) | ( (int)header->ysize[1] << 8 ) | ( (int)header->ysize[0] );

	OVR_ASSERT( numPlanes == 1 || numPlanes == 4 );
	OVR_UNUSED( numPlanes );
	if ( header->blockDim_z != 1 )
	{
		OVR_ASSERT( header->blockDim_z == 1 );
		LOG( "Only 2D ASTC textures are supported" );
		return GlTexture();
	}

	eTextureFormat format = Texture_None;
	if ( header->blockDim_x == 4 )
	{
		if ( header->blockDim_y == 4 )
		{
			format = Texture_ASTC_4x4;
		}
	}
	else if ( header->blockDim_x == 5 )
	{
		if ( header->blockDim_y == 4 )
		{
			format = Texture_ASTC_5x4;
		}
		else if ( header->blockDim_y == 5 )
		{
			format = Texture_ASTC_5x5;
		}
	}
	else if ( header->blockDim_x == 6 )
	{
		if ( header->blockDim_y == 5 )
		{
			format = Texture_ASTC_6x5;
		}
		else if ( header->blockDim_y == 6 )
		{
			format = Texture_ASTC_6x6;
		}
	}
	else if ( header->blockDim_x == 8 )
	{
		if ( header->blockDim_y == 5 )
		{
			format = Texture_ASTC_8x5;
		}
		else if ( header->blockDim_y == 6 )
		{
			format = Texture_ASTC_8x6;
		}
		else if ( header->blockDim_y == 8 )
		{
			format = Texture_ASTC_8x8;
		}
	}
	else if ( header->blockDim_x == 10 )
	{
		if ( header->blockDim_y == 5 )
		{
			format = Texture_ASTC_10x5;
		}
		else if ( header->blockDim_y == 6 )
		{
			format = Texture_ASTC_10x6;
		}
		else if ( header->blockDim_y == 8 )
		{
			format = Texture_ASTC_10x8;
		}
		else if ( header->blockDim_y == 10 )
		{
			format = Texture_ASTC_10x10;
		}
	}
	else if ( header->blockDim_x == 12 )
	{
		if ( header->blockDim_y == 10 )
		{
			format = Texture_ASTC_12x10;
		}
		else if ( header->blockDim_y == 12 )
		{
			format = Texture_ASTC_12x12;
		}
	}

	if ( format == Texture_None )
	{
		OVR_ASSERT( format != Texture_None );
		LOG( "Unhandled ASTC block size: %i x %i", header->blockDim_x, header->blockDim_y );
		return GlTexture();
	}
	return CreateGlTexture("memory-ASTC", format, w, h, buffer + sizeof(struct astcHeader), 
		bufferSize - sizeof(struct astcHeader), 1, useSrgbFormat, false);
}
/*

PVR Container Format

Offset    Size       Name           Description
0x0000    4 [DWORD]  Version        0x03525650
0x0004    4 [DWORD]  Flags          0x0000 if no flags set
                                    0x0002 if colors within the texture
0x0008    8 [Union]  Pixel Format   This can either be one of several predetermined enumerated
                                    values (a DWORD) or a 4-character array and a 4-byte array (8 bytes).
                                    If the most significant 4 bytes of the 64-bit (8-byte) value are all zero,
                                    then it indicates that it is the enumeration with the following values:
                                    Value  Pixel Type
                                    0      PVRTC 2bpp RGB
                                    1      PVRTC 2bpp RGBA
                                    2      PVRTC 4bpp RGB
                                    3      PVRTC 4bpp RGBA
                                    4      PVRTC-II 2bpp
                                    5      PVRTC-II 4bpp
                                    6      ETC1
                                    7      DXT1 / BC1
                                    8      DXT2
                                    9      DXT3 / BC2
                                    10     DXT4
                                    11     DXT5 / BC3
                                    12     BC4
                                    13     BC5
                                    14     BC6
                                    15     BC7
                                    16     UYVY
                                    17     YUY2
                                    18     BW1bpp
                                    19     R9G9B9E5 Shared Exponent
                                    20     RGBG8888
                                    21     GRGB8888
                                    22     ETC2 RGB
                                    23     ETC2 RGBA
                                    24     ETC2 RGB A1
                                    25     EAC R11 Unsigned
                                    26     EAC R11 Signed
                                    27     EAC RG11 Unsigned
                                    28     EAC RG11 Signed
                                    If the most significant 4 bytes are not zero then the 8-byte character array
                                    indicates the pixel format as follows:
                                    The least significant 4 bytes indicate channel order, such as:
                                    { 'b', 'g', 'r', 'a' } or { 'b', 'g', 'r', '\0' }
                                    The most significant 4 bytes indicate the width of each channel in bits, as follows:
                                    { 4, 4, 4, 4 } or { 2, 2, 2, 2 }, or {5, 5, 5, 0 }
0x0010  4 [DWORD]    Color Space    This is an enumerated field, currently two values:
                                    Value   Color Space
                                    0       Linear RGB
                                    1       Standard RGB
0x0014  4 [DWORD]    Channel Type   This is another enumerated field:
                                    Value   Data Type
                                    0       Unsigned Byte Normalized
                                    1       Signed Byte Normalized
                                    2       Unsigned Byte
                                    3       Signed Byte
                                    4       Unsigned Short Normalized
                                    5       Signed Short Normalized
                                    6       Unsigned Short
                                    7       Signed Short
                                    8       Unsigned Integer Normalized
                                    9       Signed Integer Normalized
                                    10      Unsigned Integer
                                    11      Signed Integer
                                    12      Float (no size specified)
0x0018  4 [DWORD]    Height         Height of the image.
0x001C  4 [DWORD]    Width          Width of the image.
0x0020  4 [DWORD]    Depth          Depth of the image, in pixels.
0x0024  4 [DWORD]    Surface Count  The number of surfaces to this texture, used for texture arrays.
0x0028  4 [DWORD]    Face Count     The number of faces to this texture, used for cube maps.
0x002C  4 [DWORD]    MIP-Map Count  The number of MIP-Map levels, including a top level.
0x0030  4 [DWORD]    Metadata Size  The size, in bytes, of meta data that immediately follows this header.

*/

#pragma pack(1)
struct OVR_PVR_HEADER
{
    UInt32	Version;
    UInt32	Flags;
    UInt64  PixelFormat;
    UInt32  ColorSpace;
    UInt32  ChannelType;
    UInt32	Height;
    UInt32	Width;
    UInt32	Depth;
    UInt32  NumSurfaces;
    UInt32  NumFaces;
    UInt32  MipMapCount;
    UInt32  MetaDataSize;
};
#pragma pack()

GlTexture LoadTexturePVR( const char * fileName, const unsigned char * buffer, const int bufferLength,
						bool useSrgbFormat, bool noMipMaps, int & width, int & height )
{
	width = 0;
	height = 0;

	if ( bufferLength < ( int )( sizeof( OVR_PVR_HEADER ) ) )
	{
		LOG( "%s: Invalid PVR file", fileName );
		return GlTexture( 0, 0, 0 );
	}

	const OVR_PVR_HEADER & header = *( OVR_PVR_HEADER * )buffer;
	if ( header.Version != 0x03525650 )
	{
		LOG( "%s: Invalid PVR file version", fileName );
		return GlTexture( 0, 0, 0 );
	}

	eTextureFormat format = Texture_None;
	switch ( header.PixelFormat )
	{
		case 2:						format = Texture_PVR4bRGB;	break;
		case 3:						format = Texture_PVR4bRGBA;	break;
		case 6:						format = Texture_ETC1;		break;
		case 22:					format = Texture_ETC2_RGB;	break;
		case 23:					format = Texture_ETC2_RGBA;	break;
		case 578721384203708274llu:	format = Texture_RGBA;		break;
		default:
			LOG( "%s: Unknown PVR texture format %" PRIu64 ", size %ix%i", fileName, header.PixelFormat, width, height );
			return GlTexture( 0, 0, 0 );
	}

	// skip the metadata
	const UInt32 startTex = sizeof( OVR_PVR_HEADER ) + header.MetaDataSize;
	if ( ( startTex < sizeof( OVR_PVR_HEADER ) ) || ( startTex >= static_cast< size_t >( bufferLength ) ) )
	{
		LOG( "%s: Invalid PVR header sizes", fileName );
		return GlTexture( 0, 0, 0 );
	}

	const UInt32 mipCount = ( noMipMaps ) ? 1 : OVR::Alg::Max( static_cast<UInt32>( 1u ), header.MipMapCount );

	width = header.Width;
	height = header.Height;

	if ( header.NumFaces == 1 )
	{
		return CreateGlTexture( fileName, format, width, height, buffer + startTex, bufferLength - startTex, mipCount, useSrgbFormat, false );
	}
	else if ( header.NumFaces == 6 )
	{
		return CreateGlCubeTexture( fileName, format, width, height, buffer + startTex, bufferLength - startTex, mipCount, useSrgbFormat, false );
	}
	else
	{
		LOG( "%s: PVR file has unsupported number of faces %d", fileName, header.NumFaces );
	}

	width = 0;
	height = 0;
	return GlTexture( 0, 0, 0 );
}


unsigned char * LoadPVRBuffer( const char * fileName, int & width, int & height )
{
	width = 0;
	height = 0;

	MemBufferFile bufferFile( fileName );

	MemBuffer buffer = bufferFile.ToMemBuffer();

	if ( buffer.Length < ( int )( sizeof( OVR_PVR_HEADER ) ) )
	{
		LOG( "Invalid PVR file" );
		buffer.FreeData();
		return NULL;
	}

	const OVR_PVR_HEADER & header = *( OVR_PVR_HEADER * )buffer.Buffer;
	if ( header.Version != 0x03525650 )
	{
		LOG( "Invalid PVR file version" );
		buffer.FreeData();
		return NULL;
	}

	eTextureFormat format = Texture_None;
	switch ( header.PixelFormat )
	{
 		case 578721384203708274llu:	format = Texture_RGBA;		break;
		default:
			LOG( "Unknown PVR texture format %" PRIu64 ", size %ix%i", header.PixelFormat, width, height );
			buffer.FreeData();
			return NULL;
	}

	// skip the metadata
	const UInt32 startTex = sizeof( OVR_PVR_HEADER ) + header.MetaDataSize;
	if ( ( startTex < sizeof( OVR_PVR_HEADER ) ) || ( startTex >= static_cast< size_t >( buffer.Length ) ) )
	{
		LOG( "Invalid PVR header sizes" );
		buffer.FreeData();
		return NULL;
	}

	size_t mipSize = GetOvrTextureSize( format, header.Width, header.Height );
	
	const int outBufferSizeBytes = buffer.Length - startTex;

	if ( outBufferSizeBytes < 0 || mipSize > static_cast< size_t >( outBufferSizeBytes ) )
	{
		buffer.FreeData();
		return NULL;
	}

	width = header.Width;
	height = header.Height;

	// skip the metadata
	unsigned char * outBuffer = ( unsigned char * )malloc( outBufferSizeBytes );
	memcpy( outBuffer, ( unsigned char * )buffer.Buffer + startTex, outBufferSizeBytes );
	buffer.FreeData();
	
	return outBuffer;
}


/*

KTX Container Format

KTX is a format for storing textures for OpenGL and OpenGL ES applications.
It is distinguished by the simplicity of the loader required to instantiate
a GL texture object from the file contents.

Byte[12] identifier
UInt32 endianness
UInt32 glType
UInt32 glTypeSize
UInt32 glFormat
Uint32 glInternalFormat
Uint32 glBaseInternalFormat
UInt32 pixelWidth
UInt32 pixelHeight
UInt32 pixelDepth
UInt32 numberOfArrayElements
UInt32 numberOfFaces
UInt32 numberOfMipmapLevels
UInt32 bytesOfKeyValueData
  
for each keyValuePair that fits in bytesOfKeyValueData
    UInt32   keyAndValueByteSize
    Byte     keyAndValue[keyAndValueByteSize]
    Byte     valuePadding[3 - ((keyAndValueByteSize + 3) % 4)]
end
  
for each mipmap_level in numberOfMipmapLevels*
    UInt32 imageSize;
    for each array_element in numberOfArrayElements*
       for each face in numberOfFaces
           for each z_slice in pixelDepth*
               for each row or row_of_blocks in pixelHeight*
                   for each pixel or block_of_pixels in pixelWidth
                       Byte data[format-specific-number-of-bytes]**
                   end
               end
           end
           Byte cubePadding[0-3]
       end
    end
    Byte mipPadding[3 - ((imageSize + 3) % 4)]
end

*/

#pragma pack(1)
struct OVR_KTX_HEADER
{
	UByte	identifier[12];
	UInt32	endianness;
	UInt32	glType;
	UInt32	glTypeSize;
	UInt32	glFormat;
	UInt32	glInternalFormat;
	UInt32	glBaseInternalFormat;
	UInt32	pixelWidth;
	UInt32	pixelHeight;
	UInt32	pixelDepth;
	UInt32	numberOfArrayElements;
	UInt32	numberOfFaces;
	UInt32	numberOfMipmapLevels;
	UInt32	bytesOfKeyValueData;
};
#pragma pack()

GlTexture LoadTextureKTX( const char * fileName, const unsigned char * buffer, const int bufferLength,
						bool useSrgbFormat, bool noMipMaps, int & width, int & height )
{
	width = 0;
	height = 0;

	if ( bufferLength < (int)( sizeof( OVR_KTX_HEADER ) ) )
	{
    	LOG( "%s: Invalid KTX file", fileName );
        return GlTexture( 0, 0, 0 );
	}

	const char fileIdentifier[12] =
	{
		'\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n'
	};

	const OVR_KTX_HEADER & header = *(OVR_KTX_HEADER *)buffer;
	if ( memcmp( header.identifier, fileIdentifier, sizeof( fileIdentifier ) ) != 0 )
	{
		LOG( "%s: Invalid KTX file", fileName );
		return GlTexture( 0, 0, 0 );
	}
	// only support little endian
	if ( header.endianness != 0x04030201 )
	{
		LOG( "%s: KTX file has wrong endianess", fileName );
		return GlTexture( 0, 0, 0 );
	}
	// only support compressed or unsigned byte
	if ( header.glType != 0 && header.glType != GL_UNSIGNED_BYTE )
	{
		LOG( "%s: KTX file has unsupported glType %d", fileName, header.glType );
		return GlTexture( 0, 0, 0 );
	}
	// no support for texture arrays
	if ( header.numberOfArrayElements != 0 )
	{
		LOG( "%s: KTX file has unsupported number of array elements %d", fileName, header.numberOfArrayElements );
		return GlTexture( 0, 0, 0 );
	}

	// derive the texture format from the GL format
	eTextureFormat format = Texture_None;
	if ( !GlFormatToTextureFormat( format, header.glFormat, header.glInternalFormat ) )
	{
		LOG( "%s: KTX file has unsupported glFormat %d, glInternalFormat %d", fileName, header.glFormat, header.glInternalFormat );
		return GlTexture( 0, 0, 0 );
	}

	// skip the key value data
	const uintptr_t startTex = sizeof( OVR_KTX_HEADER ) + header.bytesOfKeyValueData;
	if ( ( startTex < sizeof( OVR_KTX_HEADER ) ) || ( startTex >= static_cast< size_t >( bufferLength ) ) )
	{
		LOG( "%s: Invalid KTX header sizes", fileName );
		return GlTexture( 0, 0, 0 );
	}

	width = header.pixelWidth;
	height = header.pixelHeight;

	const UInt32 mipCount = ( noMipMaps ) ? 1 : OVR::Alg::Max( static_cast<UInt32>( 1u ), header.numberOfMipmapLevels );

	if ( header.numberOfFaces == 1 )
	{
		return CreateGlTexture( fileName, format, width, height, buffer + startTex, bufferLength - startTex, mipCount, useSrgbFormat, true );
	}
	else if ( header.numberOfFaces == 6 )
	{
		return CreateGlCubeTexture( fileName, format, width, height, buffer + startTex, bufferLength - startTex, mipCount, useSrgbFormat, true );
	}
	else
	{
		LOG( "%s: KTX file has unsupported number of faces %d", fileName, header.numberOfFaces );
	}

	width = 0;
	height = 0;
	return GlTexture( 0, 0, 0 );
}

unsigned char * LoadImageToRGBABuffer( const char * fileName, const unsigned char * inBuffer, const size_t inBufferLen,
		int & width, int & height )
{
	const String ext = String( fileName ).GetExtension().ToLower();

	width = 0;
	height = 0;

	if (	ext == ".jpg" || ext == ".tga" ||
			ext == ".png" || ext == ".bmp" ||
			ext == ".psd" || ext == ".gif" ||
			ext == ".hdr" || ext == ".pic" )
	{
		// Uncompressed files loaded by stb_image
		int comp;
		stbi_uc * image = stbi_load_from_memory( (unsigned char *)inBuffer, (int)inBufferLen, &width, &height, &comp, 4 );
		return image;
	}
	return nullptr;
}

void FreeRGBABuffer( const unsigned char * buffer )
{
	stbi_image_free( (void*)buffer );
}

static int MipLevelsForSize( int width, int height )
{
	int levels = 1;

	while ( width > 1 || height > 1 )
	{
		levels++;
		width >>= 1;
		height >>= 1;
	}
	return levels;
}

GlTexture LoadTextureFromBuffer( const char * fileName, const MemBuffer & buffer,
		const TextureFlags_t & flags, int & width, int & height )
{
	const String ext = String( fileName ).GetExtension().ToLower();

	// LOG( "Loading texture buffer %s (%s), length %i", fileName, ext.ToCStr(), buffer.Length );

	GlTexture texId;
	width = 0;
	height = 0;

	if ( fileName == NULL || buffer.Buffer == NULL || buffer.Length < 1 )
	{
		// can't load anything from an empty buffer
	}
	else if (	ext == ".jpg" || ext == ".tga" ||
				ext == ".png" || ext == ".bmp" ||
				ext == ".psd" || ext == ".gif" ||
				ext == ".hdr" || ext == ".pic" )
	{
		// Uncompressed files loaded by stb_image
		int comp;
		stbi_uc * image = stbi_load_from_memory( (unsigned char *)buffer.Buffer, buffer.Length, &width, &height, &comp, 4 );
		if ( image != NULL )
		{
			// Optionally outline the border alpha.
			if ( flags & TEXTUREFLAG_ALPHA_BORDER )
			{
				for ( int i = 0 ; i < width ; i++ )
				{
					image[i*4+3] = 0;
					image[((height-1)*width+i)*4+3] = 0;
				}
				for ( int i = 0 ; i < height ; i++ )
				{
					image[i*width*4+3] = 0;
					image[(i*width+width-1)*4+3] = 0;
				}
			}

			const size_t dataSize = GetOvrTextureSize( Texture_RGBA, width, height );
			texId = CreateGlTexture( fileName, Texture_RGBA, width, height, image, dataSize,
				( flags & TEXTUREFLAG_NO_MIPMAPS ) ? 1 : MipLevelsForSize( width, height ),
				flags & TEXTUREFLAG_USE_SRGB, false );
			free( image );
			if ( !( flags & TEXTUREFLAG_NO_MIPMAPS ) )
			{
				glBindTexture( texId.target, texId.texture );
				glGenerateMipmap( texId.target );
				glTexParameteri( texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			}
	    }
		else
		{
			LOG( "stbi_load_from_memory() failed!" );
		}
	}
	else if ( ext == ".pvr" )
	{
		texId = LoadTexturePVR( fileName, (const unsigned char *)buffer.Buffer, buffer.Length,
						( flags & TEXTUREFLAG_USE_SRGB ),
						( flags & TEXTUREFLAG_NO_MIPMAPS ),
						width, height );
	}
	else if ( ext == ".ktx" )
	{
		texId = LoadTextureKTX( fileName, (const unsigned char *)buffer.Buffer, buffer.Length,
						( flags & TEXTUREFLAG_USE_SRGB ),
						( flags & TEXTUREFLAG_NO_MIPMAPS ),
						width, height );
	}
	else if ( ext == ".astc" )
	{
		texId = LoadASTCTextureFromMemory( (const unsigned char*)buffer.Buffer, buffer.Length, 4, flags & TEXTUREFLAG_USE_SRGB );
	}
	else if ( ext == ".pkm" )
	{
		LOG( "PKM format not supported" );
	}
	else
	{
		LOG( "unsupported file extension '%s', for file '%s'", ext.ToCStr(), fileName );
	}

	// Create a default texture if the load failed
	if ( texId.texture == 0 )
	{
    	WARN( "Failed to load %s", fileName );
    	if ( ( flags & TEXTUREFLAG_NO_DEFAULT ) == 0 )
    	{
			static uint8_t defaultTexture[8 * 8 * 3] =
			{
					255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64, 255,255,255, 255,255,255,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64, 255,255,255, 255,255,255,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64,  64, 64, 64, 255,255,255,
					255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255, 255,255,255
			};
			texId = LoadRGBTextureFromMemory( defaultTexture, 8, 8, flags & TEXTUREFLAG_USE_SRGB );
    	}
	}

	return texId;
}

GlTexture LoadTextureFromOtherApplicationPackage( void * zipFile, const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height )
{
	width = 0;
	height = 0;
	if ( zipFile == 0 )
	{
		return GlTexture( 0, 0, 0 );
	}

	void * 	buffer;
	int		bufferLength;

	ovr_ReadFileFromOtherApplicationPackage( zipFile, nameInZip, bufferLength, buffer );
	if ( !buffer )
	{
		return GlTexture( 0, 0, 0 );
	}
	GlTexture texId = LoadTextureFromBuffer( nameInZip, MemBuffer( buffer, bufferLength ),
			flags, width, height );
	free( buffer );
	return texId;
}

GlTexture LoadTextureFromApplicationPackage( const char * nameInZip,
									const TextureFlags_t & flags, int & width, int & height )
{
	return LoadTextureFromOtherApplicationPackage( ovr_GetApplicationPackageFile(), nameInZip, flags, width, height );
}

GlTexture LoadTextureFromUri( class ovrFileSys & fileSys, const char * uri, 
					const TextureFlags_t & flags, int & width, int & height )
{
	MemBufferT< uint8_t > buffer;
	if ( !fileSys.ReadFile( uri, buffer ) )
	{
		return GlTexture();
	}

	return LoadTextureFromBuffer( uri, MemBuffer( buffer, static_cast< int >( buffer.GetSize() ) ),
			flags, width, height );
}

void FreeTexture( GlTexture texId )
{
	if ( texId.texture )
	{
		glDeleteTextures( 1, &texId.texture );
	}
}

void DeleteTexture( GlTexture & texture )
{
	if ( texture.texture != 0 )
	{
		glDeleteTextures( 1, &texture.texture );
		texture.texture = 0;
		texture.target = 0;
		texture.Width = 0;
		texture.Height = 0;
	}
}

void MakeTextureClamped( GlTexture texId )
{
	glBindTexture( texId.target, texId.texture );
	glTexParameteri( texId.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( texId.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( texId.target, 0 );
}

void MakeTextureLodClamped( GlTexture texId, int maxLod )
{
	glBindTexture( texId.target, texId.texture );
   	glTexParameteri( texId.target, GL_TEXTURE_MAX_LEVEL, maxLod );
	glBindTexture( texId.target, 0 );
}

void MakeTextureTrilinear( GlTexture texId )
{
	glBindTexture( texId.target, texId.texture );
   	glTexParameteri( texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindTexture( texId.target, 0 );
}

void MakeTextureLinearNearest( GlTexture texId )
{
	glBindTexture( texId.target, texId.texture );
   	glTexParameteri( texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	glTexParameteri( texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindTexture( texId.target, 0 );
}

void MakeTextureLinear( GlTexture texId )
{
	glBindTexture( texId.target, texId.texture );
   	glTexParameteri( texId.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( texId.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindTexture( texId.target, 0 );
}

void MakeTextureAniso( GlTexture texId, float maxAniso )
{
	glBindTexture( texId.target, texId.texture );
   	glTexParameterf( texId.target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso );
	glBindTexture( texId.target, 0 );
}

void BuildTextureMipmaps( GlTexture texId )
{
	glBindTexture( texId.target, texId.texture );
	glGenerateMipmap( texId.target );
	glBindTexture( texId.target, 0 );
}

}	// namespace OVR
