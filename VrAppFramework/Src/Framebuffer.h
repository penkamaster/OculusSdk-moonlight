/************************************************************************************

Filename    :   Framebuffer.h
Content     :   Framebuffer
Created     :   July 3rd, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_Framebuffer_h
#define OVR_Framebuffer_h

#include "OVR_GlUtils.h"	// GLuint
#include "VrApi_Types.h"

namespace OVR
{

class ovrFramebuffer
{
public:
							ovrFramebuffer( const ovrTextureFormat colorFormat, const ovrTextureFormat depthFormat,
											const int width, const int height, const int multisamples,
											const bool resolveDepth, const bool useMultiview );
							~ovrFramebuffer();

	// Advance to the next frame.
	void					Advance();
	// Bind for rendering.
	void					Bind();
	// Resolve to memory once rendering has completed.
	void					Resolve();

	// Texture swap chains passed to the time warp.
	ovrTextureSwapChain *	GetColorTextureSwapChain() const { return ColorTextureSwapChain; }
	int						GetTextureSwapChainIndex() const { return TextureSwapChainIndex; }

private:
	bool					UseMultiview;

	int						Width;
	int						Height;

	int						TextureSwapChainLength;
	int						TextureSwapChainIndex;

	// Color and depth texture sets allocated from and passed to the VrApi.
	ovrTextureSwapChain *	ColorTextureSwapChain;
	ovrTextureSwapChain *	DepthTextureSwapChain;

	// This is not used for non-MSAA or glFramebufferTexture2DMultisampleEXT.
	GLuint					ColorBuffer;

	// This may be a normal or multisample buffer.
	GLuint *				DepthBuffers;

	// For non-MSAA or glFramebufferTexture2DMultisampleEXT,
	// the textures will be attached to this frame buffer.
	GLuint *				RenderFrameBuffers;

	// These framebuffers are the target of the resolve blit
	// for MSAA without glFramebufferTexture2DMultisampleEXT.
	GLuint *				ResolveFrameBuffers;
};

} // namespace OVR

#endif // OVR_Framebuffer_h
