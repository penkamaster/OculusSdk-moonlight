/************************************************************************************

Filename    :   EyeBuffers.h
Content     :   Handling of different eye buffer formats
Created     :   March 7, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_EyeBuffers_h
#define OVR_EyeBuffers_h

#include "OVR_GlUtils.h"		// GLuint
#include "OVR_LogTimer.h"		// LogGpuTime
#include "VrApi_Types.h"

namespace OVR
{

class ovrFramebuffer;

// TODO: consider using GL internal formats directly
enum colorFormat_t
{
	COLOR_565,
	COLOR_5551,		// single bit alpha useful for overlay planes
	COLOR_4444,
	COLOR_8888,
	COLOR_8888_sRGB,
	COLOR_RGBA16F
};

enum depthFormat_t
{
	DEPTH_0,		// useful for overlay planes
	DEPTH_16,
	DEPTH_24,
	DEPTH_24_STENCIL_8
};

// Should we include an option for getting a partial mip
// chain on the frame buffer object color buffer instead of
// just a single level, allowing the application to call
// glGenerateMipmap for distortion effects or use supersampling
// for the final presentation?
//
// We may eventually want MRT formats.
struct ovrEyeBufferParms
{
		ovrEyeBufferParms() :
			resolutionWidth( 1024 ),
			resolutionHeight( 1024 ),
			multisamples( 2 ),
			resolveDepth( false ),
			colorFormat( COLOR_8888 ),
			depthFormat( DEPTH_24 )
		{
		}

	// Setting the resolution higher than necessary will cause aliasing
	// when presented to the screen, since we do not currently generate
	// mipmaps for the eye buffers, but lowering the resolution can
	// significantly improve the application frame rate.
	int					resolutionWidth;
	int					resolutionHeight;

	// Multisample anti-aliasing is almost always desirable for VR, even
	// if it requires a drop in resolution.
	int					multisamples;

	// Optionally resolve the depth buffer to a texture.
	bool				resolveDepth;

	// 16 bit color eye buffers can improve performance noticeably, but any
	// dithering effects will be distorted by the warp to screen.
	colorFormat_t		colorFormat;

	// Adreno and Tegra benefit from 16 bit depth buffers
	depthFormat_t		depthFormat;
};

struct ovrFrameTextureSwapChains
{
	// For time warp.
	ovrTextureSwapChain *	ColorTextureSwapChain[2];

	// Index to the texture from the set that should be displayed.
	int						TextureSwapChainIndex;
};

class ovrEyeBuffers
{
public:
							ovrEyeBuffers();
							~ovrEyeBuffers();

	// Changes a buffer set from one (possibly uninitialized) state to a new
	// resolution / multisample / color depth state.
	void					Initialize( const ovrEyeBufferParms & bufferParms_,
										const bool useMultiview_ );

	// Begin rendering a new frame.
	void					BeginFrame();
	// End rendering a new frame.
	void					EndFrame();

	// Handles binding the FBO or making the surface current,
	// and setting up the viewport.
	//
	// We might need a "return to eye" call if eye rendering
	// ever needs to change to a different FBO and come back,
	// but that is a bad idea on tiled GPUs anyway.
	void					BeginRenderingEye( const int eyeNum );

	// Handles resolving multisample if necessary.
	void					EndRenderingEye( const int eyeNum );

	// Returns the most recent completed
	// eye buffer set for TimeWarp to use.
	ovrFrameTextureSwapChains	GetCurrentFrameTextureSwapChains();

private:
	// GPU timer queries around eye scene rendering.
	LogGpuTime<2>			LogEyeSceneGpuTime;

	// SGX wants a clear, Adreno wants a discard, not sure what Mali wants.
	bool					DiscardInsteadOfClear;

	// Current settings
	ovrEyeBufferParms		BufferParms;

	// One render texture for each eye.
	ovrFramebuffer *		Framebuffers[2];

	// Number of buffers. For multiview, this will be 1, otherwise NUM_EYES.
	int						BufferCount;
};

} // namespace OVR

#endif // OVR_EyeBuffers_h
