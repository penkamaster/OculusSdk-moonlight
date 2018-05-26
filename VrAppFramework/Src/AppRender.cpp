/************************************************************************************

Filename    :   AppRender.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <math.h>

#include "OVR_GlUtils.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"

#include "App.h"
#include "AppLocal.h"
#include "VrCommon.h"
#include "DebugLines.h"

//#define OVR_USE_PERF_TIMER
#include "OVR_PerfTimer.h"

#include <algorithm>				// for min, max

namespace OVR
{
static int ClipPolygonToPlane( Vector4f * dstPoints, const Vector4f * srcPoints, int srcPointCount, int planeAxis, float planeDist, float planeDir )
{
	int sides[16] = { 0 };

	for ( int p0 = 0; p0 < srcPointCount; p0++ )
	{
		const int p1 = ( p0 + 1 ) % srcPointCount;
		sides[p0] = planeDir * srcPoints[p0][planeAxis] < planeDist * srcPoints[p0].w;

		const float d0 = srcPoints[p0].w * planeDir * planeDist - srcPoints[p0][planeAxis];
		const float d1 = srcPoints[p1].w * planeDir * planeDist - srcPoints[p1][planeAxis];
		const float delta = d0 - d1;
		const float fraction = fabsf( delta ) > MATH_FLOAT_SMALLEST_NON_DENORMAL ? ( d0 / delta ) : 1.0f;
		const float clamped = Alg::Clamp( fraction, 0.0f, 1.0f );

		dstPoints[p0 * 2 + 0] = srcPoints[p0];
		dstPoints[p0 * 2 + 1] = srcPoints[p0] + ( srcPoints[p1] - srcPoints[p0] ) * clamped;
	}

	sides[srcPointCount] = sides[0];

	int dstPointCount = 0;
	for ( int p = 0; p < srcPointCount; p++ )
	{
		if ( sides[p + 0] != 0 )
		{
			dstPoints[dstPointCount++] = dstPoints[p * 2 + 0];
		}
		if ( sides[p + 0] != sides[p + 1] )
		{
			dstPoints[dstPointCount++] = dstPoints[p * 2 + 1];
		}
	}

	OVR_ASSERT( dstPointCount <= 16 );
	return dstPointCount;
}

static ovrRectf TextureRectForBounds( const Bounds3f & bounds, const Matrix4f & modelViewProjectionMatrix )
{
	Vector4f projectedCorners[8];
	for ( int i = 0; i < 8; i++ )
	{
		const Vector4f corner(
			bounds.b[( i ^ ( i >> 1 ) ) & 1][0],
			bounds.b[( i >> 1 ) & 1][1],
			bounds.b[( i >> 2 ) & 1][2],
			1.0f
		);
		projectedCorners[i] = modelViewProjectionMatrix.Transform( corner );
	}

	const int sideIndices[6][4] =
	{
		{ 0, 3, 7, 4 },
		{ 1, 2, 6, 5 },
		{ 0, 1, 5, 4 },
		{ 2, 3, 7, 6 },
		{ 0, 1, 2, 3 },
		{ 4, 5, 6, 7 }
	};

	Vector4f clippedCorners[6 * 16];
	int clippedCornerCount = 0;
	for ( int i = 0; i < 6; i++ )
	{
		clippedCorners[clippedCornerCount + 0] = projectedCorners[sideIndices[i][0]];
		clippedCorners[clippedCornerCount + 1] = projectedCorners[sideIndices[i][1]];
		clippedCorners[clippedCornerCount + 2] = projectedCorners[sideIndices[i][2]];
		clippedCorners[clippedCornerCount + 3] = projectedCorners[sideIndices[i][3]];
		Vector4f * corners0 = &clippedCorners[clippedCornerCount];
		Vector4f corners1[2 * 16];
		int cornerCount = 4;
		cornerCount = ClipPolygonToPlane( corners1, corners0, cornerCount, 0, 1.0f, -1.0f );
		cornerCount = ClipPolygonToPlane( corners0, corners1, cornerCount, 0, 1.0f, +1.0f );
		cornerCount = ClipPolygonToPlane( corners1, corners0, cornerCount, 1, 1.0f, -1.0f );
		cornerCount = ClipPolygonToPlane( corners0, corners1, cornerCount, 1, 1.0f, +1.0f );
		cornerCount = ClipPolygonToPlane( corners1, corners0, cornerCount, 2, 1.0f, -1.0f );
		cornerCount = ClipPolygonToPlane( corners0, corners1, cornerCount, 2, 1.0f, +1.0f );
		clippedCornerCount += cornerCount;
	}

	if ( clippedCornerCount == 0 )
	{
		const ovrRectf rect = { 0.0f, 0.0f, 0.0f, 0.0f };
		return rect;
	}

	Bounds3f clippedBounds( Bounds3f::Init );
	for ( int i = 0; i < clippedCornerCount; i++ )
	{
		OVR_ASSERT( clippedCorners[i].w > MATH_FLOAT_SMALLEST_NON_DENORMAL );
		const float recip = 0.5f / clippedCorners[i].w;
		const Vector3f point(
			clippedCorners[i].x * recip + 0.5f,
			clippedCorners[i].y * recip + 0.5f,
			clippedCorners[i].z * recip + 0.5f );
		clippedBounds.AddPoint( point );
	}

	for ( int i = 0; i < 3; i++ )
	{
		clippedBounds.GetMins()[i] = Alg::Clamp( clippedBounds.GetMins()[i], 0.0f, 1.0f );
		clippedBounds.GetMaxs()[i] = Alg::Clamp( clippedBounds.GetMaxs()[i], 0.0f, 1.0f );
	}

	ovrRectf rect;
	rect.x = clippedBounds.GetMins().x;
	rect.y = clippedBounds.GetMins().y;
	rect.width = clippedBounds.GetMaxs().x - clippedBounds.GetMins().x;
	rect.height = clippedBounds.GetMaxs().y - clippedBounds.GetMins().y;

	return rect;
}

static Bounds3f BoundsForSurfaceList( const OVR::Array< ovrDrawSurface > & surfaceList )
{
	Bounds3f surfaceBounds( Bounds3f::Init );
	for ( int i = 0; i < surfaceList.GetSizeI(); i++ )
	{
		const ovrDrawSurface * surf = &surfaceList[i];
		if ( surf == NULL || surf->surface == NULL )
		{
			continue;
		}

		const Vector3f size = surf->surface->geo.localBounds.GetSize();
		if ( size.x == 0.0f && size.y == 0.0f && size.z == 0.0f )
		{
			WARN( "surface[%s]->cullingBounds = (%f %f %f)-(%f %f %f)", surf->surface->surfaceName.ToCStr(),
				surf->surface->geo.localBounds.GetMins().x, surf->surface->geo.localBounds.GetMins().y, surf->surface->geo.localBounds.GetMins().z,
				surf->surface->geo.localBounds.GetMaxs().x, surf->surface->geo.localBounds.GetMaxs().y, surf->surface->geo.localBounds.GetMaxs().z );
			OVR_ASSERT( false );
			continue;
		}
		const Bounds3f worldBounds = Bounds3f::Transform( surf->modelMatrix, surf->surface->geo.localBounds );
		surfaceBounds = Bounds3f::Union( surfaceBounds, worldBounds );
	}

	return surfaceBounds;
}

void AppLocal::CreateFence( ovrFence * fence )
{
	fence->Display = 0;
	fence->Sync = EGL_NO_SYNC_KHR;
}

void AppLocal::DestroyFence( ovrFence * fence )
{
#if defined( OVR_OS_ANDROID )
	if ( fence->Sync != EGL_NO_SYNC_KHR )
	{
		if ( eglDestroySyncKHR_( fence->Display, fence->Sync ) ==  EGL_FALSE )
		{
			WARN( "eglDestroySyncKHR() : EGL_FALSE" );
			return;
		}
		fence->Display = 0;
		fence->Sync = EGL_NO_SYNC_KHR;
	}
#endif
}

void AppLocal::InsertFence( ovrFence * fence )
{
#if defined( OVR_OS_ANDROID )
	DestroyFence( fence );

	fence->Display = eglGetCurrentDisplay();
	fence->Sync = eglCreateSyncKHR_( fence->Display, EGL_SYNC_FENCE_KHR, NULL );
	if ( fence->Sync == EGL_NO_SYNC_KHR )
	{
		WARN( "eglCreateSyncKHR() : EGL_NO_SYNC_KHR" );
		return;
	}
	// Force flushing the commands.
	// Note that some drivers will already flush when calling eglCreateSyncKHR.
	if ( eglClientWaitSyncKHR_( fence->Display, fence->Sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0 ) == EGL_FALSE )
	{
		WARN( "eglClientWaitSyncKHR() : EGL_FALSE" );
		return;
	}
#endif
}

void AppLocal::DrawEyeViews( ovrFrameResult & res )
{
	OVR_PERF_TIMER( AppLocal_DrawEyeViews );

	// Flush out and report any errors
	GL_CheckErrors( "FrameStart" );

	// If TexRectLayer is specified, compute a texRect which covers the render surface list for the layer.
	if ( res.TexRectLayer >= 0 && res.TexRectLayer < ovrMaxLayerCount )
	{
		const Bounds3f surfaceBounds = BoundsForSurfaceList( res.Surfaces );

		if ( res.Layers[res.TexRectLayer].Header.Type == VRAPI_LAYER_TYPE_PROJECTION2 )
		{
			ovrLayerProjection2 & layer = res.Layers[res.TexRectLayer].Projection;
			for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
			{
				layer.Textures[eye].TextureRect = TextureRectForBounds( surfaceBounds, res.FrameMatrices.EyeProjection[eye] * res.FrameMatrices.EyeView[eye] );
			}
		}
	}

	// DisplayMonoMode uses a single eye rendering for speed improvement
	// and / or high refresh rate double-scan hardware modes.
	const bool renderMonoMode = ( VrSettings.RenderMode & RENDERMODE_TYPE_MONO_MASK ) != 0;
	const int numPasses = ( renderMonoMode || UseMultiview ) ? 1 : 2;

	// Render each eye.
	OVR_PERF_TIMER( AppLocal_DrawEyeViews_RenderEyes );

	EyeBuffers->BeginFrame();

	for ( int eye = 0; eye < numPasses; eye++ )
	{
		EyeBuffers->BeginRenderingEye( eye );

		// Call back to the app for drawing.
		if ( res.ClearDepthBuffer && res.ClearColorBuffer )
		{
			glClearColor( res.ClearColor.x, res.ClearColor.y, res.ClearColor.z, res.ClearColor.w );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		}
		else if ( res.ClearColorBuffer )
		{
			glClearColor( res.ClearColor.x, res.ClearColor.y, res.ClearColor.z, res.ClearColor.w );
			glClear( GL_COLOR_BUFFER_BIT );
		}
		else if ( res.ClearDepthBuffer )
		{
			glClear( GL_DEPTH_BUFFER_BIT );
		}

		// Render the surfaces returned by Frame.
		SurfaceRender.RenderSurfaceList( res.Surfaces, res.FrameMatrices.EyeView[ 0 ], res.FrameMatrices.EyeProjection[ 0 ], eye );

		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );

		// Draw a thin vignette at the edges of the view so clamping will give black
		// This will not be reflected correctly in overlay planes.
		if ( extensionsOpenGL.EXT_texture_border_clamp == false )
		{
			OVR_PERF_TIMER( AppLocal_DrawEyeViews_FillEdge );
			{
				// We need destination alpha to be solid 1 at the edges to prevent overlay
				// plane rendering from bleeding past the rendered view border, but we do
				// not want to fade to that, which would cause overlays to fade out differently
				// than scene geometry.

				// We could skip this for the cube map overlays when used for panoramic photo viewing
				// with no foreground geometry to get a little more fov effect, but if there
				// is a swipe view or anything else being rendered, the border needs to
				// be respected.

				// Note that this single pixel border won't be sufficient if mipmaps are made for
				// the eye buffers.

				// Probably should do this with GL_LINES instead of scissor changing.
				const int fbWidth = VrSettings.EyeBufferParms.resolutionWidth;
				const int fbHeight = VrSettings.EyeBufferParms.resolutionHeight;
				glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
				glEnable( GL_SCISSOR_TEST );

				glScissor( 0, 0, fbWidth, 1 );
				glClear( GL_COLOR_BUFFER_BIT );

				glScissor( 0, fbHeight-1, fbWidth, 1 );
				glClear( GL_COLOR_BUFFER_BIT );

				glScissor( 0, 0, 1, fbHeight );
				glClear( GL_COLOR_BUFFER_BIT );

				glScissor( fbWidth-1, 0, 1, fbHeight );
				glClear( GL_COLOR_BUFFER_BIT );

				glScissor( 0, 0, fbWidth, fbHeight );
				glDisable( GL_SCISSOR_TEST );
			}
		}

		{
			OVR_PERF_TIMER( AppLocal_DrawEyeViews_EndRenderingEye );
			EyeBuffers->EndRenderingEye( eye );
		}
	}

	EyeBuffers->EndFrame();

	// Insert a single fence to indicate the frame is ready to be displayed.
	ovrFence * fence = &CompletionFences[CompletionFenceIndex];
	InsertFence( fence );
	CompletionFenceIndex = ( CompletionFenceIndex + 1 ) % MAX_FENCES;

	OVR_PERF_TIMER_STOP( AppLocal_DrawEyeViews_RenderEyes );

	OVR_PERF_TIMER_STOP( AppLocal_DrawEyeViews );

	{
		OVR_PERF_TIMER( AppLocal_DrawEyeViews_SubmitFrame );

		ovrLayerHeader2 * layers[ovrMaxLayerCount] = {};
		const int layerCount = std::min( res.LayerCount, static_cast<int>( ovrMaxLayerCount ) );
		for ( int i = 0; i < layerCount; i++ )
		{
			layers[i] = &res.Layers[i].Header;
		}

		ovrSubmitFrameDescription2 frameDesc = {};
		frameDesc.Flags = res.FrameFlags;
		frameDesc.SwapInterval = res.SwapInterval;
		frameDesc.FrameIndex = res.FrameIndex;
		frameDesc.CompletionFence = (size_t)fence->Sync;
		frameDesc.DisplayTime = res.DisplayTime;
		frameDesc.LayerCount = layerCount;
		frameDesc.Layers = layers;

		vrapi_SubmitFrame2( OvrMobile, &frameDesc );
	}
}

void AppLocal::DrawBlackFrame( const int frameFlags_ )
{
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
	frameFlags |= frameFlags_;

	// Push black images to the screen to eliminate any frames of lost head tracking.
	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2 * layers[] =
	{
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = {};
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = GetSwapInterval();
	frameDesc.FrameIndex = TheVrFrame.Get().FrameNumber;
	frameDesc.DisplayTime = TheVrFrame.Get().PredictedDisplayTimeInSeconds;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2( OvrMobile, &frameDesc );
}

void AppLocal::DrawLoadingIcon( ovrTextureSwapChain * swapChain, const float spinSpeed, const float spinScale )
{
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

	ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
	blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
	iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
	iconLayer.SpinSpeed = spinSpeed;
	iconLayer.SpinScale = spinScale;
	iconLayer.ColorSwapChain = swapChain;

	const ovrLayerHeader2 * layers[] =
	{
		&blackLayer.Header,
		&iconLayer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = {};
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = GetSwapInterval();
	frameDesc.FrameIndex = TheVrFrame.Get().FrameNumber;
	frameDesc.DisplayTime = TheVrFrame.Get().PredictedDisplayTimeInSeconds;
	frameDesc.LayerCount = 2;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2( OvrMobile, &frameDesc );
}

}	// namespace OVR
