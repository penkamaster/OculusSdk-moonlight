/************************************************************************************

Filename    :   OVR_GlUtils.cpp
Content     :   Policy-free OpenGL convenience functions
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_GlUtils.h"

#include "Kernel/OVR_LogUtils.h"

#include <string.h>
#include <stdlib.h>

ovrOpenGLExtensions extensionsOpenGL;

#if defined( ANDROID )

PFNGLDISCARDFRAMEBUFFEREXTPROC glDiscardFramebufferEXT_;

PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC  glRenderbufferStorageMultisampleEXT_;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT_;

PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR_;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR_;

PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_;
PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR_;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_;

// GL_OES_vertex_array_object
PFNGLBINDVERTEXARRAYOESPROC	glBindVertexArrayOES_;
PFNGLDELETEVERTEXARRAYSOESPROC	glDeleteVertexArraysOES_;
PFNGLGENVERTEXARRAYSOESPROC	glGenVertexArraysOES_;
PFNGLISVERTEXARRAYOESPROC	glIsVertexArrayOES_;

// QCOM_tiled_rendering
PFNGLSTARTTILINGQCOMPROC	glStartTilingQCOM_;
PFNGLENDTILINGQCOMPROC		glEndTilingQCOM_;

// EXT_disjoint_timer_query
PFNGLGENQUERIESEXTPROC glGenQueriesEXT_;
PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT_;
PFNGLISQUERYEXTPROC glIsQueryEXT_;
PFNGLBEGINQUERYEXTPROC glBeginQueryEXT_;
PFNGLENDQUERYEXTPROC glEndQueryEXT_;
PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT_;
PFNGLGETQUERYIVEXTPROC glGetQueryivEXT_;
PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT_;
PFNGLGETQUERYOBJECTUIVEXTPROC glGetQueryObjectuivEXT_;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64vEXT_;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT_;
PFNGLGETINTEGER64VPROC glGetInteger64v_;

// ES3 replacements to allow linking against ES2 libs
void           (*glBlitFramebuffer_) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
void           (*glRenderbufferStorageMultisample_) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
void           (*glInvalidateFramebuffer_) (GLenum target, GLsizei numAttachments, const GLenum* attachments);
GLvoid*        (*glMapBufferRange_) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLboolean      (*glUnmapBuffer_) (GLenum target);

#elif defined( WIN32 ) || defined( WIN64 ) || defined( _WIN32 ) || defined( _WIN64 )

PFNGLBINDFRAMEBUFFERPROC			glBindFramebuffer;
PFNGLGENFRAMEBUFFERSPROC			glGenFramebuffers;
PFNGLDELETEFRAMEBUFFERSPROC			glDeleteFramebuffers;
PFNGLBLITFRAMEBUFFERPROC			glBlitFramebuffer;
PFNGLGENRENDERBUFFERSPROC			glGenRenderbuffers;
PFNGLDELETERENDERBUFFERSPROC		glDeleteRenderbuffers;
PFNGLBINDRENDERBUFFERPROC			glBindRenderbuffer;
PFNGLISRENDERBUFFERPROC				glIsRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC		glRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFERPROC	glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC		glFramebufferTexture2D;
PFNGLCHECKFRAMEBUFFERSTATUSPROC		glCheckFramebufferStatus;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC		glRenderbufferStorageMultisample;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC  glRenderbufferStorageMultisampleEXT_;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT_;
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC		glFramebufferTextureMultiviewOVR_;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR_;

PFNGLGENBUFFERSPROC					glGenBuffers;
PFNGLDELETEBUFFERSPROC				glDeleteBuffers;
PFNGLBINDBUFFERPROC					glBindBuffer;
PFNGLBINDBUFFERBASEPROC				glBindBufferBase;
PFNGLBUFFERDATAPROC					glBufferData;
PFNGLBUFFERSUBDATAPROC				glBufferSubData;

PFNGLMAPBUFFERRANGEPROC				glMapBufferRange;
PFNGLUNMAPBUFFERPROC				glUnmapBuffer;

PFNGLDRAWELEMENTSINSTANCEDPROC		glDrawElementsInstanced;

PFNGLGENVERTEXARRAYSPROC			glGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC			glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC			glBindVertexArray;
PFNGLVERTEXATTRIB4FPROC				glVertexAttrib4f;
PFNGLVERTEXATTRIBDIVISORPROC		glVertexAttribDivisor;
PFNGLVERTEXATTRIBPOINTERPROC		glVertexAttribPointer;
PFNGLDISABLEVERTEXATTRIBARRAYPROC	glDisableVertexAttribArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC	glEnableVertexAttribArray;

PFNGLACTIVETEXTUREPROC				glActiveTexture;
PFNGLTEXIMAGE3DPROC					glTexImage3D;
PFNGLCOMPRESSEDTEXIMAGE2DPROC		glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE3DPROC		glCompressedTexImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC	glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC	glCompressedTexSubImage3D;

PFNGLTEXSTORAGE2DPROC				glTexStorage2D;
PFNGLTEXSTORAGE3DPROC				glTexStorage3D;

PFNGLGENERATEMIPMAPPROC				glGenerateMipmap;

PFNGLCREATEPROGRAMPROC				glCreateProgram;
PFNGLCREATESHADERPROC				glCreateShader;
PFNGLSHADERSOURCEPROC				glShaderSource;
PFNGLCOMPILESHADERPROC				glCompileShader;
PFNGLGETSHADERIVPROC				glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC			glGetShaderInfoLog;
PFNGLDELETESHADERPROC				glDeleteShader;
PFNGLDELETEPROGRAMPROC				glDeleteProgram;
PFNGLUSEPROGRAMPROC					glUseProgram;
PFNGLATTACHSHADERPROC				glAttachShader;
PFNGLLINKPROGRAMPROC				glLinkProgram;
PFNGLGETPROGRAMIVPROC				glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC			glGetProgramInfoLog;
PFNGLBINDATTRIBLOCATIONPROC			glBindAttribLocation;
PFNGLGETATTRIBLOCATIONPROC			glGetAttribLocation;
PFNGLGETUNIFORMLOCATIONPROC			glGetUniformLocation;
PFNGLGETUNIFORMBLOCKINDEXPROC		glGetUniformBlockIndex;
PFNGLUNIFORMBLOCKBINDINGPROC		glUniformBlockBinding;
PFNGLUNIFORM1FPROC					glUniform1f;
PFNGLUNIFORM1IVPROC					glUniform1iv;
PFNGLUNIFORM2IVPROC					glUniform2iv;
PFNGLUNIFORM3IVPROC					glUniform3iv;
PFNGLUNIFORM4IVPROC					glUniform4iv;
PFNGLUNIFORM2FVPROC					glUniform2fv;
PFNGLUNIFORM3FVPROC					glUniform3fv;
PFNGLUNIFORM4FPROC					glUniform4f;
PFNGLUNIFORM4FVPROC					glUniform4fv;
PFNGLUNIFORMMATRIX4FVPROC			glUniformMatrix4fv;
PFNGLUNIFORM1IPROC					glUniform1i;

PFNGLBLENDEQUATIONPROC				glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC		glBlendEquationSeparate;
PFNGLBLENDFUNCSEPARATEPROC			glBlendFuncSeparate;

PFNGLDEPTHRANGEFPROC                glDepthRangef;

PFNGLGENQUERIESPROC					glGenQueries;
PFNGLDELETEQUERIESPROC				glDeleteQueries;
PFNGLISQUERYPROC					glIsQuery;
PFNGLBEGINQUERYPROC					glBeginQuery;
PFNGLENDQUERYPROC					glEndQuery;
PFNGLQUERYCOUNTERPROC				glQueryCounter;
PFNGLGETQUERYIVPROC					glGetQueryiv;
PFNGLGETQUERYOBJECTIVPROC			glGetQueryObjectiv;
PFNGLGETQUERYOBJECTUIVPROC			glGetQueryObjectuiv;
PFNGLGETQUERYOBJECTI64VPROC			glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTUI64VPROC		glGetQueryObjectui64v;

PFNGLFENCESYNCPROC					glFenceSync;
PFNGLCLIENTWAITSYNCPROC				glClientWaitSync;
PFNGLDELETESYNCPROC					glDeleteSync;
PFNGLISSYNCPROC						glIsSync;

PFNWGLCREATECONTEXTATTRIBSARBPROC	wglCreateContextAttribsARB;
PFNWGLSWAPINTERVALEXTPROC			wglSwapIntervalEXT;
PFNWGLDELAYBEFORESWAPNVPROC			wglDelayBeforeSwapNV;

// To link against the ES2 library for UE4 on android, we need to make our own versions
// of these with underscore names, so include them on windows as well.
PFNGLBLITFRAMEBUFFERPROC				glBlitFramebuffer_;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC	glRenderbufferStorageMultisample_;
PFNGLINVALIDATEFRAMEBUFFERPROC			glInvalidateFramebuffer_;
PFNGLMAPBUFFERRANGEPROC					glMapBufferRange_;
PFNGLUNMAPBUFFERPROC					glUnmapBuffer_;

#elif defined( __APPLE__ )
	#error "not implemented"
#else
	#error "unknown platform"
#endif

/*
 * OpenGL convenience functions
 */
