/************************************************************************************

Filename    :   GlTexture_Windows.cpp
Content     :   OpenGL texture loading.
Created     :   September 30, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "GlTexture.h"

#if defined( OVR_OS_WIN32 )

namespace OVR {

extern GLenum GetASTCInternalFormat( eTextureFormat const format );

bool TextureFormatToGlFormat( const eTextureFormat format, const bool useSrgbFormat, GLenum & glFormat, GLenum & glInternalFormat )
{
    switch ( format & Texture_TypeMask )
    {
		case Texture_RGB:
		{
			glFormat = GL_RGB; 
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_SRGB8; 
//				LOG( "GL texture format is GL_RGB / GL_SRGB8" );
			}
			else
			{
				glInternalFormat = GL_RGB8; 
//				LOG( "GL texture format is GL_RGB / GL_RGB" );
			}
			return true;
		}
		case Texture_RGBA:
		{
			glFormat = GL_RGBA; 
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_SRGB8_ALPHA8; 
//				LOG( "GL texture format is GL_RGBA / GL_SRGB8_ALPHA8" );
			}
			else
			{
				glInternalFormat = GL_RGBA8; 
//				LOG( "GL texture format is GL_RGBA / GL_RGBA" );
			}
			return true;
		}
		case Texture_R:
		{
			glInternalFormat = GL_R8;
			glFormat = GL_RED;
//			LOG( "GL texture format is GL_R8" );
			return true;
		}
		case Texture_DXT1:
		{
			glFormat = glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
//			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
			return true;
		}
		case Texture_DXT3:
		{
			glFormat = glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
//			LOG( "GL texture format is GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" );
			return true;
		}
		case Texture_ETC2_RGB:
		{
			glFormat = GL_RGB;
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_COMPRESSED_SRGB8_ETC2;
			}
			else
			{
				glInternalFormat = GL_COMPRESSED_RGB8_ETC2;
			}
			return true;
		}
		case Texture_ETC2_RGBA:
		{
			glFormat = GL_RGBA;
			if ( useSrgbFormat )
			{
				glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
//				LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC " );
			}
			else
			{
				glInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
//				LOG( "GL texture format is GL_RGBA / GL_COMPRESSED_RGBA8_ETC2_EAC " );
			}
			return true;
		}
	// unsupported on OpenGL ES:
	//    case Texture_DXT3:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
	//    case Texture_DXT5:  glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;

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
			glFormat = GL_RGBA;
			glInternalFormat = GetASTCInternalFormat( format );
			return true;
		}
    }
	return false;
}

bool GlFormatToTextureFormat( eTextureFormat & format, const GLenum glFormat, const GLenum glInternalFormat )
{
	if ( glFormat == GL_RED && glInternalFormat == GL_R8 )
	{
		format = Texture_R;
		return true;
	}
	if ( glFormat == GL_RGB && ( glInternalFormat == GL_RGB || glInternalFormat == GL_RGB8 || glInternalFormat == GL_SRGB8 ) )
	{
		format = Texture_RGB;
		return true;
	}
	if ( glFormat == GL_RGBA && ( glInternalFormat == GL_RGBA || glInternalFormat == GL_RGBA8 || glInternalFormat == GL_SRGB8_ALPHA8 ) )
	{
		format = Texture_RGBA;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ) && glInternalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT )
	{
		format = Texture_DXT1;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ) && glInternalFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT )
	{
		format = Texture_DXT3;
		return true;
	}
	if ( glFormat == 0 && glInternalFormat == GL_COMPRESSED_RGB8_ETC2 )
	{
		format = Texture_ETC2_RGB;
		return true;
	}
	if ( ( glFormat == 0 || glFormat == GL_RGBA ) && ( glInternalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC || glInternalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC ) )
	{
		format = Texture_ETC2_RGBA;
		return true;
	}
	if ( glFormat == 0 || glFormat == GL_RGBA )
	{
		if (glInternalFormat >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR && glInternalFormat <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR)
		{
			format = (eTextureFormat)(Texture_ASTC_4x4 + ((glInternalFormat - GL_COMPRESSED_RGBA_ASTC_4x4_KHR) << 8));
			return true;
		}
		if (glInternalFormat >= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR && glInternalFormat <= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)
		{
			format = (eTextureFormat)(Texture_ASTC_SRGB_4x4 + ((glInternalFormat - GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR) << 8));
			return true;
		}
	}

	return false;
}

}	// namespace OVR

#endif
