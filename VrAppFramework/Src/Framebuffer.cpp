/************************************************************************************

Filename    :   Framebuffer.cpp
Content     :   Framebuffer
Created     :   July 3rd, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "Framebuffer.h"
#include "Kernel/OVR_LogUtils.h"
#include "VrApi.h"

#define MALI_SEPARATE_DEPTH_BUFFERS		1

namespace OVR
{
ovrFramebuffer::ovrFramebuffer( const ovrTextureFormat colorFormat, const ovrTextureFormat depthFormat,
								const int width, const int height, const int multisamples,
								const bool resolveDepth, const bool useMultiview_ ) :
		UseMultiview( useMultiview_ ),
		Width( width ),
		Height( height ),
		TextureSwapChainLength( 0 ),
		TextureSwapChainIndex( 0 ),
		ColorTextureSwapChain( NULL ),
		DepthTextureSwapChain( NULL ),
		ColorBuffer( 0 ),
		DepthBuffers( NULL ),
		RenderFrameBuffers( NULL ),
		ResolveFrameBuffers( NULL )
{
	enum multisample_t
	{
		MSAA_OFF,
		MSAA_RENDER_TO_TEXTURE,	// GL_multisampled_render_to_texture_IMG / EXT
		MSAA_BLIT				// GL ES 3.0 explicit resolve
	};

	multisample_t multisampleMode = MSAA_OFF;

	if ( UseMultiview )
	{
		if ( multisamples > 1 && glFramebufferTextureMultisampleMultiviewOVR_ != NULL )
		{
			OVR_ASSERT( resolveDepth == false );
			multisampleMode = MSAA_RENDER_TO_TEXTURE;
			LOG( "multisample mode = MSAA_RENDER_TO_TEXTURE" );
		}
		else
		{
			multisampleMode = MSAA_OFF;
			LOG( "multisample mode = MSAA_OFF" );
		}

		// Create the color texture set and associated color buffer.
		{
			ColorTextureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D_ARRAY, colorFormat, width, height, 1, true );
			OVR_ASSERT( ColorTextureSwapChain != NULL );
			TextureSwapChainLength = vrapi_GetTextureSwapChainLength( ColorTextureSwapChain );
			OVR_ASSERT( TextureSwapChainLength );
			TextureSwapChainIndex = 0;
		}

		// Create the depth texture set and associated depth buffer.
		if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
		{
			// GL_DEPTH_COMPONENT16 is the only strictly legal thing in unextended GL ES 2.0
			// The GL_OES_depth24 extension allows GL_DEPTH_COMPONENT24_OES.
			// The GL_OES_packed_depth_stencil extension allows GL_DEPTH24_STENCIL8_OES.
			GLenum glInternalFormat;
			switch ( depthFormat )
			{
				case VRAPI_TEXTURE_FORMAT_DEPTH_16:				glInternalFormat = GL_DEPTH_COMPONENT16; break;
				case VRAPI_TEXTURE_FORMAT_DEPTH_24:				glInternalFormat = GL_DEPTH_COMPONENT24; break;
				case VRAPI_TEXTURE_FORMAT_DEPTH_24_STENCIL_8:	glInternalFormat = GL_DEPTH24_STENCIL8; break;
				default: FAIL( "Unknown depthFormat %i", depthFormat );
			}
			// FIXME: we should only need one depth buffer but the Mali driver
			// does not like sharing the depth buffer between multiple frame buffers
			DepthBuffers = new GLuint[TextureSwapChainLength];
			for ( int i = 0; i < ( MALI_SEPARATE_DEPTH_BUFFERS ? TextureSwapChainLength : 1 ); i++ )
			{
				glGenTextures( 1, &DepthBuffers[i] );
				glBindTexture( GL_TEXTURE_2D_ARRAY, DepthBuffers[i] );
				glTexStorage3D( GL_TEXTURE_2D_ARRAY, 1, glInternalFormat, width, height, 2 );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
			}
		}

		// Create the framebuffer.
		RenderFrameBuffers = new GLuint[TextureSwapChainLength];

		for ( int i = 0; i < TextureSwapChainLength; i++ )
		{
			const GLuint colorTexture = vrapi_GetTextureSwapChainHandle( ColorTextureSwapChain, i );
			const GLuint depthTexture = ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE ) ? DepthBuffers[MALI_SEPARATE_DEPTH_BUFFERS ? i : 0] : 0;

			if ( extensionsOpenGL.EXT_texture_border_clamp )
			{
				glBindTexture( GL_TEXTURE_2D_ARRAY, colorTexture );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
				glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
				GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				glTexParameterfv( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor );
				glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
			}

			glGenFramebuffers( 1, &RenderFrameBuffers[i] );
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, RenderFrameBuffers[i] );

			if ( multisampleMode == MSAA_RENDER_TO_TEXTURE )
			{
				glFramebufferTextureMultisampleMultiviewOVR_( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture,
															 0, multisamples, 0 /* baseViewIndex */, 2 /* numViews */ );
				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					glFramebufferTextureMultisampleMultiviewOVR_( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture,
															0, multisamples, 0 /* baseViewIndex */, 2 /* numViews */ );
				}
				GLenum renderStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
				if ( renderStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffers[i], renderStatus );	// TODO: fall back to something else
				}

				GLint actualMultisamples = 0;
				glGetIntegerv( GL_SAMPLES, &actualMultisamples );
				LOG( "multisamples: requested = %d actual = %d", multisamples, actualMultisamples );
				glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

				GL_CheckErrors( "MSAA_RENDER_TO_TEXTURE");
			}
			else
			{
				// No MSAA, use ES 2 render targets
				glGenFramebuffers( 1, &RenderFrameBuffers[i] );
				glBindFramebuffer( GL_DRAW_FRAMEBUFFER, RenderFrameBuffers[i] );
				glFramebufferTextureMultiviewOVR_( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0,
												  0 /* baseViewIndex */, 2 /* numViews */ );

				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					glFramebufferTextureMultiviewOVR_( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, DepthBuffers[MALI_SEPARATE_DEPTH_BUFFERS ? i : 0], 0,
													  0 /* baseViewIndex */, 2 /* numViews */ );
				}

				GLenum renderStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER );
				if ( renderStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffers[i], renderStatus );	// TODO: fall back to something else
				}
				glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

				GL_CheckErrors( "MSAA_OFF");
			}

			// Explicitly clear the color buffer to a color we would notice.
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, RenderFrameBuffers[i] );
			glScissor( 0, 0, width, height );
			glViewport( 0, 0, width, height );
			glClearColor( 0, 1, 0, 1 );
			glClear( GL_COLOR_BUFFER_BIT );
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
		}
	}
	else
	{
		if ( multisamples > 1 )
		{
			if ( glFramebufferTexture2DMultisampleEXT_ != NULL && resolveDepth == false )
			{
				multisampleMode = MSAA_RENDER_TO_TEXTURE;
				LOG( "multisample mode = MSAA_RENDER_TO_TEXTURE" );
			}
			else
			{
				multisampleMode = MSAA_BLIT;
				LOG( "multisample mode = MSAA_BLIT" );
			}
		}
		else
		{
			multisampleMode = MSAA_OFF;
			LOG( "multisample mode = MSAA_OFF" );
		}

		LOG( "resolveDepth = %s", resolveDepth ? "true" : "false" );

		// Create the color texture set and associated color buffer.
		{
			ColorTextureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, true );
			OVR_ASSERT( ColorTextureSwapChain != NULL );
			TextureSwapChainLength = vrapi_GetTextureSwapChainLength( ColorTextureSwapChain );
			OVR_ASSERT( TextureSwapChainLength );
			TextureSwapChainIndex = 0;

			if ( multisampleMode == MSAA_BLIT )
			{
				GLenum glInternalFormat;
				switch ( colorFormat )
				{
					case VRAPI_TEXTURE_FORMAT_565:			glInternalFormat = GL_RGB565; break;
					case VRAPI_TEXTURE_FORMAT_5551:			glInternalFormat = GL_RGB5_A1; break;
					case VRAPI_TEXTURE_FORMAT_4444:			glInternalFormat = GL_RGBA4; break;
					case VRAPI_TEXTURE_FORMAT_8888:			glInternalFormat = GL_RGBA8; break;
					case VRAPI_TEXTURE_FORMAT_8888_sRGB:	glInternalFormat = GL_SRGB8_ALPHA8; break;
					case VRAPI_TEXTURE_FORMAT_RGBA16F:		glInternalFormat = GL_RGBA16F; break;
					default: FAIL( "Unknown colorFormat %i", colorFormat );
				}

				// Create a multi-sampled render buffer.
				glGenRenderbuffers( 1, &ColorBuffer );
				glBindRenderbuffer( GL_RENDERBUFFER, ColorBuffer );
				glRenderbufferStorageMultisample( GL_RENDERBUFFER, multisamples, glInternalFormat, width, height );
				GLint actualMultisamples = 0;
				glGetIntegerv( GL_SAMPLES, &actualMultisamples );
				LOG( "multisamples: requested = %d actual = %d", multisamples, actualMultisamples );
				glBindRenderbuffer( GL_RENDERBUFFER, 0 );
			}
		}

		// Create the depth texture set and associated depth buffer.
		if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
		{
			if ( resolveDepth )
			{
				DepthTextureSwapChain = vrapi_CreateTextureSwapChain( VRAPI_TEXTURE_TYPE_2D, depthFormat, width, height, 1, true );
				OVR_ASSERT( DepthTextureSwapChain != NULL );
				const int depthChainLength = vrapi_GetTextureSwapChainLength( DepthTextureSwapChain );
				OVR_ASSERT( depthChainLength == TextureSwapChainLength );
				OVR_UNUSED( depthChainLength );
			}

			if ( !resolveDepth || multisampleMode == MSAA_BLIT )
			{
				// GL_DEPTH_COMPONENT16 is the only strictly legal thing in unextended GL ES 2.0
				// The GL_OES_depth24 extension allows GL_DEPTH_COMPONENT24_OES.
				// The GL_OES_packed_depth_stencil extension allows GL_DEPTH24_STENCIL8_OES.
				GLenum glInternalFormat;
				switch ( depthFormat )
				{
					case VRAPI_TEXTURE_FORMAT_DEPTH_16:				glInternalFormat = GL_DEPTH_COMPONENT16; break;
					case VRAPI_TEXTURE_FORMAT_DEPTH_24:				glInternalFormat = GL_DEPTH_COMPONENT24; break;
					case VRAPI_TEXTURE_FORMAT_DEPTH_24_STENCIL_8:	glInternalFormat = GL_DEPTH24_STENCIL8; break;
					default: FAIL( "Unknown depthFormat %i", depthFormat );
				}

				// FIXME: we should only need one depth buffer but the Mali driver
				// does not like sharing the depth buffer between multiple frame buffers
				DepthBuffers = new GLuint[TextureSwapChainLength];
				for ( int i = 0; i < ( MALI_SEPARATE_DEPTH_BUFFERS ? TextureSwapChainLength : 1 ); i++ )
				{
					glGenRenderbuffers( 1, &DepthBuffers[i] );
					glBindRenderbuffer( GL_RENDERBUFFER, DepthBuffers[i] );
					if ( multisampleMode == MSAA_RENDER_TO_TEXTURE )
					{
						glRenderbufferStorageMultisampleEXT_( GL_RENDERBUFFER, multisamples, glInternalFormat, width, height );
					}
					else if ( multisampleMode == MSAA_BLIT )
					{
						glRenderbufferStorageMultisample( GL_RENDERBUFFER, multisamples, glInternalFormat, width, height );
					}
					else
					{
						glRenderbufferStorage( GL_RENDERBUFFER, glInternalFormat, width, height );
					}
					glBindRenderbuffer( GL_RENDERBUFFER, 0 );
				}
			}
		}

		RenderFrameBuffers = new GLuint[TextureSwapChainLength];
		if ( multisampleMode == MSAA_BLIT )
		{
			ResolveFrameBuffers = new GLuint[TextureSwapChainLength];
		}

		for ( int i = 0; i < TextureSwapChainLength; i++ )
		{
			const GLuint colorTexture = vrapi_GetTextureSwapChainHandle( ColorTextureSwapChain, i );
			const GLuint depthTexture = ( DepthTextureSwapChain != NULL ) ? vrapi_GetTextureSwapChainHandle( DepthTextureSwapChain, i ) : 0;

			if ( extensionsOpenGL.EXT_texture_border_clamp )
			{
				glBindTexture( GL_TEXTURE_2D, colorTexture );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
				GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor );
				glBindTexture( GL_TEXTURE_2D, 0 );
			}

			if ( multisampleMode == MSAA_RENDER_TO_TEXTURE )
			{
				// Today's tiling GPUs can automatically resolve a multisample rendering on a tile-by-tile basis,
				// without needing to draw to a full size multisample buffer, then blit resolve to a normal texture.
				glGenFramebuffers( 1, &RenderFrameBuffers[i] );
				glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffers[i] );
				glFramebufferTexture2DMultisampleEXT_( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, multisamples );
				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthBuffers[MALI_SEPARATE_DEPTH_BUFFERS ? i : 0] );
				}
				GLenum renderStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
				if ( renderStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffers[i], renderStatus );	// TODO: fall back to something else
				}
				GLint actualMultisamples = 0;
				glGetIntegerv( GL_SAMPLES, &actualMultisamples );
				LOG( "multisamples: requested = %d actual = %d", multisamples, actualMultisamples );
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				GL_CheckErrors( "MSAA_RENDER_TO_TEXTURE");
			}
			else if ( multisampleMode == MSAA_BLIT )
			{
				// Allocate a new frame buffer and attach the multi-sampled color and depth buffer.
				glGenFramebuffers( 1, &RenderFrameBuffers[i] );
				glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffers[i] );
				glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ColorBuffer );
				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthBuffers[MALI_SEPARATE_DEPTH_BUFFERS ? i : 0] );
				}
				GLenum renderStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
				if ( renderStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffers[i], renderStatus );	// TODO: fall back to something else
				}
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				// Create a second FBO without multi-sampling to blit to the swapchain texture from the multi-sampled buffer.
				glGenFramebuffers( 1, &ResolveFrameBuffers[i] );
				glBindFramebuffer( GL_FRAMEBUFFER, ResolveFrameBuffers[i] );
				glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0 );
				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					if ( resolveDepth )
					{
						glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0 );
					}
					else
					{
						// No attachment.
					}
				}
				GLenum resolveStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
				if ( resolveStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "resolve FBO %i is not complete: 0x%x", ResolveFrameBuffers[i], resolveStatus );	// TODO: fall back to something else
				}
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				GL_CheckErrors( "MSAA_BLIT");
			}
			else
			{
				// No MSAA, use ES 2 render targets
				glGenFramebuffers( 1, &RenderFrameBuffers[i] );
				glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffers[i] );
				glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0 );
				if ( depthFormat != VRAPI_TEXTURE_FORMAT_NONE )
				{
					if ( resolveDepth )
					{
						glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0 );
					}
					else
					{
						glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DepthBuffers[MALI_SEPARATE_DEPTH_BUFFERS ? i : 0] );
					}
				}
				GLenum renderStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
				if ( renderStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					FAIL( "render FBO %i is not complete: 0x%x", RenderFrameBuffers[i], renderStatus );	// TODO: fall back to something else
				}
				glBindFramebuffer( GL_FRAMEBUFFER, 0 );

				GL_CheckErrors( "MSAA_OFF");
			}

			// Explicitly clear the color buffer to a color we would notice.
			glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffers[i] );
			glScissor( 0, 0, width, height );
			glViewport( 0, 0, width, height );
			glClearColor( 0, 1, 0, 1 );
			glClear( GL_COLOR_BUFFER_BIT );
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}
	}
}

