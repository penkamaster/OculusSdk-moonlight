/************************************************************************************

Filename    :   ModelRender.cpp
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "ModelRender.h"

#include <stdlib.h>
#include <algorithm>
#include "OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

namespace OVR
{

// Returns 0 if the bounds is culled by the mvp, otherwise returns the max W
// value of the bounds corners so it can be sorted into roughly front to back
// order for more efficient Z cull.  Sorting bounds in increasing order of
// their farthest W value usually makes characters and objects draw before
// the environments they are in, and draws sky boxes last, which is what we want.
static float BoundsSortCullKey( const Bounds3f & bounds, const Matrix4f & mvp )
{
	// Always cull empty bounds, which can be used to disable a surface.
	// Don't just check a single axis, or billboards would be culled.
	if ( bounds.b[1].x == bounds.b[0].x &&  bounds.b[1].y == bounds.b[0].y )
	{
		return 0;
	}

	// Not very efficient code...
	Vector4f c[8];
	for ( int i = 0; i < 8; i++ )
	{
		Vector4f world;
		world.x = bounds.b[(i&1)].x;
		world.y = bounds.b[(i&2)>>1].y;
		world.z = bounds.b[(i&4)>>2].z;
		world.w = 1.0f;

		c[i] = mvp.Transform( world );
	}

	int i;
	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].x > -c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].x < c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}

	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].y > -c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].y < c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}

	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].z > -c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}
	for ( i = 0; i < 8; i++ )
	{
		if ( c[i].z < c[i].w )
		{
			break;
		}
	}
	if ( i == 8 )
	{
		return 0;	// all off one side
	}

	// calculate the farthest W point for front to back sorting
	float maxW = 0;
	for ( i = 0; i < 8; i++ )
	{
		const float w = c[i].w;
		if ( w > maxW )
		{
			maxW = w;
		}
	}

	return maxW;		// couldn't cull
}

struct bsort_t
{
	float						key;
	Matrix4f					modelMatrix;
	const Array< Matrix4f > *	joints;
	const ovrSurfaceDef *		surface;
	bool						transparent;

	bool operator< (const bsort_t& b2) const
	{
		const bsort_t& b1 = *this;
		bool trans1 = b1.transparent;
		bool trans2 = b2.transparent;
		if ( trans1 == trans2 )
		{
			float f1 = b1.key;
			float f2 = b2.key;
			if ( !trans1 )
			{
				// both are solid, sort front-to-back
				return ( f1 < f2 );
			}
			else
			{
				// both are transparent, sort back-to-front
				return ( f2 < f1 );
			}
		}
		// otherwise, one is solid and one is translucent... the solid is always rendered first
		return !trans1;
	};
};

void BuildModelSurfaceList(	Array<ovrDrawSurface> & surfaceList,
							const Array<ModelNodeState *> & emitNodes,
							const Array<ovrDrawSurface> & emitSurfaces,
							const Matrix4f & viewMatrix,
							const Matrix4f & projectionMatrix )
{
	// A mobile GPU will be in trouble if it draws more than this.
	static const int MAX_DRAW_SURFACES = 1024;
	bsort_t	bsort[ MAX_DRAW_SURFACES ];

	const Matrix4f vpMatrix = projectionMatrix * viewMatrix;

	int	numSurfaces = 0;
	int	cullCount = 0;

	for ( int nodeNum = 0; nodeNum < emitNodes.GetSizeI(); nodeNum++ )
	{
		const ModelNodeState & nodeState = *emitNodes[nodeNum];
		if ( nodeState.GetNode() != NULL && nodeState.GetNode()->model != NULL )
		{
			// #TODO currently we aren't properly updating the geo local bounds for skinned animated objects.  Fix that.
			bool allowCulling = true;
			if ( nodeState.node->skinIndex >= 0 )
			{
				allowCulling = false;
			}

			if ( nodeState.GetNode()->model != nullptr )
			{
				const Model & modelDef = *nodeState.GetNode()->model;
				for ( int surfaceNum = 0; surfaceNum < modelDef.surfaces.GetSizeI(); surfaceNum++ )
				{
					const ovrSurfaceDef & surfaceDef = modelDef.surfaces[surfaceNum].surfaceDef;
					const float sort = BoundsSortCullKey( surfaceDef.geo.localBounds, vpMatrix * nodeState.GetGlobalTransform() );
					if ( sort == 0 )
					{
						if ( allowCulling )
						{
							if ( LogRenderSurfaces )
							{
								LOG( "Culled %s", surfaceDef.surfaceName.ToCStr() );
							}
							cullCount++;
							continue;
						}
						else
						{
							if ( LogRenderSurfaces )
							{
								LOG( "Skipped Culling of %s", surfaceDef.surfaceName.ToCStr() );
							}
						}
					}

					if ( numSurfaces == MAX_DRAW_SURFACES )
					{
						break;
					}

					// Update the Joint Uniform Buffer
					if ( nodeState.JointMatricesOvrScene.GetSize() > 0 )
					{
						static Matrix4f transposedJoints[MAX_JOINTS];
						const int numJoints = Alg::Min( nodeState.JointMatricesOvrScene.GetSizeI(), MAX_JOINTS );

						for ( int j = 0; j < numJoints; j++ )
						{
							transposedJoints[j] = nodeState.JointMatricesOvrScene[j].Transposed();
						}

						const size_t updateSize = numJoints * sizeof( Matrix4f );
						surfaceDef.graphicsCommand.uniformJoints.Update( updateSize, &transposedJoints[0] );
					}
					else if ( nodeState.node->skinIndex >= 0 )
					{
						const ModelSkin & skin = nodeState.state->mf->Skins[nodeState.node->skinIndex];
						
						static Matrix4f transposedJoints[MAX_JOINTS];
						const int numJoints = Alg::Min( skin.jointIndexes.GetSizeI(), MAX_JOINTS );

						Matrix4f inverseGlobalSkeletonTransform;
						if ( skin.skeletonRootIndex >= 0 )
						{
							inverseGlobalSkeletonTransform = nodeState.state->nodeStates[skin.skeletonRootIndex].GetGlobalTransform().Inverted();
						}
						else
						{
							inverseGlobalSkeletonTransform = nodeState.state->nodeStates[nodeState.node->parentIndex].GetGlobalTransform().Inverted();
						}

						for ( int j = 0; j < numJoints; j++ )
						{
							Matrix4f globalTransform  = nodeState.state->nodeStates[skin.jointIndexes[j]].GetGlobalTransform();
							Matrix4f tempTransform;
							Matrix4f::Multiply( &tempTransform, inverseGlobalSkeletonTransform, globalTransform );
							Matrix4f localJointTransform;

							if ( skin.inverseBindMatrices.GetSizeI() > 0 )
							{
								Matrix4f::Multiply( &localJointTransform, tempTransform, skin.inverseBindMatrices[j] );
							}
							else
							{
								WARN( "No inverse bind on modle" );
								localJointTransform = tempTransform;
							}

							transposedJoints[j] = localJointTransform.Transposed();
						}
						const size_t updateSize = numJoints * sizeof( Matrix4f );
						surfaceDef.graphicsCommand.uniformJoints.Update( updateSize, &transposedJoints[0] );
					}

					bsort[numSurfaces].key = sort;
					bsort[numSurfaces].modelMatrix = nodeState.GetGlobalTransform();
					bsort[numSurfaces].surface = &surfaceDef;
					bsort[numSurfaces].transparent = ( surfaceDef.graphicsCommand.GpuState.blendEnable != ovrGpuState::BLEND_DISABLE );
					numSurfaces++;
				}
			}
		}
	}

	for ( int i = 0; i < emitSurfaces.GetSizeI(); i++  )
	{
		const ovrDrawSurface & drawSurf = emitSurfaces[i];
		const ovrSurfaceDef & surfaceDef = *drawSurf.surface;
		const float sort = BoundsSortCullKey( surfaceDef.geo.localBounds, vpMatrix * drawSurf.modelMatrix );
		if ( sort == 0 )
		{
			if ( LogRenderSurfaces )
			{
				LOG( "Culled %s", surfaceDef.surfaceName.ToCStr() );
			}
			cullCount++;
			continue;
		}

		if ( numSurfaces == MAX_DRAW_SURFACES )
		{
			break;
		}

		bsort[ numSurfaces ].key = sort;
		bsort[ numSurfaces ].modelMatrix = drawSurf.modelMatrix;
		bsort[ numSurfaces ].surface = &surfaceDef;
		bsort[ numSurfaces ].transparent = ( surfaceDef.graphicsCommand.GpuState.blendEnable != ovrGpuState::BLEND_DISABLE );
		numSurfaces++;
	}

	//LOG( "Culled %i, draw %i", cullCount, numSurfaces );

	// sort by the far W and transparency
	// IMPORTANT: use a stable sort so surfaces with identical bounds
	// will sort consistently from frame to frame, rather than randomly
	// as happens with qsort.
	std::stable_sort( bsort, bsort+numSurfaces );

	// ----TODO_DRAWEYEVIEW : don't overwrite surfaces which may have already been added to the surfaceList.
	surfaceList.Resize( numSurfaces );
	for ( int i = 0; i < numSurfaces; i++ )
	{
		surfaceList[i].modelMatrix = bsort[i].modelMatrix;
		surfaceList[i].surface = bsort[i].surface;
	}
}

}	// namespace OVR
