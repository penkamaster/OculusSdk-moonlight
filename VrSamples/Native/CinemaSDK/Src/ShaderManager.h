/************************************************************************************

Filename    :   ShaderManager.h
Content     :	Allocates and builds shader programs.
Created     :	7/3/2014
Authors     :   Jim Dosé and John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( ShaderManager_h )
#define ShaderManager_h

#include "GlProgram.h"
#include "ModelFile.h"

using namespace OVR;

namespace OculusCinema {

class CinemaApp;

enum sceneProgram_t
{
	SCENE_PROGRAM_BLACK,
	SCENE_PROGRAM_STATIC_ONLY,
	SCENE_PROGRAM_STATIC_DYNAMIC,
	SCENE_PROGRAM_DYNAMIC_ONLY,
	SCENE_PROGRAM_ADDITIVE,
	SCENE_PROGRAM_MAX
};

class ShaderManager
{
public:
							ShaderManager( CinemaApp &cinema );

	void					OneTimeInit( const char * launchIntent );
	void					OneTimeShutdown();

	CinemaApp &				Cinema;

	// Render the external image texture to a conventional texture to allow
	// mipmap generation.
	GlProgram				CopyMovieProgram;
	GlProgram				MovieExternalUiProgram;
	GlProgram				UniformColorProgram;

	GlProgram				ProgVertexColor;
	GlProgram				ProgSingleTexture;
	GlProgram				ProgLightMapped;
	GlProgram				ProgReflectionMapped;
	GlProgram				ProgSkinnedVertexColor;
	GlProgram				ProgSkinnedSingleTexture;
	GlProgram				ProgSkinnedLightMapped;
	GlProgram				ProgSkinnedReflectionMapped;

	GlProgram				ScenePrograms[ SCENE_PROGRAM_MAX ];

	ModelGlPrograms 		DynamicPrograms;
	ModelGlPrograms 		DefaultPrograms;

private:
	ShaderManager &			operator=( const ShaderManager & );
};

} // namespace OculusCinema

#endif // ShaderManager_h