ovrFramebuffer::~ovrFramebuffer()
{
	if ( RenderFrameBuffers != NULL )
	{
		glDeleteFramebuffers( TextureSwapChainLength, RenderFrameBuffers );
		delete [] RenderFrameBuffers;
		RenderFrameBuffers = NULL;
	}
	if ( ResolveFrameBuffers != NULL )
	{
		glDeleteFramebuffers( TextureSwapChainLength, ResolveFrameBuffers );
		delete [] ResolveFrameBuffers;
		ResolveFrameBuffers = NULL;
	}
	if ( ColorTextureSwapChain != NULL )
	{
		vrapi_DestroyTextureSwapChain( ColorTextureSwapChain );
		ColorTextureSwapChain = NULL;
	}
	if ( DepthTextureSwapChain != NULL )
	{
		vrapi_DestroyTextureSwapChain( DepthTextureSwapChain );
		DepthTextureSwapChain = NULL;
	}
	if ( DepthBuffers != NULL )
	{
		if ( UseMultiview )
		{
			glDeleteTextures( MALI_SEPARATE_DEPTH_BUFFERS ? TextureSwapChainLength : 1, DepthBuffers );
		}
		else
		{
			glDeleteRenderbuffers( MALI_SEPARATE_DEPTH_BUFFERS ? TextureSwapChainLength : 1, DepthBuffers );
		}
		delete [] DepthBuffers;
		DepthBuffers = NULL;
	}
	if ( ColorBuffer != 0 )
	{
		glDeleteRenderbuffers( 1, &ColorBuffer );
		ColorBuffer = 0;
	}

	Width = 0;
	Height = 0;
	TextureSwapChainLength = 0;
	TextureSwapChainIndex = 0;
}

