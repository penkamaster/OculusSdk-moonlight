/************************************************************************************

Filename    :   ModelFile.h
Content     :   Model file loading.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef MODELFILE_H
#define MODELFILE_H

#include "ModelDef.h"

namespace OVR {

// A ModelFile is the in-memory representation of a digested model file.
// It should be imutable in normal circumstances, but it is ok to load
// and modify a model for a particular task, such as changing materials.
class ModelFile
{
public:
									ModelFile();
									ModelFile( const char * name ) : FileName( name ) {}
									~ModelFile();	// Frees all textures and geometry

	ovrSurfaceDef *					FindNamedSurface( const char * name ) const;
	const ModelTexture *			FindNamedTexture( const char * name ) const;
	const ModelJoint *				FindNamedJoint( const char * name ) const;

	// #TODO: deprecate, we should be doing things off Nodes instead of Tags now.
	const ModelTag *				FindNamedTag( const char * name ) const;

	Bounds3f						GetBounds() const;

public:
	String							FileName;
	bool							UsingSrgbTextures;

	float							animationStartTime;
	float							animationEndTime;

	Array< ModelTag >				Tags;

	// This is used by the movement code
	ModelCollision					Collisions;
	ModelCollision					GroundCollisions;

	// This is typically used for gaze selection.
	ModelTrace						TraceModel;

	Array< ModelBuffer >			Buffers;
	Array< ModelBufferView >		BufferViews;
	Array< ModelAccessor >			Accessors;
	Array< ModelTexture >			Textures;
	Array< ModelSampler >			Samplers;
	Array< ModelTextureWrapper >	TextureWrappers;
	Array< ModelMaterial >			Materials;
	Array< Model >					Models;
	Array< ModelCamera >			Cameras;
	Array< ModelNode >				Nodes;
	Array< ModelAnimation >			Animations;
	Array< ModelAnimationTimeLine >	AnimationTimeLines;
	Array< ModelSkin >				Skins;
	Array< ModelSubScene >			SubScenes;
};

// Pass in the programs that will be used for the model materials.
// Obviously not very general purpose.
// Returns nullptr if there is an error loading the file
ModelFile * LoadModelFileFromMemory( const char * fileName,
		const void * buffer, int bufferLength,
		const ModelGlPrograms & programs,
		const MaterialParms & materialParms,
		ModelGeo * outModelGeo = nullptr );

// Returns nullptr if there is an error loading the file
ModelFile * LoadModelFile( const char * fileName,
		const ModelGlPrograms & programs,
		const MaterialParms & materialParms );

// Returns nullptr if the file is not found.
ModelFile * LoadModelFileFromOtherApplicationPackage( void * zipFile, const char * nameInZip,
		const ModelGlPrograms & programs, const MaterialParms & materialParms );

// Returns nullptr if there is an error loading the file
ModelFile * LoadModelFileFromApplicationPackage( const char * nameInZip,
		const ModelGlPrograms & programs, const MaterialParms & materialParms );

// Returns nullptr if there is an error loading the file
ModelFile * LoadModelFile( class ovrFileSys & fileSys, const char * uri,
		const ModelGlPrograms & programs, const MaterialParms & materialParms );

} // namespace OVR

#endif	// MODELFILE_H
