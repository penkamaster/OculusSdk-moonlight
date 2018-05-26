/************************************************************************************

Filename    :   SurfaceRender.cpp
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "SurfaceRender.h"

#include <stdlib.h>

#include "Kernel/OVR_LogUtils.h"

#include "OVR_GlUtils.h"
#include "GlTexture.h"
#include "GlProgram.h"
#include "GlBuffer.h"
#include "VrCommon.h"

//#define OVR_USE_PERF_TIMER
#include "OVR_PerfTimer.h"

namespace OVR
{

bool LogRenderSurfaces = false;	// Do not check in set to true!

OVR_PERF_ACCUMULATOR( SurfaceRender_ChangeGpuState );

static void ChangeGpuState( const ovrGpuState oldState, const ovrGpuState newState, bool force = false )
{
	OVR_PERF_ACCUMULATE( SurfaceRender_ChangeGpuState );

	if ( force || newState.blendEnable != oldState.blendEnable )
	{
		if ( newState.blendEnable )
		{
			glEnable( GL_BLEND );
		}
		else
		{
			glDisable( GL_BLEND );
		}
	}
	if ( force || newState.blendEnable != oldState.blendEnable
			|| newState.blendSrc != oldState.blendSrc
			|| newState.blendDst != oldState.blendDst
			|| newState.blendSrcAlpha != oldState.blendSrcAlpha
			|| newState.blendDstAlpha != oldState.blendDstAlpha
			|| newState.blendMode != oldState.blendMode
			|| newState.blendModeAlpha != oldState.blendModeAlpha
			)
	{
		if ( newState.blendEnable == ovrGpuState::BLEND_ENABLE_SEPARATE )
		{
			glBlendFuncSeparate( newState.blendSrc, newState.blendDst,
					newState.blendSrcAlpha, newState.blendDstAlpha );
			glBlendEquationSeparate( newState.blendMode, newState.blendModeAlpha );
		}
		else
		{
			glBlendFunc( newState.blendSrc, newState.blendDst );
			glBlendEquation( newState.blendMode );
		}
	}

	if ( force || newState.depthFunc != oldState.depthFunc )
	{
		glDepthFunc( newState.depthFunc );
	}
	if ( force || newState.frontFace != oldState.frontFace )
	{
		glFrontFace( newState.frontFace );
	}
	if ( force || newState.depthEnable != oldState.depthEnable )
	{
		if ( newState.depthEnable )
		{
			glEnable( GL_DEPTH_TEST );
		}
		else
		{
			glDisable( GL_DEPTH_TEST );
		}
	}
	if ( force || newState.depthMaskEnable != oldState.depthMaskEnable )
	{
		if ( newState.depthMaskEnable )
		{
			glDepthMask( GL_TRUE );
		}
		else
		{
			glDepthMask( GL_FALSE );
		}
	}
	if ( force
		|| newState.colorMaskEnable[0] != oldState.colorMaskEnable[0]
		|| newState.colorMaskEnable[1] != oldState.colorMaskEnable[1]
		|| newState.colorMaskEnable[2] != oldState.colorMaskEnable[2]
		|| newState.colorMaskEnable[3] != oldState.colorMaskEnable[3]
		)
	{
		glColorMask(
			newState.colorMaskEnable[0] ? GL_TRUE : GL_FALSE,
			newState.colorMaskEnable[1] ? GL_TRUE : GL_FALSE,
			newState.colorMaskEnable[2] ? GL_TRUE : GL_FALSE,
			newState.colorMaskEnable[3] ? GL_TRUE : GL_FALSE
			);
	}
	if ( force || newState.polygonOffsetEnable != oldState.polygonOffsetEnable )
	{
		if ( newState.polygonOffsetEnable )
		{
			glEnable( GL_POLYGON_OFFSET_FILL );
			glPolygonOffset( 1.0f, 1.0f );
		}
		else
		{
			glDisable( GL_POLYGON_OFFSET_FILL );
		}
	}
	if ( force || newState.cullEnable != oldState.cullEnable )
	{
		if ( newState.cullEnable )
		{
			glEnable( GL_CULL_FACE );
		}
		else
		{
			glDisable( GL_CULL_FACE );
		}
	}
	if ( force || newState.lineWidth != oldState.lineWidth )
	{
		glLineWidth( newState.lineWidth );
	}
	if ( force ||
		( newState.depthRange[0] != oldState.depthRange[0] ) ||
		( newState.depthRange[1] != oldState.depthRange[1] ) )
	{
		glDepthRangef( newState.depthRange[0], newState.depthRange[1] );
	}
#if GL_ES_VERSION_2_0 == 0
	if ( force || newState.polygonMode != oldState.polygonMode )
	{
		glPolygonMode( GL_FRONT_AND_BACK, newState.polygonMode );
	}
#endif
	// extend as needed
}

ovrSurfaceRender::ovrSurfaceRender() :
	 CurrentSceneMatricesIdx( 0 )
{
}

ovrSurfaceRender::~ovrSurfaceRender()
{
}

void ovrSurfaceRender::Init()
{
	for ( int i = 0; i < MAX_SCENEMATRICES_UBOS; i++ )
	{
		SceneMatrices[i].Create( GLBUFFER_TYPE_UNIFORM, GlProgram::SCENE_MATRICES_UBO_SIZE, NULL );
	}

	CurrentSceneMatricesIdx = 0;
}

void ovrSurfaceRender::Shutdown()
{
	for ( int i = 0; i < MAX_SCENEMATRICES_UBOS; i++ )
	{
		SceneMatrices[i].Destroy();
	}
}

int ovrSurfaceRender::UpdateSceneMatrices( const Matrix4f * viewMatrix,
										   const Matrix4f * projectionMatrix,
										   const int numViews )
{
	OVR_ASSERT( numViews >= 0 && numViews <= GlProgram::MAX_VIEWS );

	// ----DEPRECATED_DRAWEYEVIEW
	// NOTE: Apps which still use DrawEyeView (or that are in process of moving away from it) will
	// call RenderSurfaceList multiple times per frame outside of AppRender. This can cause a
	// rendering hazard in that we may be updating the scene matrices ubo while it is still in use.
	// The typical DEV case is 2x a frame, but have seen it top 8x a frame. Since the matrices
	// in the DEV path are typically the same, test for that condition and don't update the ubo.
	// This check can be removed once the DEV path is removed.
	bool requiresUpdate = false;
#if 1
	for ( int i = 0; i < numViews; i++ )
	{
		requiresUpdate |= !( CachedViewMatrix[i] == viewMatrix[i] );
		requiresUpdate |= !( CachedProjectionMatrix[i] == projectionMatrix[i] );
	}
#else
	requiresUpdate = true;
#endif
	// ----DEPRECATED_DRAWEYEVIEW

	if ( requiresUpdate )
	{
		// Advance to the next available scene matrix ubo.
		CurrentSceneMatricesIdx = ( CurrentSceneMatricesIdx + 1 ) % MAX_SCENEMATRICES_UBOS;

		// Cache and transpose the matrices before passing to GL.
		Matrix4f viewMatrixTransposed[GlProgram::MAX_VIEWS];
		Matrix4f projectionMatrixTransposed[GlProgram::MAX_VIEWS];
		for ( int i = 0; i < numViews; i++ )
		{
			CachedViewMatrix[i] = viewMatrix[i];
			CachedProjectionMatrix[i] = projectionMatrix[i];

			viewMatrixTransposed[i] = CachedViewMatrix[i].Transposed();
			projectionMatrixTransposed[i] = CachedProjectionMatrix[i].Transposed();
		}

		void * matricesBuffer = SceneMatrices[CurrentSceneMatricesIdx].MapBuffer();
		if ( matricesBuffer != NULL )
		{
			memcpy( (char *)matricesBuffer + 0 * GlProgram::MAX_VIEWS * sizeof( Matrix4f ), viewMatrixTransposed, GlProgram::MAX_VIEWS * sizeof( Matrix4f ) );
			memcpy( (char *)matricesBuffer + 1 * GlProgram::MAX_VIEWS * sizeof( Matrix4f ), projectionMatrixTransposed, GlProgram::MAX_VIEWS * sizeof( Matrix4f ) );
			SceneMatrices[CurrentSceneMatricesIdx].UnmapBuffer();
		}
	}

	//LOG( "UpdateSceneMatrices: RequiresUpdate %d, CurrIdx %d", requiresUpdate, CurrentSceneMatricesIdx );

	return CurrentSceneMatricesIdx;
}

OVR_PERF_ACCUMULATOR( SurfaceRender_ChangeProgram );
OVR_PERF_ACCUMULATOR( SurfaceRender_UpdateUniforms );
OVR_PERF_ACCUMULATOR( SurfaceRender_geo_Draw );

// Renders a list of pointers to models in order.
ovrDrawCounters ovrSurfaceRender::RenderSurfaceList( const Array<ovrDrawSurface> & surfaceList,
													 const Matrix4f & viewMatrix,
													 const Matrix4f & projectionMatrix,
													 const int eye )
{
	OVR_PERF_TIMER( SurfaceRender_RenderSurfaceList );
	OVR_ASSERT( eye >= 0 && eye < GlProgram::MAX_VIEWS );

	// Force the GPU state to a known value, then only set on changes
	ovrGpuState			currentGpuState;
	ChangeGpuState( currentGpuState, currentGpuState, true /* force */ );

	// TODO: These should be range checked containers.
	GLuint				currentBuffers[ ovrUniform::MAX_UNIFORMS ] = {};
	GLuint				currentTextures[ ovrUniform::MAX_UNIFORMS ] = {};
	GLuint				currentProgramObject = 0;

	// ----DEPRECATED_GLPROGRAM
	const Matrix4f vpMatrix = (&projectionMatrix)[eye] * (&viewMatrix)[eye];
	// ----DEPRECATED_GLPROGRAM

	const int sceneMatricesIdx = UpdateSceneMatrices( &viewMatrix, &projectionMatrix, GlProgram::MAX_VIEWS /* num eyes */ );

	// counters
	ovrDrawCounters counters;

	// Loop through all the surfaces
	for ( int surfaceNum = 0; surfaceNum < surfaceList.GetSizeI(); surfaceNum++ )
	{
		const ovrDrawSurface & drawSurface = surfaceList[ surfaceNum ];
		const ovrSurfaceDef & surfaceDef = *drawSurface.surface;
		const ovrGraphicsCommand & cmd = surfaceDef.graphicsCommand;

		if ( cmd.Program.IsValid() && cmd.Program.UseDeprecatedInterface == false )
		{
			ChangeGpuState( currentGpuState, cmd.GpuState );
			currentGpuState = cmd.GpuState;
			//GL_CheckErrors( surfaceDef.surfaceName.ToCStr() );

			// update the program object
			if ( cmd.Program.Program != currentProgramObject )
			{
				OVR_PERF_ACCUMULATE( SurfaceRender_ChangeProgram );
				counters.numProgramBinds++;

				currentProgramObject = cmd.Program.Program;
				glUseProgram( cmd.Program.Program );
			}

			// Update globally defined system level uniforms.
			{
				if ( cmd.Program.ViewID.Location >= 0 )	// not defined when multiview enabled
				{
					glUniform1i( cmd.Program.ViewID.Location, eye );
				}
				glUniformMatrix4fv( cmd.Program.ModelMatrix.Location, 1, GL_TRUE, drawSurface.modelMatrix.M[0] );

				if ( cmd.Program.SceneMatrices.Location >= 0 )
				{
					glBindBufferBase( GL_UNIFORM_BUFFER, cmd.Program.SceneMatrices.Binding, SceneMatrices[sceneMatricesIdx].GetBuffer() );
				}

				// ----IMAGE_EXTERNAL_WORKAROUND
				if ( cmd.Program.ProjectionMatrix.Location >= 0 )
				{
					/// WORKAROUND: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices
					/// produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
					static Matrix4f projMatrixT[GlProgram::MAX_VIEWS];
					for ( int j = 0; j < GlProgram::MAX_VIEWS; j++ )
					{
						projMatrixT[j] = (&projectionMatrix)[j].Transposed();
					}
					glUniformMatrix4fv( cmd.Program.ProjectionMatrix.Location, GlProgram::MAX_VIEWS, GL_FALSE, projMatrixT[0].M[0] );
				}
				if ( cmd.Program.ViewMatrix.Location >= 0 )
				{
					/// WORKAROUND: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices
					/// produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
					static Matrix4f viewMatrixT[GlProgram::MAX_VIEWS];
					for ( int j = 0; j < GlProgram::MAX_VIEWS; j++ )
					{
						viewMatrixT[j] = (&viewMatrix)[j].Transposed();
					}
					glUniformMatrix4fv( cmd.Program.ViewMatrix.Location, GlProgram::MAX_VIEWS, GL_FALSE, viewMatrixT[0].M[0] );
				}
				// ----IMAGE_EXTERNAL_WORKAROUND
			}

			// update texture bindings and uniform values
			bool uniformsDone = false;
			{
				OVR_PERF_ACCUMULATE( SurfaceRender_UpdateUniforms );

				for ( int i = 0; i < ovrUniform::MAX_UNIFORMS && !uniformsDone; ++i )
				{
					counters.numParameterUpdates++;
					const int parmLocation = cmd.Program.Uniforms[i].Location;

					switch( cmd.Program.Uniforms[i].Type )
					{
						case ovrProgramParmType::INT:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform1iv( parmLocation, 1,
									static_cast< const int * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::INT_VECTOR2:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform2iv( parmLocation, 1,
									static_cast< const int * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::INT_VECTOR3:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform3iv( parmLocation, 1,
									static_cast< const int * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::INT_VECTOR4:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform4iv( parmLocation, 1,
									static_cast< const int * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::FLOAT:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform1f( parmLocation, 
									*static_cast< const float * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::FLOAT_VECTOR2:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform2fv( parmLocation, 1,
									static_cast< const float * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::FLOAT_VECTOR3:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform3fv( parmLocation, 1,
									static_cast< const float * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::FLOAT_VECTOR4:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								glUniform4fv( parmLocation, 1,
									static_cast< const float * >( cmd.UniformData[i].Data ) );
							}
						}
						break;
						case ovrProgramParmType::FLOAT_MATRIX4:
						{
							if ( parmLocation >= 0 && cmd.UniformData[i].Data != NULL )
							{
								if ( cmd.UniformData[i].Count > 1 )
								{
									/// FIXME: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices
									/// produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
									static Matrix4f transposedJoints[MAX_JOINTS];
									const int numJoints = Alg::Min( cmd.UniformData[i].Count, MAX_JOINTS );
									for ( int j = 0; j < numJoints; j++ )
									{
										transposedJoints[j] = static_cast< Matrix4f * >( cmd.UniformData[i].Data )[j].Transposed();
									}
									glUniformMatrix4fv( parmLocation, numJoints,
											GL_FALSE,
											static_cast< const float * >( &transposedJoints[0].M[0][0] ) );
								}
								else
								{
									glUniformMatrix4fv( parmLocation, cmd.UniformData[i].Count,
											GL_TRUE,
											static_cast< const float * >( cmd.UniformData[i].Data ) );
								}
							}
						}
						break;
						case ovrProgramParmType::TEXTURE_SAMPLED:
						{
							const int parmBinding = cmd.Program.Uniforms[i].Binding;
							if ( parmBinding >= 0 && cmd.UniformData[i].Data != NULL )
							{
								const GlTexture & texture = *static_cast< GlTexture * >( cmd.UniformData[i].Data );
								if ( currentTextures[parmBinding] != texture.texture )
								{
									counters.numTextureBinds++;
									currentTextures[parmBinding] = texture.texture;
									glActiveTexture( GL_TEXTURE0 + parmBinding );
									glBindTexture( texture.target ? texture.target : GL_TEXTURE_2D, texture.texture );
								}
							}
						}
						break;
						case ovrProgramParmType::BUFFER_UNIFORM:
						{
							const int parmBinding =  cmd.Program.Uniforms[i].Binding;
							if ( parmBinding >= 0 && cmd.UniformData[i].Data != NULL )
							{
								const GlBuffer & buffer = *static_cast< GlBuffer * >( cmd.UniformData[i].Data );
								if ( currentBuffers[parmBinding] != buffer.GetBuffer() )
								{
									counters.numBufferBinds++;
									currentBuffers[parmBinding] = buffer.GetBuffer();
									glBindBufferBase( GL_UNIFORM_BUFFER, parmBinding, buffer.GetBuffer() );
								}
							}
						}
						break;
						case ovrProgramParmType::MAX:
							uniformsDone = true;
							break;	// done
						default:
							OVR_ASSERT( false );
							uniformsDone = true;
							break;
					}
				}
			}
		}
		else // ----DEPRECATED_GLPROGRAM
		{
			Matrix4f mvp = vpMatrix * drawSurface.modelMatrix;

			// Update GPU state -- blending, etc
			ChangeGpuState( currentGpuState, cmd.GpuState );
			currentGpuState = cmd.GpuState;

			// Update texture bindings
			OVR_ASSERT( cmd.numUniformTextures <= ovrUniform::MAX_UNIFORMS );
			for ( int textureNum = 0; textureNum < cmd.numUniformTextures; textureNum++ )
			{
				const GLuint texNObj = cmd.uniformTextures[textureNum].texture;
				if ( currentTextures[textureNum] != texNObj )
				{
					counters.numTextureBinds++;
					currentTextures[textureNum] = texNObj;
					glActiveTexture( GL_TEXTURE0 + textureNum );
					// Something is leaving target set to 0; assume GL_TEXTURE_2D
					glBindTexture( cmd.uniformTextures[textureNum].target ?
							cmd.uniformTextures[textureNum].target : GL_TEXTURE_2D, texNObj );
				}
			}

			{
				OVR_PERF_ACCUMULATE( SurfaceRender_ChangeProgram );
				// Update program object
				OVR_ASSERT( cmd.Program.Program != 0 );
				if ( cmd.Program.Program != currentProgramObject )
				{
					counters.numProgramBinds++;

					currentProgramObject = cmd.Program.Program;
					glUseProgram( currentProgramObject );
				}
			}

			// Update the program parameters
			{
				OVR_PERF_ACCUMULATE( SurfaceRender_UpdateUniforms );
				counters.numParameterUpdates++;

				// Update globally defined system level uniforms.
				{
					if ( cmd.Program.ViewID.Location >= 0 ) // not defined when multiview enabled
					{
						glUniform1i( cmd.Program.ViewID.Location, eye );
					}
					glUniformMatrix4fv( cmd.Program.ModelMatrix.Location, 1, GL_TRUE, drawSurface.modelMatrix.M[0] );

					if ( cmd.Program.SceneMatrices.Location >= 0 )
					{
						glBindBufferBase( GL_UNIFORM_BUFFER, cmd.Program.SceneMatrices.Binding, SceneMatrices[sceneMatricesIdx].GetBuffer() );
					}

					// ----IMAGE_EXTERNAL_WORKAROUND
					if ( cmd.Program.ProjectionMatrix.Location >= 0 )
					{
						/// WORKAROUND: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices
						/// produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
						static Matrix4f projMatrixT[GlProgram::MAX_VIEWS];
						for ( int j = 0; j < GlProgram::MAX_VIEWS; j++ )
						{
							projMatrixT[j] = (&projectionMatrix)[j].Transposed();
						}
						glUniformMatrix4fv( cmd.Program.ProjectionMatrix.Location, GlProgram::MAX_VIEWS, GL_FALSE, projMatrixT[0].M[0] );
					}
					if ( cmd.Program.ViewMatrix.Location >= 0 )
					{
						/// WORKAROUND: setting glUniformMatrix4fv transpose to GL_TRUE for an array of matrices
						/// produces garbage using the Adreno 420 OpenGL ES 3.0 driver.
						static Matrix4f viewMatrixT[GlProgram::MAX_VIEWS];
						for ( int j = 0; j < GlProgram::MAX_VIEWS; j++ )
						{
							viewMatrixT[j] = (&viewMatrix)[j].Transposed();
						}
						glUniformMatrix4fv( cmd.Program.ViewMatrix.Location, GlProgram::MAX_VIEWS, GL_FALSE, viewMatrixT[0].M[0] );
					}
					// ----IMAGE_EXTERNAL_WORKAROUND
				}

				// FIXME: get rid of the MVP and transform vertices with the individial model/view/projection matrices for improved precision
				if ( cmd.Program.uMvp != -1 )
				{
					glUniformMatrix4fv( cmd.Program.uMvp, 1, GL_TRUE, &mvp.M[0][0] );
				}

				// set the model matrix
				if ( cmd.Program.uModel != -1 )
				{
					glUniformMatrix4fv( cmd.Program.uModel, 1, GL_TRUE, &drawSurface.modelMatrix.M[0][0] );
				}

				// set the joint matrices ubo
				if ( cmd.Program.uJoints != -1 )
				{
					OVR_ASSERT( cmd.Program.uJointsBinding != -1 );
					const GLuint bufferObj = cmd.uniformJoints.GetBuffer();
					if ( currentBuffers[cmd.Program.uJointsBinding] != bufferObj )
					{
						counters.numBufferBinds++;
						currentBuffers[cmd.Program.uJointsBinding] = bufferObj;
						glBindBufferBase( GL_UNIFORM_BUFFER, cmd.Program.uJointsBinding, bufferObj );
					}
				}
			}

			{
				OVR_PERF_ACCUMULATE( SurfaceRender_UpdateUniforms );
				for ( int unif = 0; unif < ovrUniform::MAX_UNIFORMS; unif++ )
				{
					const int slot = cmd.uniformSlots[unif];
					if ( slot == -1 )
					{
						break;
					}
					counters.numParameterUpdates++;
					glUniform4fv( slot, 1, cmd.uniformValues[unif] );
				}
			}
		}	// ----DEPRECATED_GLPROGRAM

		counters.numDrawCalls++;

		if ( LogRenderSurfaces )
		{
			LOG( "Drawing %s", surfaceDef.surfaceName.ToCStr() );
		}

		// Bind all the vertex and element arrays
		{
			OVR_PERF_ACCUMULATE( SurfaceRender_geo_Draw );
			glBindVertexArray( surfaceDef.geo.vertexArrayObject );
			if ( surfaceDef.numInstances > 1 )
			{
				glDrawElementsInstanced( surfaceDef.geo.primitiveType, surfaceDef.geo.indexCount, surfaceDef.geo.IndexType, NULL, surfaceDef.numInstances );
			}
			else
			{
				glDrawElements( surfaceDef.geo.primitiveType, surfaceDef.geo.indexCount, surfaceDef.geo.IndexType, NULL );
			}
		}

		GL_CheckErrors( surfaceDef.surfaceName.ToCStr() );
	}

	// set the gpu state back to the default
	ChangeGpuState( currentGpuState, ovrGpuState() );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glUseProgram( 0 );
	glBindVertexArray( 0 );

	OVR_PERF_REPORT( SurfaceRender_ChangeGpuState );
	OVR_PERF_REPORT( SurfaceRender_ChangeProgram );
	OVR_PERF_REPORT( SurfaceRender_UpdateUniforms );
	OVR_PERF_REPORT( SurfaceRender_geo_Draw );
	return counters;
}

}	// namespace OVR