namespace OVR {

static void LogStringWords( const char * allExtensions )
{
	const char * start = allExtensions;
	while( 1 )
	{
		const char * end = strstr( start, " " );
		if ( end == NULL )
		{
			break;
		}
		unsigned int nameLen = (unsigned int)(end - start);
		if ( nameLen > 256 )
		{
			nameLen = 256;
		}
		char * word = new char[nameLen+1];
		memcpy( word, start, nameLen );
		word[nameLen] = '\0';
		LOG( "%s", word );
		delete[] word;

		start = end + 1;
	}
}

void * GetExtensionProc( const char * functionName )
{
#if defined( ANDROID )
	void * ptr = (void *)eglGetProcAddress( functionName );
#elif defined( WIN32 ) || defined( WIN64 ) || defined( _WIN32 ) || defined( _WIN64 )
	void * ptr = (void *)wglGetProcAddress( functionName );
#elif defined( __APPLE__ )
#error "not implemented"
	void * ptr = NULL;
#else
#error "unknown platform"
	void * ptr = NULL;
#endif
	if ( ptr == NULL )
	{
		LOG( "NOT FOUND: %s", functionName );
	}
	return ptr;
}

void GL_InitExtensions()
{
	// get extension pointers
	const char * extensions = (const char *)glGetString( GL_EXTENSIONS );
	if ( NULL == extensions )
	{
		LOG( "glGetString( GL_EXTENSIONS ) returned NULL" );
		return;
	}

	// Unfortunately, the Android log truncates strings over 1024 bytes long,
	// even if there are \n inside, so log each word in the string separately.
	LOG( "GL_EXTENSIONS:" );
	LogStringWords( extensions );

#if defined( ANDROID )
	const bool es3 = ( strncmp( (const char *)glGetString( GL_VERSION ), "OpenGL ES 3", 11 ) == 0 );
	LOG( "es3 = %s", es3 ? "TRUE" : "FALSE" );

	if ( GL_ExtensionStringPresent( "GL_EXT_discard_framebuffer", extensions ) )
	{
		extensionsOpenGL.EXT_discard_framebuffer = true;
		glDiscardFramebufferEXT_ = (PFNGLDISCARDFRAMEBUFFEREXTPROC)GetExtensionProc( "glDiscardFramebufferEXT" );
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_multisampled_render_to_texture", extensions ) )
	{
		glRenderbufferStorageMultisampleEXT_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)GetExtensionProc( "glRenderbufferStorageMultisampleEXT" );
		glFramebufferTexture2DMultisampleEXT_ = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)GetExtensionProc( "glFramebufferTexture2DMultisampleEXT" );
	}
	else if ( GL_ExtensionStringPresent( "GL_IMG_multisampled_render_to_texture", extensions ) )
	{
		glRenderbufferStorageMultisampleEXT_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)GetExtensionProc( "glRenderbufferStorageMultisampleIMG" );
		glFramebufferTexture2DMultisampleEXT_ = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)GetExtensionProc( "glFramebufferTexture2DMultisampleIMG" );
	}

	glFramebufferTextureMultiviewOVR_ = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) eglGetProcAddress( "glFramebufferTextureMultiviewOVR" );
	glFramebufferTextureMultisampleMultiviewOVR_ = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) eglGetProcAddress( "glFramebufferTextureMultisampleMultiviewOVR" );

	eglCreateSyncKHR_ = (PFNEGLCREATESYNCKHRPROC)GetExtensionProc( "eglCreateSyncKHR" );
	eglDestroySyncKHR_ = (PFNEGLDESTROYSYNCKHRPROC)GetExtensionProc( "eglDestroySyncKHR" );
	eglClientWaitSyncKHR_ = (PFNEGLCLIENTWAITSYNCKHRPROC)GetExtensionProc( "eglClientWaitSyncKHR" );
	eglSignalSyncKHR_ = (PFNEGLSIGNALSYNCKHRPROC)GetExtensionProc( "eglSignalSyncKHR" );
	eglGetSyncAttribKHR_ = (PFNEGLGETSYNCATTRIBKHRPROC)GetExtensionProc( "eglGetSyncAttribKHR" );

	if ( GL_ExtensionStringPresent( "GL_OES_vertex_array_object", extensions ) )
	{
		extensionsOpenGL.OES_vertex_array_object = true;
		glBindVertexArrayOES_ = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
		glDeleteVertexArraysOES_ = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
		glGenVertexArraysOES_ = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
		glIsVertexArrayOES_ = (PFNGLISVERTEXARRAYOESPROC)eglGetProcAddress("glIsVertexArrayOES");
	}

	if ( GL_ExtensionStringPresent( "GL_QCOM_tiled_rendering", extensions ) )
	{
		extensionsOpenGL.QCOM_tiled_rendering = true;
		glStartTilingQCOM_ = (PFNGLSTARTTILINGQCOMPROC)eglGetProcAddress("glStartTilingQCOM");
		glEndTilingQCOM_ = (PFNGLENDTILINGQCOMPROC)eglGetProcAddress("glEndTilingQCOM");
	}

	// Enabling this seems to cause strange problems in Unity
	if ( GL_ExtensionStringPresent( "GL_EXT_disjoint_timer_query", extensions ) )
	{
		extensionsOpenGL.EXT_disjoint_timer_query = true;
		glGenQueriesEXT_ = (PFNGLGENQUERIESEXTPROC)eglGetProcAddress("glGenQueriesEXT");
		glDeleteQueriesEXT_ = (PFNGLDELETEQUERIESEXTPROC)eglGetProcAddress("glDeleteQueriesEXT");
		glIsQueryEXT_ = (PFNGLISQUERYEXTPROC)eglGetProcAddress("glIsQueryEXT");
		glBeginQueryEXT_ = (PFNGLBEGINQUERYEXTPROC)eglGetProcAddress("glBeginQueryEXT");
		glEndQueryEXT_ = (PFNGLENDQUERYEXTPROC)eglGetProcAddress("glEndQueryEXT");
		glQueryCounterEXT_ = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterEXT");
		glGetQueryivEXT_ = (PFNGLGETQUERYIVEXTPROC)eglGetProcAddress("glGetQueryivEXT");
		glGetQueryObjectivEXT_ = (PFNGLGETQUERYOBJECTIVEXTPROC)eglGetProcAddress("glGetQueryObjectivEXT");
		glGetQueryObjectuivEXT_ = (PFNGLGETQUERYOBJECTUIVEXTPROC)eglGetProcAddress("glGetQueryObjectuivEXT");
		glGetQueryObjecti64vEXT_ = (PFNGLGETQUERYOBJECTI64VEXTPROC)eglGetProcAddress("glGetQueryObjecti64vEXT");
		glGetQueryObjectui64vEXT_  = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
		glGetInteger64v_  = (PFNGLGETINTEGER64VPROC)eglGetProcAddress("glGetInteger64v");
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_texture_sRGB_decode", extensions ) )
	{
		extensionsOpenGL.EXT_sRGB_texture_decode = true;
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_texture_border_clamp", extensions ) || GL_ExtensionStringPresent( "GL_OES_texture_border_clamp", extensions ) )
	{
		extensionsOpenGL.EXT_texture_border_clamp = true;
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_texture_filter_anisotropic", extensions ) )
	{
		extensionsOpenGL.EXT_texture_filter_anisotropic = true;
	}

	if ( GL_ExtensionStringPresent( "GL_OVR_multiview2", extensions ) &&
		 GL_ExtensionStringPresent( "GL_OVR_multiview_multisampled_render_to_texture", extensions ) )
	{
		extensionsOpenGL.OVR_multiview2 = true;
	}

	GLint MaxTextureSize = 0;
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &MaxTextureSize );
	LOG( "GL_MAX_TEXTURE_SIZE = %d", MaxTextureSize );

	GLint MaxVertexUniformVectors = 0;
	glGetIntegerv( GL_MAX_VERTEX_UNIFORM_VECTORS, &MaxVertexUniformVectors );
	LOG( "GL_MAX_VERTEX_UNIFORM_VECTORS = %d", MaxVertexUniformVectors );

	GLint MaxFragmentUniformVectors = 0;
	glGetIntegerv( GL_MAX_FRAGMENT_UNIFORM_VECTORS, &MaxFragmentUniformVectors );
	LOG( "GL_MAX_FRAGMENT_UNIFORM_VECTORS = %d", MaxFragmentUniformVectors );

	// ES3 functions we need to getprocaddress to allow linking against ES2 lib
	glBlitFramebuffer_  = (PFNGLBLITFRAMEBUFFER_)eglGetProcAddress("glBlitFramebuffer");
	glRenderbufferStorageMultisample_  = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_)eglGetProcAddress("glRenderbufferStorageMultisample");
	glInvalidateFramebuffer_  = (PFNGLINVALIDATEFRAMEBUFFER_)eglGetProcAddress("glInvalidateFramebuffer");
	glMapBufferRange_  = (PFNGLMAPBUFFERRANGE_)eglGetProcAddress("glMapBufferRange");
	glUnmapBuffer_  = (PFNGLUNMAPBUFFEROESPROC_)eglGetProcAddress("glUnmapBuffer");

#elif defined( WIN32 ) || defined( WIN64 ) || defined( _WIN32 ) || defined( _WIN64 )

	if ( GL_ExtensionStringPresent( "GL_OVR_multiview2", extensions ) )
	{
		extensionsOpenGL.OVR_multiview2 = true;
	}
	extensionsOpenGL.EXT_texture_border_clamp = true;

	glBindFramebuffer			= (PFNGLBINDFRAMEBUFFERPROC)			GetExtensionProc( "glBindFramebuffer" );
	glGenFramebuffers			= (PFNGLGENFRAMEBUFFERSPROC)			GetExtensionProc( "glGenFramebuffers" );
	glDeleteFramebuffers		= (PFNGLDELETEFRAMEBUFFERSPROC)			GetExtensionProc( "glDeleteFramebuffers" );
	glBlitFramebuffer			= (PFNGLBLITFRAMEBUFFERPROC)			GetExtensionProc( "glBlitFramebuffer" );
	glGenRenderbuffers			= (PFNGLGENRENDERBUFFERSPROC)			GetExtensionProc( "glGenRenderbuffers" );
	glDeleteRenderbuffers		= (PFNGLDELETERENDERBUFFERSPROC)		GetExtensionProc( "glDeleteRenderbuffers" );
	glBindRenderbuffer			= (PFNGLBINDRENDERBUFFERPROC)			GetExtensionProc( "glBindRenderbuffer" );
	glIsRenderbuffer			= (PFNGLISRENDERBUFFERPROC)				GetExtensionProc( "glIsRenderbuffer" );
	glRenderbufferStorage		= (PFNGLRENDERBUFFERSTORAGEPROC)		GetExtensionProc( "glRenderbufferStorage" );
	glFramebufferRenderbuffer	= (PFNGLFRAMEBUFFERRENDERBUFFERPROC)	GetExtensionProc( "glFramebufferRenderbuffer" );
	glFramebufferTexture2D		= (PFNGLFRAMEBUFFERTEXTURE2DPROC)		GetExtensionProc( "glFramebufferTexture2D" );
	glCheckFramebufferStatus	= (PFNGLCHECKFRAMEBUFFERSTATUSPROC)		GetExtensionProc( "glCheckFramebufferStatus" );
	glRenderbufferStorageMultisample =	(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) GetExtensionProc( "glRenderbufferStorageMultisample" );
	glRenderbufferStorageMultisampleEXT_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) GetExtensionProc( "glRenderbufferStorageMultisampleEXT" );
	//glFramebufferTexture2DMultisampleEXT_ = NULL;
	glFramebufferTextureMultiviewOVR_ = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) GetExtensionProc( "glFramebufferTextureMultiviewOVR" );
	glFramebufferTextureMultisampleMultiviewOVR_ = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) GetExtensionProc( "glFramebufferTextureMultisampleMultiviewOVR" );

	glGenBuffers				= (PFNGLGENBUFFERSPROC)					GetExtensionProc( "glGenBuffers" );
	glDeleteBuffers				= (PFNGLDELETEBUFFERSPROC)				GetExtensionProc( "glDeleteBuffers" );
	glBindBuffer				= (PFNGLBINDBUFFERPROC)					GetExtensionProc( "glBindBuffer" );
	glBindBufferBase			= (PFNGLBINDBUFFERBASEPROC)				GetExtensionProc( "glBindBufferBase" );
	glBufferData				= (PFNGLBUFFERDATAPROC)					GetExtensionProc( "glBufferData" );
	glBufferSubData				= (PFNGLBUFFERSUBDATAPROC)				GetExtensionProc( "glBufferSubData" );

	glMapBufferRange			= (PFNGLMAPBUFFERRANGEPROC)				GetExtensionProc( "glMapBufferRange" );
	glUnmapBuffer				= (PFNGLUNMAPBUFFERPROC)				GetExtensionProc( "glUnmapBuffer" );

	glDrawElementsInstanced		= (PFNGLDRAWELEMENTSINSTANCEDPROC)		GetExtensionProc( "glDrawElementsInstanced" );

	glGenVertexArrays			= (PFNGLGENVERTEXARRAYSPROC)			GetExtensionProc( "glGenVertexArrays" );
	glDeleteVertexArrays		= (PFNGLDELETEVERTEXARRAYSPROC)			GetExtensionProc( "glDeleteVertexArrays" );
	glBindVertexArray			= (PFNGLBINDVERTEXARRAYPROC)			GetExtensionProc( "glBindVertexArray" );
	glVertexAttrib4f			= (PFNGLVERTEXATTRIB4FPROC)				GetExtensionProc( "glVertexAttrib4f" );
	glVertexAttribDivisor		= (PFNGLVERTEXATTRIBDIVISORPROC)		GetExtensionProc( "glVertexAttribDivisor" );
	glVertexAttribPointer		= (PFNGLVERTEXATTRIBPOINTERPROC)		GetExtensionProc( "glVertexAttribPointer" );
	glDisableVertexAttribArray	= (PFNGLDISABLEVERTEXATTRIBARRAYPROC)	GetExtensionProc( "glDisableVertexAttribArray" );
	glEnableVertexAttribArray	= (PFNGLENABLEVERTEXATTRIBARRAYPROC)	GetExtensionProc( "glEnableVertexAttribArray" );

	glActiveTexture				= (PFNGLACTIVETEXTUREPROC)				GetExtensionProc( "glActiveTexture" );
	glTexImage3D				= (PFNGLTEXIMAGE3DPROC)					GetExtensionProc( "glTexImage3D" );
	glCompressedTexImage2D		= (PFNGLCOMPRESSEDTEXIMAGE2DPROC)		GetExtensionProc( "glCompressedTexImage2D");
	glCompressedTexImage3D		= (PFNGLCOMPRESSEDTEXIMAGE3DPROC)		GetExtensionProc( "glCompressedTexImage3D");
	glCompressedTexSubImage2D	= (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)	GetExtensionProc( "glCompressedTexSubImage2D" );
	glCompressedTexSubImage3D	= (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)	GetExtensionProc( "glCompressedTexSubImage3D" );

	glTexStorage2D				= (PFNGLTEXSTORAGE2DPROC)				GetExtensionProc( "glTexStorage2D" );
	glTexStorage3D				= (PFNGLTEXSTORAGE3DPROC)				GetExtensionProc( "glTexStorage3D" );

	glGenerateMipmap			= (PFNGLGENERATEMIPMAPPROC)				GetExtensionProc( "glGenerateMipmap" );

	glCreateProgram				= (PFNGLCREATEPROGRAMPROC)				GetExtensionProc( "glCreateProgram" );
	glCreateShader				= (PFNGLCREATESHADERPROC)				GetExtensionProc( "glCreateShader" );
	glShaderSource				= (PFNGLSHADERSOURCEPROC)				GetExtensionProc( "glShaderSource" );
	glCompileShader				= (PFNGLCOMPILESHADERPROC)				GetExtensionProc( "glCompileShader" );
	glGetShaderiv				= (PFNGLGETSHADERIVPROC)				GetExtensionProc( "glGetShaderiv" );
	glGetShaderInfoLog			= (PFNGLGETSHADERINFOLOGPROC)			GetExtensionProc( "glGetShaderInfoLog" );
	glDeleteShader				= (PFNGLDELETESHADERPROC)				GetExtensionProc( "glDeleteShader" );
	glDeleteProgram				= (PFNGLDELETEPROGRAMPROC)				GetExtensionProc( "glDeleteProgram" );
	glUseProgram				= (PFNGLUSEPROGRAMPROC)					GetExtensionProc( "glUseProgram" );
	glAttachShader				= (PFNGLATTACHSHADERPROC)				GetExtensionProc( "glAttachShader" );
	glLinkProgram				= (PFNGLLINKPROGRAMPROC)				GetExtensionProc( "glLinkProgram" );
	glGetProgramiv				= (PFNGLGETPROGRAMIVPROC)				GetExtensionProc( "glGetProgramiv" );
	glGetProgramInfoLog			= (PFNGLGETPROGRAMINFOLOGPROC)			GetExtensionProc( "glGetProgramInfoLog" );
	glBindAttribLocation		= (PFNGLBINDATTRIBLOCATIONPROC)			GetExtensionProc( "glBindAttribLocation" );
	glGetAttribLocation			= (PFNGLGETATTRIBLOCATIONPROC)			GetExtensionProc( "glGetAttribLocation" );
	glGetUniformLocation		= (PFNGLGETUNIFORMLOCATIONPROC)			GetExtensionProc( "glGetUniformLocation" );
	glGetUniformBlockIndex		= (PFNGLGETUNIFORMBLOCKINDEXPROC)		GetExtensionProc( "glGetUniformBlockIndex" );
	glUniformBlockBinding		= (PFNGLUNIFORMBLOCKBINDINGPROC)		GetExtensionProc( "glUniformBlockBinding" );
	glUniform1f					= (PFNGLUNIFORM1FPROC)					GetExtensionProc( "glUniform1f" );
	glUniform1iv				= (PFNGLUNIFORM1IVPROC)					GetExtensionProc( "glUniform1iv" );
	glUniform2iv				= (PFNGLUNIFORM2IVPROC)					GetExtensionProc( "glUniform2iv" );
	glUniform3iv				= (PFNGLUNIFORM3IVPROC)					GetExtensionProc( "glUniform3iv" );
	glUniform4iv				= (PFNGLUNIFORM4IVPROC)					GetExtensionProc( "glUniform4iv" );
	glUniform2fv				= (PFNGLUNIFORM2FVPROC)					GetExtensionProc( "glUniform2fv" );
	glUniform3fv				= (PFNGLUNIFORM3FVPROC)					GetExtensionProc( "glUniform3fv" );
	glUniform4f					= (PFNGLUNIFORM4FPROC)					GetExtensionProc( "glUniform4f" );
	glUniform4fv				= (PFNGLUNIFORM4FVPROC)					GetExtensionProc( "glUniform4fv" );
	glUniformMatrix4fv			= (PFNGLUNIFORMMATRIX4FVPROC)			GetExtensionProc( "glUniformMatrix4fv" );
	glUniform1i					= (PFNGLUNIFORM1IPROC)					GetExtensionProc( "glUniform1i" );

	glBlendEquation				= (PFNGLBLENDEQUATIONPROC)				glBlendEquation;
	glBlendEquationSeparate		= (PFNGLBLENDEQUATIONSEPARATEPROC)		glBlendEquationSeparate;
	glBlendFuncSeparate			= (PFNGLBLENDFUNCSEPARATEPROC)			glBlendFuncSeparate;

	glDepthRangef               = (PFNGLDEPTHRANGEFPROC)                GetExtensionProc( "glDepthRangef" );

	glBlendEquation				= (PFNGLBLENDEQUATIONPROC)				GetExtensionProc( "glBlendEquation" );
	glBlendEquationSeparate		= (PFNGLBLENDEQUATIONSEPARATEPROC)		GetExtensionProc( "glBlendEquationSeparate" );
	glBlendFuncSeparate			= (PFNGLBLENDFUNCSEPARATEPROC)			GetExtensionProc( "glBlendFuncSeparate" );

	glGenQueries				= (PFNGLGENQUERIESPROC)					GetExtensionProc( "glGenQueries" );
	glDeleteQueries				= (PFNGLDELETEQUERIESPROC)				GetExtensionProc( "glDeleteQueries" );
	glIsQuery					= (PFNGLISQUERYPROC)					GetExtensionProc( "glIsQuery" );
	glBeginQuery				= (PFNGLBEGINQUERYPROC)					GetExtensionProc( "glBeginQuery" );
	glEndQuery					= (PFNGLENDQUERYPROC)					GetExtensionProc( "glEndQuery" );
	glQueryCounter				= (PFNGLQUERYCOUNTERPROC)				GetExtensionProc( "glQueryCounter" );
	glGetQueryiv				= (PFNGLGETQUERYIVPROC)					GetExtensionProc( "glGetQueryiv" );
	glGetQueryObjectiv			= (PFNGLGETQUERYOBJECTIVPROC)			GetExtensionProc( "glGetQueryObjectiv" );
	glGetQueryObjectuiv			= (PFNGLGETQUERYOBJECTUIVPROC)			GetExtensionProc( "glGetQueryObjectuiv" );
	glGetQueryObjecti64v		= (PFNGLGETQUERYOBJECTI64VPROC)			GetExtensionProc( "glGetQueryObjecti64v" );
	glGetQueryObjectui64v		= (PFNGLGETQUERYOBJECTUI64VPROC)		GetExtensionProc( "glGetQueryObjectui64v" );

	glFenceSync					= (PFNGLFENCESYNCPROC)					GetExtensionProc( "glFenceSync" );
	glClientWaitSync			= (PFNGLCLIENTWAITSYNCPROC)				GetExtensionProc( "glClientWaitSync" );
	glDeleteSync				= (PFNGLDELETESYNCPROC)					GetExtensionProc( "glDeleteSync" );
	glIsSync					= (PFNGLISSYNCPROC)						GetExtensionProc( "glIsSync" );

	wglCreateContextAttribsARB	= (PFNWGLCREATECONTEXTATTRIBSARBPROC)	GetExtensionProc( "wglCreateContextAttribsARB" );
	wglSwapIntervalEXT			= (PFNWGLSWAPINTERVALEXTPROC)			GetExtensionProc( "wglSwapIntervalEXT" );
	wglDelayBeforeSwapNV		= (PFNWGLDELAYBEFORESWAPNVPROC)			GetExtensionProc( "wglDelayBeforeSwapNV" );

	// ES3 functions we need to getprocaddress to allow linking against ES2 lib
	glBlitFramebuffer_			= (PFNGLBLITFRAMEBUFFERPROC)GetExtensionProc("glBlitFramebuffer");
	glRenderbufferStorageMultisample_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)GetExtensionProc("glRenderbufferStorageMultisample");
	glInvalidateFramebuffer_	= (PFNGLINVALIDATEFRAMEBUFFERPROC)GetExtensionProc("glInvalidateFramebuffer");
	glMapBufferRange_			= (PFNGLMAPBUFFERRANGEPROC)GetExtensionProc("glMapBufferRange");
	glUnmapBuffer_				= (PFNGLUNMAPBUFFERPROC)GetExtensionProc("glUnmapBuffer");

