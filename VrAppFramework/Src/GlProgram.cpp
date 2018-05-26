/************************************************************************************

Filename    :   GlProgram.cpp
Content     :   Shader program compilation.
Created     :   October 11, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "GlProgram.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_String_Utils.h"

namespace OVR
{
static bool UseMultiview = false;

// All GlPrograms implicitly get the VertexHeader
static const char * VertexHeader =
#if defined( OVR_OS_ANDROID )
R"=====(
#ifndef DISABLE_MULTIVIEW
 #define DISABLE_MULTIVIEW 0
#endif
#define NUM_VIEWS 2
#if __VERSION__ < 300
  #define in attribute
  #define out varying
#else
  #define attribute in
  #define varying out
#endif
#if defined( GL_OVR_multiview2 ) && ! DISABLE_MULTIVIEW && __VERSION__ >= 300
  #extension GL_OVR_multiview2 : require
  layout(num_views=NUM_VIEWS) in;
  #define VIEW_ID gl_ViewID_OVR
#else
  uniform lowp int ViewID;
  #define VIEW_ID ViewID
#endif

uniform highp mat4 ModelMatrix;
#if __VERSION__ >= 300
// Use a ubo in v300 path to workaround corruption issue on Adreno 420+v300
// when uniform array of matrices used.
uniform SceneMatrices
{
	highp mat4 ViewMatrix[NUM_VIEWS];
	highp mat4 ProjectionMatrix[NUM_VIEWS];
} sm;
// Use a function macro for TransformVertex to workaround an issue on exynos8890+Android-M driver:
// gl_ViewID_OVR outside of the vertex main block causes VIEW_ID to be 0 for every view.
// NOTE: Driver fix has landed with Android-N.
//highp vec4 TransformVertex( highp vec4 oPos )
//{
//	highp vec4 hPos = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * oPos ) );
//	return hPos;
//}
#define TransformVertex(localPos) (sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * localPos )))
#else
// Still support v100 for image_external as v300 support is lacking in Mali-T760/Android-L drivers.
uniform highp mat4 ViewMatrix[NUM_VIEWS];
uniform highp mat4 ProjectionMatrix[NUM_VIEWS];
highp vec4 TransformVertex( highp vec4 oPos )
{
	highp vec4 hPos = ProjectionMatrix[VIEW_ID] * ( ViewMatrix[VIEW_ID] * ( ModelMatrix * oPos ) );
	return hPos;
}
#endif
)====="
#else
	"#define attribute in\n"
	"#define varying out\n"
	"#define NUM_VIEWS 2\n"
	"uniform int ViewID;\n"
	"#define VIEW_ID ViewID\n"
	"uniform highp mat4 ModelMatrix;\n"
	"uniform SceneMatrices\n"
	"{\n"
		"highp mat4 ViewMatrix[NUM_VIEWS];\n"
		"highp mat4 ProjectionMatrix[NUM_VIEWS];\n"
	"} sm;\n"
	"highp vec4 TransformVertex( highp vec4 oPos )\n"
	"{\n"
		"highp vec4 hPos = sm.ProjectionMatrix[ VIEW_ID ] * ( sm.ViewMatrix[ VIEW_ID ] * ( ModelMatrix * oPos ) );\n"
		"return hPos;\n"
	"}\n"
#endif
;

// All GlPrograms implicitly get the FragmentHeader
static const char * FragmentHeader =
#if defined( OVR_OS_ANDROID )
R"=====(
#if __VERSION__ < 300
	#define in varying
#else
	#define varying in
	#define gl_FragColor fragColor
	out mediump vec4 fragColor;
	#define texture2D texture
	#define textureCube texture
#endif
)====="
#else
    "#define varying in\n"
    "#define texture2D texture\n"
    "#define textureCube texture\n"
    "#define gl_FragColor ovr_FragColor\n"
    "out vec4 ovr_FragColor;\n"
#endif
;

// ----DEPRECATED_GLPROGRAM

GlProgram BuildProgram( const char * vertexDirectives, const char * vertexSrc, const char * fragmentDirectives, const char * fragmentSrc, const int programVersion )
{
	return GlProgram::Build( vertexDirectives, vertexSrc, fragmentDirectives, fragmentSrc, NULL, 0, programVersion, true /* abort on error */, true /* use deprecated interface */ );
}

GlProgram BuildProgram( const char * vertexSrc, const char * fragmentSrc, const int programVersion )
{
	return BuildProgram( NULL, vertexSrc, NULL, fragmentSrc, programVersion );
}

void DeleteProgram( GlProgram & prog )
{
	GlProgram::Free( prog );
}
// ----DEPRECATED_GLPROGRAM