void ovrFramebuffer::Advance()
{
	if ( TextureSwapChainLength )
	{
		TextureSwapChainIndex = ( TextureSwapChainIndex + 1 ) % TextureSwapChainLength;
	}
}

void ovrFramebuffer::Bind()
{
	if ( UseMultiview )
	{
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, RenderFrameBuffers[TextureSwapChainIndex] );
	}
	else
	{
		glBindFramebuffer( GL_FRAMEBUFFER, RenderFrameBuffers[TextureSwapChainIndex] );
	}
}

void ovrFramebuffer::Resolve()
{
	// Discard the depth buffer, so the tiler won't need to write it back out to memory
	if ( DepthTextureSwapChain == NULL )
	{
		GL_InvalidateFramebuffer( INV_FBO, false, true );
	}

	// Do a blit-MSAA-resolve if necessary.
	if ( ResolveFrameBuffers != 0 )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, RenderFrameBuffers[TextureSwapChainIndex] );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, ResolveFrameBuffers[TextureSwapChainIndex] );
		glBlitFramebuffer(	0, 0, Width, Height, 0, 0, Width, Height,
							GL_COLOR_BUFFER_BIT | ( DepthTextureSwapChain != NULL ? GL_DEPTH_BUFFER_BIT : 0 ),
							GL_NEAREST );

		// Discard the multisample buffers after we have resolved it,
		// so the tiler won't need to write it back out to memory
		GL_InvalidateFramebuffer( INV_FBO, true, ( DepthTextureSwapChain != NULL ) );
	}

	if ( UseMultiview )
	{
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	}
	else
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
}

} // namespace OVR