#elif defined( __APPLE__ )
	#error "not implemented"
#else
	#error "unknown platform"
#endif
}

#if defined( ANDROID )
EGLConfig EglConfigForConfigID( const EGLDisplay display, const GLint configID )
{
	static const int MAX_CONFIGS = 1024;
	EGLConfig 	configs[MAX_CONFIGS];
	EGLint  	numConfigs = 0;

	if (EGL_FALSE == eglGetConfigs(display,
		configs, MAX_CONFIGS, &numConfigs))
	{
		WARN("eglGetConfigs() failed");
		return NULL;
	}

	for (int i = 0; i < numConfigs; i++)
	{
		EGLint	value = 0;

		eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &value);
		if (value == configID)
		{
			return configs[i];
		}
	}

	return NULL;
}

#endif

bool GL_ExtensionStringPresent( const char * extension, const char * allExtensions )
{
	if ( strstr( allExtensions, extension ) )
	{
		LOG( "Found: %s", extension );
		return true;
	}
	else
	{
		LOG( "Not found: %s", extension );
		return false;
	}
}

#if defined( ANDROID )
EGLint GL_FlushSync( int timeout )
{
	// if extension not present, return NO_SYNC
	if ( eglCreateSyncKHR_ == NULL )
	{
		return EGL_FALSE;
	}

	EGLDisplay eglDisplay = eglGetCurrentDisplay();

	const EGLSyncKHR sync = eglCreateSyncKHR_( eglDisplay, EGL_SYNC_FENCE_KHR, NULL );
	if ( sync == EGL_NO_SYNC_KHR )
	{
		return EGL_FALSE;
	}

	const EGLint wait = eglClientWaitSyncKHR_( eglDisplay, sync,
							EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, timeout );

	eglDestroySyncKHR_( eglDisplay, sync );

	return wait;
}
#endif // OVR_OS_ANDROID

// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
void GL_InvalidateFramebuffer( const invalidateTarget_t isFBO, const bool colorBuffer, const bool depthBuffer )
{
#if defined( OVR_OS_ANDROID )
	const int offset = (int)!colorBuffer;
	const int count = (int)colorBuffer + ((int)depthBuffer) * 2;

	const GLenum fboAttachments[3] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
	const GLenum attachments[3] = { GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT };
	glInvalidateFramebuffer_( GL_FRAMEBUFFER, count, ( isFBO == INV_FBO ? fboAttachments : attachments ) + offset);
#endif
}

void GL_Finish()
{
#if defined( ANDROID )
	// Given the common driver "optimization" of ignoring glFinish, we
	// can't run reliably while drawing to the front buffer without
	// the Sync extension.
	if ( eglCreateSyncKHR_ != NULL )
	{
		// 100 milliseconds == 100000000 nanoseconds
		const EGLint wait = GL_FlushSync( 100000000 );
		if ( wait == EGL_TIMEOUT_EXPIRED_KHR )
		{
			LOG( "EGL_TIMEOUT_EXPIRED_KHR" );
		}
		if ( wait == EGL_FALSE )
		{
			LOG( "eglClientWaitSyncKHR returned EGL_FALSE" );
		}
	}
#else
	glFinish();
#endif
}

