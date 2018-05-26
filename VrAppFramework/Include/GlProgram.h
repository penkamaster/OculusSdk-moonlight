/************************************************************************************

Filename    :   GlProgram.h
Content     :   Shader program compilation.
Created     :   October 11, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_GlProgram_h
#define OVR_GlProgram_h

#include "GpuState.h"
#include "GlTexture.h"
#include "GlBuffer.h"
#include "Kernel/OVR_Math.h"

namespace OVR
{

// STRINGIZE is used so program text strings can include lines like:
// "uniform highp mat4 Joints["MAX_JOINTS_STRING"];\n"

#define STRINGIZE( x )			#x
#define STRINGIZE_VALUE( x )	STRINGIZE( x )

#define MAX_JOINTS				64
#define MAX_JOINTS_STRING		STRINGIZE_VALUE( MAX_JOINTS )

// No attempt is made to support sharing shaders between programs,
// it isn't worth avoiding the duplication.

// Shader uniforms Texture0 - Texture7 are bound to texture units 0 - 7

enum VertexAttributeLocation
{
	VERTEX_ATTRIBUTE_LOCATION_POSITION		= 0,
	VERTEX_ATTRIBUTE_LOCATION_NORMAL		= 1,
	VERTEX_ATTRIBUTE_LOCATION_TANGENT		= 2,
	VERTEX_ATTRIBUTE_LOCATION_BINORMAL		= 3,
	VERTEX_ATTRIBUTE_LOCATION_COLOR			= 4,
	VERTEX_ATTRIBUTE_LOCATION_UV0			= 5,
	VERTEX_ATTRIBUTE_LOCATION_UV1			= 6,
	VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES	= 7,
	VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS	= 8,
	VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS	= 9
};

enum class ovrProgramParmType : char
{
	INT,				// int
	INT_VECTOR2,		// Vector2i
	INT_VECTOR3,		// Vector3i
	INT_VECTOR4,		// Vector4i
	FLOAT,				// float
	FLOAT_VECTOR2,		// Vector2f
	FLOAT_VECTOR3,		// Vector3f
	FLOAT_VECTOR4,		// Vector4f
	FLOAT_MATRIX4,		// Matrix4f (always inverted for GL)
	TEXTURE_SAMPLED,	// GlTexture
	BUFFER_UNIFORM,		// read-only uniform buffer (GLSL: uniform)
	MAX
};

struct ovrProgramParm
{
	const char *		Name;
	ovrProgramParmType	Type;
};

struct ovrUniform
{
	// can be made as high as 16
	static const int MAX_UNIFORMS = 8;

	ovrUniform() :
	  Location( -1 )
	, Binding( -1 )
	, Type( ovrProgramParmType::MAX )
	{
	}

	int					Location;		// the index of the uniform in the render program
	int					Binding;		// the resource binding (eg. texture image unit or uniform block)
	ovrProgramParmType	Type;			// the type of the data
};

struct ovrUniformData
{
	ovrUniformData()
		: Data( nullptr )
		, Count( 1 )
	{
	}

	void *			Data;	// pointer to data
	int				Count;	// number of items of ovrProgramParmType in the Data buffer
};

//==============================================================
// GlProgram
// Freely copyable. In general, the compilation unit that calls Build() should
// be the compilation unit that calls Free(). Other copies of the object should
// never Free().
struct GlProgram
{
	GlProgram()
		: Program( 0 )
		, VertexShader( 0 )
		, FragmentShader( 0 )
		, Uniforms()
		, numTextureBindings( 0 )
		, numUniformBufferBindings( 0 )
		, uMvp( -1 )
		, uModel( -1 )
		, uColor( -1 )
		, uFadeDirection( -1 )
		, uTexm( -1 )
		, uColorTableOffset( -1 )
		, uClipUVs( -1 )
		, uJoints( -1 )
		, uJointsBinding( -1 )
		, uUVOffset( -1 )
	{
	}

#if defined( OVR_OS_ANDROID )
	static const int GLSL_PROGRAM_VERSION = 300;		// Minimum requirement for multiview support.
#else
	static const int GLSL_PROGRAM_VERSION = 150;
#endif

	static GlProgram			Build( const char * vertexSrc, const char * fragmentSrc,
									   const ovrProgramParm * parms, const int numParms,
									   const int programVersion = GLSL_PROGRAM_VERSION,		// minimum requirement
									   bool abortOnError = true,
									   bool useDeprecatedInterface = false );

	// Use this build variant for specifying extensions or other program directives.
	static GlProgram			Build( const char * vertexDirectives, const char * vertexSrc,
									   const char * fragmentDirectives, const char * fragmentSrc,
									   const ovrProgramParm * parms, const int numParms,
									   const int programVersion = GLSL_PROGRAM_VERSION,		// minimum requirement
									   bool abortOnError = true,
									   bool useDeprecatedInterface = false );

	static void					Free( GlProgram & program );

	static void					SetUseMultiview( const bool useMultiview_ );

	bool						IsValid() const { return Program != 0; }

	static const int			MAX_VIEWS = 2;
	static const int			SCENE_MATRICES_UBO_SIZE = 2 * sizeof( Matrix4f ) * MAX_VIEWS;

	unsigned int				Program;
	unsigned int				VertexShader;
	unsigned int 				FragmentShader;

	// Globally-defined system level uniforms.
	ovrUniform					ViewID; 		// uniform for ViewID; is -1 if OVR_multiview unavailable or disabled
	ovrUniform					ModelMatrix;	// uniform for "uniform mat4 ModelMatrix;"
	ovrUniform					SceneMatrices;	// uniform for "SceneMatrices" ubo :
												// uniform SceneMatrices {
												//   mat4 ViewMatrix[NUM_VIEWS];
												//   mat4 ProjectionMatrix[NUM_VIEWS];
												// } sm;

	// ----IMAGE_EXTERNAL_WORKAROUND
	ovrUniform					ViewMatrix;
	ovrUniform					ProjectionMatrix;
	// ----IMAGE_EXTERNAL_WORKAROUND

	ovrUniform					Uniforms[ovrUniform::MAX_UNIFORMS];
	int							numTextureBindings;
	int							numUniformBufferBindings;

	// ----DEPRECATED_GLPROGRAM
	// deprecated interface - to be removed
	bool UseDeprecatedInterface;

	// Uniforms that aren't found will have a -1 value
	int		uMvp;				// uniform Mvpm
	int		uModel;				// uniform Modelm
	int		uColor;				// uniform UniformColor
	int		uFadeDirection;		// uniform FadeDirection
	int		uTexm;				// uniform Texm
	int		uColorTableOffset;	// uniform offset to apply to color table index
	int		uClipUVs;			// uniform min / max UVs for fragment clipping
	int		uJoints;			// uniform Joints ubo block index
	int		uJointsBinding;		// uniform Joints ubo binding
	int		uUVOffset;			// uniform uv offset
	// ----DEPRECATED_GLPROGRAM
};

struct ovrGraphicsCommand
{
	GlProgram					Program;
	ovrGpuState					GpuState;
	ovrUniformData				UniformData[ovrUniform::MAX_UNIFORMS];	// data matching the types in Program.Uniforms[]

	// ----DEPRECATED_GLPROGRAM
	// the old ovrMaterialDef interface... this will go away eventually!
	bool UseDeprecatedInterface;
	ovrGraphicsCommand()
	: UseDeprecatedInterface( false )
	, numUniformTextures( 0 )
	{
		for ( int i = 0; i < ovrUniform::MAX_UNIFORMS; ++i )
		{
			uniformSlots[i] = -1;
		}
	}

	// Parameter setting stops when uniformSlots[x] == -1
	GLint		uniformSlots[ovrUniform::MAX_UNIFORMS];
	GLfloat		uniformValues[ovrUniform::MAX_UNIFORMS][4];

	// Currently assumes GL_TEXTURE_2D for all.
	//
	// There should never be any 0 textures in the active elements.
	//
	// This should be a range checked container.
	int			numUniformTextures;
	GlTexture	uniformTextures[ovrUniform::MAX_UNIFORMS];
	// There should only be one joint uniform buffer for the deprecated path.
	GlBuffer	uniformJoints;
	// ----DEPRECATED_GLPROGRAM
};

// ----DEPRECATED_GLPROGRAM
// Will abort() after logging an error if either compiles or the link status
// fails, but not if uniforms are missing.
GlProgram	BuildProgram( const char * vertexSrc, const char * fragmentSrc, const int programVersion = GlProgram::GLSL_PROGRAM_VERSION );

// Use this build variant for specifying extensions or other program directives.
GlProgram	BuildProgram( const char * vertexDirectives, const char * vertexSrc,
						  const char * fragmentDirectives, const char * fragmentSrc,
						  const int programVersion = GlProgram::GLSL_PROGRAM_VERSION );

void		DeleteProgram( GlProgram & prog );
// ----DEPRECATED_GLPROGRAM

}	// namespace OVR

#endif	// OVR_GlProgram_h