static const char * FindShaderVersionEnd( const char * src )
{
	if ( src == NULL || OVR_strncmp( src, "#version ", 9 ) != 0 )
	{
		return src;
	}
	while ( *src != 0 && *src != '\n' )
	{
		src++;
	}
	if ( *src == '\n' )
	{
		src++;
	}
	return src;
}

#if 0
static const char * FindShaderDirectivesEnd( const char * src )
{
	const char * next = src;
	if ( next == NULL )
	{
		return NULL;
	}
	enum State { EAT_WHITESPACE, EAT_TO_NEW_LINE };
	State state = EAT_WHITESPACE;
	while ( *next )
	{
		char c = *next;
		if ( state == EAT_WHITESPACE )
		{
			if ( c == '#' )
			{
				state = EAT_TO_NEW_LINE;
			}
			else if ( c != ' ' && c != '\t' && c != '\n' )
			{
				break;
			}
		}
		else if ( state == EAT_TO_NEW_LINE )
		{
			if ( c == '\n' )
			{
				state = EAT_WHITESPACE;
			}
		}
		next++;
	}
	return next;
}
#endif

static GLuint CompileShader( GLenum shaderType, const char * directives, const char * src, GLint programVersion )
{
	const char * postVersion = FindShaderVersionEnd( src );
	if ( postVersion != src )
	{
		WARN( "GlProgram: #version in source is not supported. Specify at program build time." );
	}

	OVR::String srcString;

#if defined( OVR_OS_ANDROID )
	// Valid versions for GL ES:
	// #version 100      -- ES 2.0
	// #version 300 es	 -- ES 3.0
	// #version 310 es	 -- ES 3.1
	const char * versionModifier = ( programVersion > 100 ) ? "es" : "";
#else
	const char * versionModifier = "";
#endif
	srcString = StringUtils::Va( "#version %d %s\n", programVersion, versionModifier );

	if ( directives != NULL )
	{
		srcString.AppendString( directives );
	}

	srcString.AppendString( StringUtils::Va( "#define DISABLE_MULTIVIEW %d\n", ( UseMultiview ) ? 0 : 1 ) );

	if ( shaderType == GL_VERTEX_SHADER )
	{
		srcString.AppendString( VertexHeader );
	}
	else if ( shaderType == GL_FRAGMENT_SHADER )
	{
		srcString.AppendString( FragmentHeader );
	}

	srcString.AppendString( postVersion );

	src = srcString.ToCStr();

	GLuint shader = glCreateShader( shaderType );

	const int numSources = 1;
	const char * srcs[1];
	srcs[0] = src;

	glShaderSource( shader, numSources, srcs, 0 );
	glCompileShader( shader );

	GLint r;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &r );
	if ( r == GL_FALSE )
	{
		WARN( "Compiling %s shader: ****** failed ******\n", shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment" );
		GLchar msg[1024];
		const char * sp = src;
		int charCount = 0;
		int line = 0;
		do
		{
			if ( *sp != '\n' )
			{
				msg[charCount++] = *sp;
				msg[charCount] = 0;
			}
			if ( *sp == 0 || *sp == '\n' || charCount == 1023 )
			{
				charCount = 0;
				line++;
				WARN( "%03d  %s", line, msg );
				msg[0] = 0;
				if ( *sp != '\n' )
				{
					line--;
				}
			}
			sp++;
		} while( *sp != 0 );
		if ( charCount != 0 )
		{
			line++;
			WARN( "%03d  %s", line, msg );
		}
		glGetShaderInfoLog( shader, sizeof( msg ), 0, msg );
		WARN( "%s\n", msg );
		glDeleteShader( shader );
		return 0;
	}
	return shader;
}

GlProgram GlProgram::Build( const char * vertexSrc,
							const char * fragmentSrc,
							const ovrProgramParm * parms, const int numParms,
							const int requestedProgramVersion,
							bool abortOnError, bool useDeprecatedInterface )
{
	return Build( NULL, vertexSrc, NULL, fragmentSrc, parms, numParms,
					requestedProgramVersion, abortOnError, useDeprecatedInterface );
}