void GL_Flush()
{
#if defined( ANDROID )
	if ( eglCreateSyncKHR_ != NULL )
	{
		const EGLint wait = GL_FlushSync( 0 );
		if ( wait == EGL_FALSE )
		{
			LOG("eglClientWaitSyncKHR returned EGL_FALSE");
		}
	}

	// Also do a glFlush() so it shows up in logging tools that
	// don't capture eglClientWaitSyncKHR_ calls.
//	glFlush();
#else
	glFlush();
#endif
}

const char * GL_GetErrorString()
{
#if defined( ANDROID )
	const EGLint err = eglGetError();
	switch( err )
	{
	case EGL_SUCCESS:			return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:	return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:		return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:			return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:		return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:		return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:		return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:		return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:		return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:			return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:		return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:	return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:	return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:		return "EGL_CONTEXT_LOST";
	default: return "Unknown egl error code";
	}
#else
	GLenum err = glGetError();
	switch( err )
	{
	case GL_NO_ERROR:			return "GL_NO_ERROR";
	case GL_INVALID_ENUM:		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:	return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY:		return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW:	return "GL_STACK_UNDERFLOW";
	case GL_STACK_OVERFLOW:		return "GL_STACK_OVERFLOW";
	default: return "Unknown GL error code";
	}
#endif
}

const char * GL_ErrorForEnum( const GLenum e )
{
	switch( e )
	{
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	default: return "Unknown gl error code";
	}
}

bool GL_CheckErrors( const char * logTitle )
{
	bool hadError = false;

	// There can be multiple errors that need reporting.
	do
	{
		GLenum err = glGetError();
		if ( err == GL_NO_ERROR )
		{
			break;
		}
		hadError = true;
		WARN( "%s GL Error: %s", ( logTitle != nullptr ) ? logTitle : "<untitled>", GL_ErrorForEnum( err ) );
		if ( err == GL_OUT_OF_MEMORY )
		{
			FAIL( "GL_OUT_OF_MEMORY" );
		}
	} while ( 1 );
	return hadError;
}

}	// namespace OVR
