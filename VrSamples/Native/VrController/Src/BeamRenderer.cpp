/************************************************************************************

Filename    :   BeamRenderer.cpp
Content     :   Class that manages and renders view-oriented beams.
Created     :   October 23; 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR; LLC. All Rights reserved.

TODO: this is completely untested and will likely need some more work before use.

*************************************************************************************/

#include "BeamRenderer.h"

#include <stdlib.h>

#include "Kernel/OVR_LogUtils.h"
#include "OVR_GlUtils.h"
#include "TextureAtlas.h"
#include "VrCommon.h"

namespace OVR {

static const char* DebugLineVertexSrc =
	"attribute vec4 Position;\n"
	"attribute vec4 VertexColor;\n"
	"attribute vec2 TexCoord;\n"
	"varying lowp vec4 outColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"	oTexCoord = TexCoord;\n"
	"   outColor = VertexColor;\n"
	"}\n";

static const char* DebugLineFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"varying lowp vec4 outColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = outColor * texture2D( Texture0, oTexCoord );\n"
	"}\n";

float ovrBeamRenderer::LIFETIME_INFINITE = FLT_MAX;

//==============================
// ovrBeamRenderer::ovrBeamRenderer
ovrBeamRenderer::ovrBeamRenderer()
	: MaxBeams( 0 )
{
}

//==============================
// ovrBeamRenderer::ovrBeamRenderer
ovrBeamRenderer::~ovrBeamRenderer()
{
	Shutdown();
}

//==============================
// ovrBeamRenderer::Init
void ovrBeamRenderer::Init( const int maxBeams, const bool depthTest )
{
	Shutdown();

	MaxBeams = maxBeams;

	if ( LineProgram.VertexShader == 0 || LineProgram.FragmentShader == 0 )
	{
		LineProgram = BuildProgram( DebugLineVertexSrc, DebugLineFragmentSrc );
	}

	const int numVerts = maxBeams * 4;

	VertexAttribs attr;
	attr.position.Resize( numVerts );
	attr.uv0.Resize( numVerts );
	attr.color.Resize( numVerts );

	// the indices will never change once we've set them up; we just won't necessarily
	// use all of the index buffer to render.
	Array< TriangleIndex > indices;
	indices.Resize( MaxBeams * 6 );

	for ( int i = 0; i < MaxBeams; i ++ )
	{
		indices[i * 6 + 0] = static_cast< TriangleIndex >( i * 4 + 0 );
		indices[i * 6 + 1] = static_cast< TriangleIndex >( i * 4 + 1 );
		indices[i * 6 + 2] = static_cast< TriangleIndex >( i * 4 + 3 );
		indices[i * 6 + 3] = static_cast< TriangleIndex >( i * 4 + 0 );
		indices[i * 6 + 4] = static_cast< TriangleIndex >( i * 4 + 3 );
		indices[i * 6 + 5] = static_cast< TriangleIndex >( i * 4 + 2 );
	}

	Surf.surfaceName = "beams";
	Surf.geo.Create( attr, indices );
	Surf.geo.primitiveType = GL_TRIANGLES;
	Surf.geo.indexCount = 0;

	ovrGraphicsCommand & gc = Surf.graphicsCommand;

	gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = depthTest;
	gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
	gc.GpuState.blendSrc = GL_SRC_ALPHA;
	gc.GpuState.blendDst = GL_ONE;
	gc.Program = LineProgram;

#if defined( OVR_OS_ANDROID )
	gc.GpuState.lineWidth = 2.0f;
#else
	gc.GpuState.lineWidth = 1.0f;	// deprecated in OpenGL 4.2
#endif
}

//==============================
// ovrBeamRenderer::Shutdown
void ovrBeamRenderer::Shutdown()
{
	Surf.geo.Free();
	DeleteProgram( LineProgram );

	MaxBeams = 0;
	FreeBeams.Resize( 0 );
	ActiveBeams.Resize( 0 );
	BeamInfos.Resize( 0 );
}

//==============================
// ovrBeamRenderer::AddBeam
ovrBeamRenderer::handle_t ovrBeamRenderer::AddBeam( const ovrFrameInput & frame,
		const ovrTextureAtlas & atlas, const int atlasIndex,
		const float width,
		const Vector3f & startPos, const Vector3f & endPos,
		const Vector4f & initialColor,
		const float lifeTime )
{
	handle_t handle;

	//LOG( "ovrBeamRenderer::AddDebugLine" );
	if ( FreeBeams.GetSizeI() > 0 )
	{
		handle = FreeBeams[FreeBeams.GetSizeI() - 1];
		FreeBeams.PopBack();
	}
	else
	{
		handle = handle_t( static_cast< uint16_t >( BeamInfos.GetSizeI() ) );
		if ( handle.Get() >= MaxBeams || handle.Get() >= MAX_BEAMS )
		{
			return handle_t();
		}
		BeamInfos.PushBack( ovrBeamInfo() );
	}

	OVR_ASSERT( handle.IsValid() );
	OVR_ASSERT( handle.Get() < BeamInfos.GetSizeI() );
	OVR_ASSERT( handle.Get() < MAX_BEAMS );

	ActiveBeams.PushBack( handle );

	UpdateBeamInternal( frame, handle, atlas, atlasIndex, width, startPos, endPos, initialColor, lifeTime );

	return ( lifeTime == LIFETIME_INFINITE ) ? handle : handle_t();
}

//==============================
// ovrBeamRenderer::UpdateBeam
void ovrBeamRenderer::UpdateBeam( const ovrFrameInput & frame, const handle_t handle, 
		const ovrTextureAtlas & atlas, const int atlasIndex, const float width, 
		const Vector3f & startPos, const Vector3f & endPos, const Vector4f & initialColor )
{
	OVR_ASSERT( BeamInfos[handle.Get()].Handle.IsValid() );
	UpdateBeamInternal( frame, handle, atlas, atlasIndex, width, startPos, endPos, initialColor, LIFETIME_INFINITE );
}

void ovrBeamRenderer::RemoveBeam( const handle_t handle )
{
	if ( !handle.IsValid() || handle.Get() >= BeamInfos.GetSize() )
	{
		return;
	}
	BeamInfos[handle.Get()].StartTime = -1.0;
	BeamInfos[handle.Get()].LifeTime = -1.0f;
}

//==============================
// ovrBeamRenderer::UpdateBeamInternal
void ovrBeamRenderer::UpdateBeamInternal( const ovrFrameInput & frame, const handle_t handle, 
		const ovrTextureAtlas & atlas, const int atlasIndex, const float width, 
		const Vector3f & startPos, const Vector3f & endPos, const Vector4f & initialColor,
		float const lifeTime )
{
	if ( !handle.IsValid() )
	{
		OVR_ASSERT( handle.IsValid() );
		return;
	}

	ovrBeamInfo & beam = BeamInfos[handle.Get()];

	beam.Handle = handle;
	beam.StartTime = frame.PredictedDisplayTimeInSeconds;
	beam.LifeTime = lifeTime;
	beam.Width = width;
	beam.AtlasIndex = static_cast< uint16_t >( atlasIndex );
	beam.StartPos = startPos;
	beam.EndPos = endPos;
	beam.InitialColor = initialColor;
	const ovrTextureAtlas::ovrSpriteDef & sd = atlas.GetSpriteDef( atlasIndex );
	beam.TexCoords[0] = sd.uvMins;		// min tex coords
	beam.TexCoords[1] = sd.uvMaxs;		// max tex coords
//	beam.TexCoordVel = initialVel;// vel of tex coord 0
//	beam.TexCoordAccel = accel;		// accel of tex coord 0
}

//==============================
// ovrBeamRenderer::Frame
void ovrBeamRenderer::Frame( const ovrFrameInput & frame, const Matrix4f & centerViewMatrix,
		const ovrTextureAtlas & atlas )
{
	Surf.geo.indexCount = 0;

	Surf.graphicsCommand.numUniformTextures = 1;
	Surf.graphicsCommand.uniformTextures[0] = atlas.GetTexture();

	VertexAttribs attr;
	attr.position.Resize( ActiveBeams.GetSize() * 4 );
	attr.color.Resize( ActiveBeams.GetSizeI() * 4 );
	attr.uv0.Resize( ActiveBeams.GetSizeI() * 4 );

	const Vector3f viewPos = GetViewMatrixPosition( centerViewMatrix );

	int quadIndex = 0;
	for ( int i = 0; i < ActiveBeams.GetSizeI(); ++i )
	{
		const handle_t beamHandle = ActiveBeams[i];
		if ( !beamHandle.IsValid() )
		{
			continue;
		}

		const ovrBeamInfo & cur = BeamInfos[beamHandle.Get()];
		double const timeAlive = frame.PredictedDisplayTimeInSeconds - cur.StartTime;
		if ( timeAlive > cur.LifeTime )
		{
			LOG( "Free beamIndex %i", (int)beamHandle.Get() );
			BeamInfos[beamHandle.Get()].Handle = handle_t();
			FreeBeams.PushBack( beamHandle );
			ActiveBeams.RemoveAtUnordered( i );
			i--;
			continue;
		}

		const Vector3f beamCenter = ( cur.EndPos - cur.StartPos ) * 0.5f;
		const Vector3f beamDir = beamCenter.Normalized();
		const Vector3f viewToCenter = ( beamCenter - viewPos ).Normalized();
		const Vector3f cross = beamDir.Cross( viewToCenter ).Normalized() * cur.Width * 0.5f;

		const float t = static_cast< float >( frame.PredictedDisplayTimeInSeconds - cur.StartTime );

		const Vector4f color = EaseFunctions[cur.EaseFunc]( cur.InitialColor, t / cur.LifeTime );
//		const Vector2f uvOfs = TexCoordVel[0] * t + TexCoordAccel[0] * 0.5f * t * t;
		const Vector2f uvOfs( 0.0f );

		attr.position[quadIndex * 4 + 0] = cur.StartPos + cross;
		attr.position[quadIndex * 4 + 1] = cur.StartPos - cross;
		attr.position[quadIndex * 4 + 2] = cur.EndPos + cross;
		attr.position[quadIndex * 4 + 3] = cur.EndPos - cross;
		attr.color[quadIndex * 4 + 0] = color;
		attr.color[quadIndex * 4 + 1] = color;
		attr.color[quadIndex * 4 + 2] = color;
		attr.color[quadIndex * 4 + 3] = color;
		attr.uv0[quadIndex * 4 + 0] = Vector2f( cur.TexCoords[0].x, cur.TexCoords[0].y ) + uvOfs;
		attr.uv0[quadIndex * 4 + 1] = Vector2f( cur.TexCoords[1].x, cur.TexCoords[0].y ) + uvOfs;
		attr.uv0[quadIndex * 4 + 2] = Vector2f( cur.TexCoords[0].x, cur.TexCoords[1].y ) + uvOfs;
		attr.uv0[quadIndex * 4 + 3] = Vector2f( cur.TexCoords[1].x, cur.TexCoords[1].y ) + uvOfs;

		quadIndex++;
	}

	//Surf.graphicsCommand.GpuState.polygonMode = GL_LINE;
	Surf.graphicsCommand.GpuState.cullEnable = false;
	Surf.geo.indexCount = quadIndex * 6;
	Surf.geo.Update( attr );
}

//==============================
// ovrBeamRenderer::RenderEyeView
void ovrBeamRenderer::RenderEyeView( const Matrix4f & /*viewMatrix*/, const Matrix4f & /*projMatrix*/,
		Array< ovrDrawSurface > & surfaceList )
{
	if ( Surf.geo.indexCount > 0 )
	{
		surfaceList.PushBack( ovrDrawSurface( ModelMatrix, &Surf ) );
	}
}

//==============================
// ovrBeamRenderer::SetPose
void ovrBeamRenderer::SetPose( const Posef & pose )
{
	ModelMatrix = Matrix4f( pose );
}

} // namespace OVR