GlProgram GlProgram::Build( const char * vertexDirectives, const char * vertexSrc,
							const char * fragmentDirectives, const char * fragmentSrc,
							const ovrProgramParm * parms, const int numParms,
							const int requestedProgramVersion,
							bool abortOnError, bool useDeprecatedInterface )
{
	GlProgram p;
	p.UseDeprecatedInterface = useDeprecatedInterface;

	//--------------------------
	// Compile and Create the Program
	//--------------------------

	int programVersion = requestedProgramVersion;
	if ( programVersion < GLSL_PROGRAM_VERSION )
	{
		WARN( "GlProgram: Program GLSL version requested %d, but does not meet required minimum %d",
			requestedProgramVersion, GLSL_PROGRAM_VERSION );
		programVersion = GLSL_PROGRAM_VERSION;
	}
#if defined( OVR_OS_ANDROID )
	// ----IMAGE_EXTERNAL_WORKAROUND
	// GL_OES_EGL_image_external extension support issues:
	// -- Both Adreno and Mali drivers do not maintain support for base GL_OES_EGL_image_external
	//    when compiled against v300. Instead, GL_OES_EGL_image_external_essl3 is required.
	// -- Mali drivers for T760 + Android-L currently list both extensions as unsupported under v300
	//    and fail to recognize 'samplerExternalOES' on compile.
	// P0003: Warning: Extension 'GL_OES_EGL_image_external' not supported
	// P0003: Warning: Extension 'GL_OES_EGL_image_external_essl3' not supported
	// L0001: Expected token '{', found 'identifier' (samplerExternalOES)
	// 
	// Currently, it appears that drivers which fully support multiview also support 
	// GL_OES_EGL_image_external_essl3 with v300. In the case where multiview is not
	// fully supported, we force the shader version to v100 in order to maintain support
	// for image_external with the Mali T760+Android-L drivers.
	if ( !UseMultiview && (
			( fragmentDirectives != NULL && strstr( fragmentDirectives, "GL_OES_EGL_image_external" ) != NULL ) ||
			( strstr( fragmentSrc, "GL_OES_EGL_image_external" ) != NULL ) ) )
	{
		LOG( "GlProgram: Program GLSL version v100 due to GL_OES_EGL_image_external use." );
		programVersion = 100;
	}
	// ----IMAGE_EXTERNAL_WORKAROUND
#endif
	p.VertexShader = CompileShader( GL_VERTEX_SHADER, vertexDirectives, vertexSrc, programVersion );
	if ( p.VertexShader == 0 )
	{
		Free( p );
		if ( abortOnError )
		{
			FAIL( "Failed to compile vertex shader" );
		}
		return GlProgram();
	}

	p.FragmentShader = CompileShader( GL_FRAGMENT_SHADER, fragmentDirectives, fragmentSrc, programVersion );
	if ( p.FragmentShader == 0 )
	{
		Free( p );
		if ( abortOnError )
		{
			FAIL( "Failed to compile fragment shader" );
		}
		return GlProgram();
	}

	p.Program = glCreateProgram();
	glAttachShader( p.Program, p.VertexShader );
	glAttachShader( p.Program, p.FragmentShader );

	//--------------------------
	// Set attributes before linking
	//--------------------------

	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_POSITION,		"Position" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_NORMAL,			"Normal" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_TANGENT,			"Tangent" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_BINORMAL,		"Binormal" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_COLOR,			"VertexColor" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_UV0,				"TexCoord" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_UV1,				"TexCoord1" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_JOINT_INDICES,	"JointIndices" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_JOINT_WEIGHTS,	"JointWeights" );
	glBindAttribLocation( p.Program, VERTEX_ATTRIBUTE_LOCATION_FONT_PARMS,		"FontParms" );

	//--------------------------
	// Link Program
	//--------------------------

	glLinkProgram( p.Program );

	GLint linkStatus;
	glGetProgramiv( p.Program, GL_LINK_STATUS, &linkStatus );
	if ( linkStatus == GL_FALSE )
	{
		GLchar msg[1024];
		glGetProgramInfoLog( p.Program, sizeof( msg ), 0, msg );
		Free( p );
		LOG( "Linking program failed: %s\n", msg );
		if ( abortOnError )
		{
			FAIL( "Failed to link program" );
		}
		return GlProgram();
	}

	//--------------------------
	// Determine Uniform Parm Location and Binding.
	//--------------------------

	p.numTextureBindings = 0;
	p.numUniformBufferBindings = 0;

	// Globally-defined system level uniforms.
	{
		p.ViewID.Type	  = ovrProgramParmType::INT;
		p.ViewID.Location = glGetUniformLocation( p.Program, "ViewID" );
		p.ViewID.Binding  = p.ViewID.Location;

		p.SceneMatrices.Type = ovrProgramParmType::BUFFER_UNIFORM;
		p.SceneMatrices.Location = glGetUniformBlockIndex( p.Program, "SceneMatrices" );
		if ( p.SceneMatrices.Location >= 0 )	// this won't be present for v100 shaders.
		{
			p.SceneMatrices.Binding = p.numUniformBufferBindings++;
			glUniformBlockBinding( p.Program, p.SceneMatrices.Location, p.SceneMatrices.Binding );
		}

		p.ModelMatrix.Type	 = ovrProgramParmType::FLOAT_MATRIX4;
		p.ModelMatrix.Location = glGetUniformLocation( p.Program, "ModelMatrix" );
		p.ModelMatrix.Binding  = p.ModelMatrix.Location;

		// ----IMAGE_EXTERNAL_WORKAROUND
		p.ViewMatrix.Type	 = ovrProgramParmType::FLOAT_MATRIX4;
		p.ViewMatrix.Location = glGetUniformLocation( p.Program, "ViewMatrix" );
		p.ViewMatrix.Binding  = p.ViewMatrix.Location;

		p.ProjectionMatrix.Type	 = ovrProgramParmType::FLOAT_MATRIX4;
		p.ProjectionMatrix.Location = glGetUniformLocation( p.Program, "ProjectionMatrix" );
		p.ProjectionMatrix.Binding  = p.ProjectionMatrix.Location;
		// ----IMAGE_EXTERNAL_WORKAROUND
	}

	// ----DEPRECATED_GLPROGRAM
	// old materialDef members - these will go away soon
	p.uMvp				= glGetUniformLocation( p.Program, "Mvpm" );
	p.uModel			= glGetUniformLocation( p.Program, "Modelm" );
	p.uColor			= glGetUniformLocation( p.Program, "UniformColor" );
	p.uFadeDirection	= glGetUniformLocation( p.Program, "UniformFadeDirection" );
	p.uTexm				= glGetUniformLocation( p.Program, "Texm" );
	p.uColorTableOffset = glGetUniformLocation( p.Program, "ColorTableOffset" );
	p.uClipUVs			= glGetUniformLocation( p.Program, "ClipUVs" );
	p.uJoints			= glGetUniformBlockIndex( p.Program, "JointMatrices" );
	p.uUVOffset			= glGetUniformLocation( p.Program, "UniformUVOffset" );
	if ( p.uJoints != -1 )
	{
		p.uJointsBinding = p.numUniformBufferBindings++;
		glUniformBlockBinding( p.Program, p.uJoints, p.uJointsBinding );
	}
	// ^^ old materialDef members - these should go away eventually - ideally soon
	// ----DEPRECATED_GLPROGRAM

	glUseProgram( p.Program );

	for ( int i = 0; i < numParms; ++i )
	{
		OVR_ASSERT( parms[i].Type != ovrProgramParmType::MAX );
		p.Uniforms[i].Type = parms[i].Type;
		if ( parms[i].Type == ovrProgramParmType::TEXTURE_SAMPLED )
		{
			p.Uniforms[i].Location = static_cast<int16_t>( glGetUniformLocation( p.Program, parms[i].Name ) );
			p.Uniforms[i].Binding = p.numTextureBindings++;
			glUniform1i( p.Uniforms[i].Location, p.Uniforms[i].Binding );
		}
		else if ( parms[i].Type == ovrProgramParmType::BUFFER_UNIFORM )
		{
			p.Uniforms[i].Location = glGetUniformBlockIndex( p.Program, parms[i].Name );
			p.Uniforms[i].Binding = p.numUniformBufferBindings++;
			glUniformBlockBinding( p.Program, p.Uniforms[i].Location, p.Uniforms[i].Binding );
		}
		else
		{
			p.Uniforms[i].Location = static_cast<int16_t>( glGetUniformLocation( p.Program, parms[i].Name ) );
			p.Uniforms[i].Binding = p.Uniforms[i].Location;
		}

#ifdef OVR_BUILD_DEBUG
		if ( p.Uniforms[i].Location < 0 || p.Uniforms[i].Binding < 0 )
		{
			LOG( "GlProgram::Build. Invalid shader parm: %s", parms[i].Name );
		}
#endif

		OVR_ASSERT( p.Uniforms[i].Location >= 0 && p.Uniforms[i].Binding >= 0 );
	}

	// implicit texture and image_external bindings
	for ( int i = 0; i < ovrUniform::MAX_UNIFORMS; i++ )
	{
		char name[32];
		sprintf( name, "Texture%i", i );
		const GLint uTex = glGetUniformLocation( p.Program, name );
		if ( uTex != -1 )
		{
			glUniform1i( uTex, i );
		}
	}

	glUseProgram( 0 );

	return p;
}

void GlProgram::Free( GlProgram & prog )
{
	glUseProgram( 0 );
	if ( prog.Program != 0 )
	{
		glDeleteProgram( prog.Program );
	}
	if ( prog.VertexShader != 0 )
	{
		glDeleteShader( prog.VertexShader );
	}
	if ( prog.FragmentShader != 0 )
	{
		glDeleteShader( prog.FragmentShader );
	}
	prog.Program = 0;
	prog.VertexShader = 0;
	prog.FragmentShader = 0;
}

void GlProgram::SetUseMultiview( const bool useMultiview_ )
{
	UseMultiview = useMultiview_;
}

}	// namespace OVR
