/************************************************************************************

Filename    :   OvrMenuMgr.cpp
Content     :   Menuing system for VR apps.
Created     :   May 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "VRMenuMgr.h"

#include "OVR_GlUtils.h"
#include "App.h"
#include "DebugLines.h"
#include "BitmapFont.h"
#include "VRMenuObject.h"
#include "GuiSys.h"
#include "Kernel/OVR_Lexer.h"

//#define OVR_USE_PERF_TIMER
#include "OVR_PerfTimer.h"

namespace OVR {

// diffuse-only programs
char const* GUIDiffuseOnlyVertexShaderSrc =
	"uniform lowp vec4 UniformColor;\n"
	"uniform lowp vec4 UniformFadeDirection;\n"
	"uniform lowp vec4 UniformUVOffset;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec4 VertexColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = TransformVertex( Position );\n"
	"    oTexCoord = vec2( TexCoord.x + UniformUVOffset.x, TexCoord.y + UniformUVOffset.y );\n"
	"    oColor = UniformColor * VertexColor;\n"
	// Fade out vertices if direction is positive
	"    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )\n"
	"	 {\n"
	"        if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }\n"
	"    }\n"
	"}\n";

char const* GUIDiffuseOnlyFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform highp vec4 ClipUVs;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    if ( oTexCoord.x < ClipUVs.x || oTexCoord.y < ClipUVs.y || oTexCoord.x > ClipUVs.z || oTexCoord.y > ClipUVs.w )\n"
	"    {\n"
	"        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );\n"
	"    }\n"
	"    else\n"
	"    {\n"
	"        gl_FragColor = oColor * texture2D( Texture0, oTexCoord );\n"
	"    }\n"
	"}\n";

char const* GUIDiffuseAlphaDiscardFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform highp vec4 ClipUVs;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    if ( oTexCoord.x < ClipUVs.x || oTexCoord.y < ClipUVs.y || oTexCoord.x > ClipUVs.z || oTexCoord.y > ClipUVs.w )\n"
	"    {\n"
	"        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );\n"
	"    }\n"
	"    else\n"
	"    {\n"
	"        lowp vec4 texColor = texture2D( Texture0, oTexCoord );\n"
	"        if ( texColor.w < 0.01 ) discard;\n"
	"        gl_FragColor = oColor * texColor;\n"
	"    }\n"
	"}\n";

// diffuse color ramped programs
static const char * GUIColorRampFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying lowp vec4 oColor;\n"
	"uniform sampler2D Texture1;\n"
	"uniform mediump vec4 ColorTableOffset;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 texel = texture2D( Texture0, oTexCoord );\n"
	"    lowp vec2 colorIndex = vec2( ColorTableOffset.x + texel.x, ColorTableOffset.y );\n"
	"    lowp vec4 remappedColor = texture2D( Texture1, colorIndex.xy );\n"
	"    gl_FragColor = oColor * vec4( remappedColor.xyz, texel.a );\n"
	"}\n";

// diffuse + color ramped + target programs
char const* GUIDiffuseColorRampTargetVertexShaderSrc =
	"uniform lowp vec4 UniformColor;\n"
	"uniform lowp vec4 UniformFadeDirection;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec2 TexCoord1;\n"
	"attribute vec4 VertexColor;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = TransformVertex( Position );\n"
	"    oTexCoord = TexCoord;\n"
	"    oTexCoord1 = TexCoord1;\n"
	"    oColor = UniformColor * VertexColor;\n"
	// Fade out vertices if direction is positive
	"    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )\n"
	"	 {\n"
	"       if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }\n"
	"    }\n"
	"}\n";

static const char * GUIColorRampTargetFragmentSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"	// color ramp target
	"uniform sampler2D Texture2;\n"	// color ramp
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"uniform mediump vec4 ColorTableOffset;\n"
	"void main()\n"
	"{\n"
	"    mediump vec4 lookup = texture2D( Texture1, oTexCoord1 );\n"
	"    mediump vec2 colorIndex = vec2( ColorTableOffset.x + lookup.x, ColorTableOffset.y );\n"
	"    mediump vec4 remappedColor = texture2D( Texture2, colorIndex.xy );\n"
//	"    gl_FragColor = lookup;\n"
	"    mediump vec4 texel = texture2D( Texture0, oTexCoord );\n"
	"    mediump vec3 blended = ( texel.xyz * ( 1.0 - lookup.a ) ) + ( remappedColor.xyz * lookup.a );\n"
//	"    mediump vec3 blended = ( texel.xyz * ( 1.0 - lookup.a ) ) + ( lookup.xyz * lookup.a );\n"
	"    gl_FragColor = oColor * vec4( blended.xyz, texel.a );\n"
//	"    gl_FragColor = texel;\n"
	"}\n";

// diffuse + additive programs
char const* GUITwoTextureColorModulatedShaderSrc =
	"uniform lowp vec4 UniformColor;\n"
	"uniform lowp vec4 UniformFadeDirection;\n"
	"attribute vec4 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec2 TexCoord1;\n"
	"attribute vec4 VertexColor;\n"
	"attribute vec4 Parms;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = TransformVertex( Position );\n"
	"    oTexCoord = TexCoord;\n"
	"    oTexCoord1 = TexCoord1;\n"
	"    oColor = UniformColor * VertexColor;\n"
	// Fade out vertices if direction is positive
	"    if ( dot(UniformFadeDirection.xyz, UniformFadeDirection.xyz) > 0.0 )\n"
	"	 {\n"
	"        if ( dot(UniformFadeDirection.xyz, Position.xyz ) > 0.0 ) { oColor[3] = 0.0; }\n"
	"    }\n"
	"}\n";

char const* GUIDiffusePlusAdditiveFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 diffuseTexel = texture2D( Texture0, oTexCoord );\n"
	//"   lowp vec4 additiveTexel = texture2D( Texture1, oTexCoord1 ) * oColor;\n"
	"    lowp vec4 additiveTexel = texture2D( Texture1, oTexCoord1 );\n"
	"    lowp vec4 additiveModulated = vec4( additiveTexel.xyz * additiveTexel.a, 0.0 );\n"
	//"    gl_FragColor = min( diffuseTexel + additiveModulated, 1.0 );\n"
	"    gl_FragColor = min( diffuseTexel + additiveModulated, 1.0 ) * oColor;\n"
	"}\n";

// diffuse + diffuse program
// the alpha for the second diffuse is used to composite the color to the first diffuse and
// the alpha of the first diffuse is used to composite to the fragment.
char const* GUIDiffuseCompositeFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 diffuse1Texel = texture2D( Texture0, oTexCoord );\n"
	"    lowp vec4 diffuse2Texel = texture2D( Texture1, oTexCoord1 );\n"
	"    gl_FragColor = vec4( diffuse1Texel.xyz * ( 1.0 - diffuse2Texel.a ) + diffuse2Texel.xyz * diffuse2Texel.a, diffuse1Texel.a ) * oColor;\n"
	"}\n";

// alpha map + diffuse program
// the alpha channel of first texture is to composite to the fragment
char const* GUIAlphaDiffuseFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"    lowp vec4 diffuse1Texel = texture2D( Texture0, oTexCoord );\n"
	"    lowp vec4 diffuse2Texel = texture2D( Texture1, oTexCoord1 );\n"
	"    gl_FragColor = vec4( diffuse2Texel.xyz, diffuse1Texel.a ) * oColor;\n"
	"}\n";


//==================================
// ComposeHandle
menuHandle_t ComposeHandle( int const index, UInt32 const id )
{
	UInt64 handle = ( ( (UInt64)id ) << 32ULL ) | (UInt64)index;
	return menuHandle_t( handle );
}

//==================================
// DecomposeHandle
void DecomposeHandle( menuHandle_t const handle, int & index, UInt32 & id )
{
	index = (int)( handle.Get() & 0xFFFFFFFF );
	id = (UInt32)( handle.Get() >> 32ULL );
}

//==================================
// HandleComponentsAreValid
static bool HandleComponentsAreValid( int const index, UInt32 const id )
{
	if ( id == INVALID_MENU_OBJECT_ID )
	{
		return false;
	}
	if ( index < 0 )
	{
		return false;
	}
	return true;
}

//==============================================================
// SurfSort
class SurfSort
{
public:
	int64_t		Key;

	bool operator < ( SurfSort const & other ) const
	{
		return Key - other.Key > 0;	// inverted because we want to render furthest-to-closest
	}
};

//==============================================================
// VRMenuMgrLocal
class VRMenuMgrLocal : public OvrVRMenuMgr
{
public:
	static int const	MAX_SUBMITTED	= 256;

								VRMenuMgrLocal( OvrGuiSys & guiSys );
	virtual						~VRMenuMgrLocal();

	// Initialize the VRMenu system
	virtual void				Init( OvrGuiSys & guiSys );
	// Shutdown the VRMenu syatem
	virtual void				Shutdown();

	// creates a new menu object
	virtual menuHandle_t		CreateObject( VRMenuObjectParms const & parms );
	// Frees a menu object.  If the object is a child of a parent object, this will
	// also remove the child from the parent.
	virtual void				FreeObject( menuHandle_t const handle );
	// Returns true if the handle is valid.
	virtual bool				IsValid( menuHandle_t const handle ) const;
	// Return the object for a menu handle or NULL if the object does not exist or the
	// handle is invalid;
	virtual VRMenuObject	*	ToObject( menuHandle_t const handle ) const;

	// Submits the specified menu object to be renderered
	virtual void				SubmitForRendering( OvrGuiSys & guiSys, Matrix4f const & centerViewMatrix,
										menuHandle_t const handle, Posef const & worldPose,
										VRMenuRenderFlags_t const & flags );

	// Call once per frame before rendering to sort surfaces.
	virtual void				Finish( Matrix4f const & viewMatrix );

	virtual void 				AppendSurfaceList( Matrix4f const & centerViewMatrix,
										Array< ovrDrawSurface > & surfaceList ) const;

    virtual GlProgram const *   GetGUIGlProgram( eGUIProgramType const programType ) const;

	static VRMenuMgrLocal &		ToLocal( OvrVRMenuMgr & menuMgr ) { return *(VRMenuMgrLocal*)&menuMgr; }

private:
	//--------------------------------------------------------------
	// private methods
	//--------------------------------------------------------------

	// make a private assignment operator to prevent warning C4512: assignment operator could not be generated
	VRMenuMgrLocal &			operator=( const VRMenuMgrLocal & );
	
	void						AddComponentToDeletionList( menuHandle_t const ownerHandle, VRMenuComponent * component );
	void						ExecutePendingComponentDeletions();

	void						CondenseList();
	void						SubmitForRenderingRecursive( OvrGuiSys & guiSys, Matrix4f const & centerViewMatrix,
										VRMenuRenderFlags_t const & flags, VRMenuObject const * obj,
										Posef const & parentModelPose, Vector4f const & parentColor,
										Vector3f const & parentScale, Bounds3f & cullBounds,
                                        SubmittedMenuObject * submitted, int const maxIndices, int & curIndex,
										int const distanceIndex ) const;

	//--------------------------------------------------------------
	// private members
	//--------------------------------------------------------------
	OvrGuiSys &				GuiSys;			// reference to the GUI sys that owns this menu manager
	UInt32					CurrentId;		// ever-incrementing object ID (well... up to 4 billion or so :)
	Array< VRMenuObject* >	ObjectList;		// list of all menu objects
	Array< int >			FreeList;		// list of free slots in the array
	
	Array< ovrComponentList >	PendingDeletions;	// list of components (and owning objects) that are pending deletion

	bool					Initialized;	// true if Init has been called

	SubmittedMenuObject		Submitted[MAX_SUBMITTED];	// all objects that have been submitted for rendering on the current frame
	Array< SurfSort >		SortKeys;					// sort key consisting of distance from view and submission index
	int						NumSubmitted;				// number of currently submitted menu objects
	mutable int				NumToRender;				// number of submitted objects to render

	GlProgram		        GUIProgramDiffuseOnly;					// has a diffuse only
	GlProgram				GUIProgramDiffuseAlphaDiscard;			// diffuse, but discard fragments with 0 alpha
	GlProgram		        GUIProgramDiffusePlusAdditive;			// has a diffuse and an additive
	GlProgram				GUIProgramDiffuseComposite;				// has a two diffuse maps
	GlProgram		        GUIProgramDiffuseColorRamp;				// has a diffuse and color ramp, and color ramp target is the diffuse
	GlProgram		        GUIProgramDiffuseColorRampTarget;		// has diffuse, color ramp, and a separate color ramp target
	GlProgram				GUIProgramAlphaDiffuse;					// alpha map + diffuse map

	static bool				ShowCollision;		// show collision bounds only
	static bool				ShowDebugBounds;	// true to show the menu items' debug bounds. This is static so that the console command will turn on bounds for all activities.
	static bool				ShowDebugHierarchy;	// true to show the menu items' hierarchy. This is static so that the console command will turn on bounds for all activities.
	static bool				ShowDebugNormals;
	static bool				ShowPoses;
	static bool				ShowStats;			// show stats like number of draw calls
	static bool				ShowWrapWidths;

	static void				DebugCollision( void * appPtr, const char * cmdLine );
	static void				DebugMenuBounds( void * appPtr, const char * cmdLine );
	static void				DebugMenuHierarchy( void * appPtr, const char * cmdLine );
	static void				DebugMenuPoses( void * appPtr, const char * cmdLine );
	static void				DebugShowStats( void * appPtr, const char * cmdLine );
	static void				DebugWordWrap( void * appPtr, const char * cmdLine );
};

bool VRMenuMgrLocal::ShowCollision = false;
bool VRMenuMgrLocal::ShowDebugBounds = false;
bool VRMenuMgrLocal::ShowDebugHierarchy = false;
bool VRMenuMgrLocal::ShowDebugNormals = false;
bool VRMenuMgrLocal::ShowPoses = false;
bool VRMenuMgrLocal::ShowStats = false;
bool VRMenuMgrLocal::ShowWrapWidths = false;

void VRMenuMgrLocal::DebugCollision( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowCollision = show != 0;
	LOG( "DebugCollision( '%s' ): show = %i", parms, show );
}

void VRMenuMgrLocal::DebugMenuBounds( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowDebugBounds = show != 0;
	LOG( "DebugMenuBounds( '%s' ): show = %i", parms, show );
}

void VRMenuMgrLocal::DebugMenuHierarchy( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowDebugHierarchy = show != 0;
	LOG( "DebugMenuHierarchy( '%s' ): show = %i", parms, show );
}

void VRMenuMgrLocal::DebugMenuPoses( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowPoses = show != 0;
	LOG( "DebugMenuPoses( '%s' ): show = %i", parms, show );
}

void VRMenuMgrLocal::DebugShowStats( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowStats = show != 0;
	LOG( "ShowStats( '%s' ): show = %i", parms, show );
}

void VRMenuMgrLocal::DebugWordWrap( void * appPtr, const char * parms )
{
	ovrLexer lex( parms );
	int show;
	lex.ParseInt( show, 0 );
	ShowWrapWidths = show != 0;
	LOG( "ShowWrapWidths( '%s' ): show = %i", parms, show );
}

//==================================
// VRMenuMgrLocal::VRMenuMgrLocal
VRMenuMgrLocal::VRMenuMgrLocal( OvrGuiSys & guiSys )
	: GuiSys( guiSys )
	, CurrentId( 0 )
	, Initialized( false )
	, NumSubmitted( 0 )
	, NumToRender( 0 )
{
}

//==================================
// VRMenuMgrLocal::~VRMenuMgrLocal
VRMenuMgrLocal::~VRMenuMgrLocal()
{
}

//==================================
// VRMenuMgrLocal::Init
//
// Initialize the VRMenu system
void VRMenuMgrLocal::Init( OvrGuiSys & guiSys )
{
	LOG( "VRMenuMgrLocal::Init" );
	if ( Initialized )
	{
        return;
	}

	// diffuse only
	if ( GUIProgramDiffuseOnly.VertexShader == 0 || GUIProgramDiffuseOnly.FragmentShader == 0 )
	{
		GUIProgramDiffuseOnly = BuildProgram( GUIDiffuseOnlyVertexShaderSrc, GUIDiffuseOnlyFragmentShaderSrc );
	}
	// diffuse alpha discard only
	if ( GUIProgramDiffuseAlphaDiscard.VertexShader == 0 || GUIProgramDiffuseAlphaDiscard.FragmentShader == 0 )
	{
		GUIProgramDiffuseAlphaDiscard = BuildProgram( GUIDiffuseOnlyVertexShaderSrc, GUIDiffuseAlphaDiscardFragmentShaderSrc );
	}	
	// diffuse + additive
	if ( GUIProgramDiffusePlusAdditive.VertexShader == 0 || GUIProgramDiffusePlusAdditive.FragmentShader == 0 )
	{
		GUIProgramDiffusePlusAdditive = BuildProgram( GUITwoTextureColorModulatedShaderSrc, GUIDiffusePlusAdditiveFragmentShaderSrc );
	}
	// diffuse + diffuse
	if ( GUIProgramDiffuseComposite.VertexShader == 0 || GUIProgramDiffuseComposite.FragmentShader == 0 )
	{
		GUIProgramDiffuseComposite = BuildProgram( GUITwoTextureColorModulatedShaderSrc, GUIDiffuseCompositeFragmentShaderSrc );
	}
	// diffuse color ramped
	if ( GUIProgramDiffuseColorRamp.VertexShader == 0 || GUIProgramDiffuseColorRamp.FragmentShader == 0 )
	{
		GUIProgramDiffuseColorRamp = BuildProgram( GUIDiffuseOnlyVertexShaderSrc, GUIColorRampFragmentSrc );
	}
	// diffuse, color ramp, and a specific target for the color ramp
	if ( GUIProgramDiffuseColorRampTarget.VertexShader == 0 || GUIProgramDiffuseColorRampTarget.FragmentShader == 0 )
	{
		GUIProgramDiffuseColorRampTarget = BuildProgram( GUIDiffuseColorRampTargetVertexShaderSrc, GUIColorRampTargetFragmentSrc );
	}
	if ( GUIProgramAlphaDiffuse.VertexShader == 0 || GUIProgramAlphaDiffuse.FragmentShader == 0 )
	{
		GUIProgramAlphaDiffuse = BuildProgram( GUITwoTextureColorModulatedShaderSrc, GUIAlphaDiffuseFragmentShaderSrc );
	}

	guiSys.GetApp()->RegisterConsoleFunction( "debugMenuBounds", DebugMenuBounds );
	guiSys.GetApp()->RegisterConsoleFunction( "debugMenuHierarchy", DebugMenuHierarchy );
	guiSys.GetApp()->RegisterConsoleFunction( "debugMenuPoses", DebugMenuPoses );
	guiSys.GetApp()->RegisterConsoleFunction( "debugShowStats", DebugShowStats );
	guiSys.GetApp()->RegisterConsoleFunction( "debugWordWrap", DebugWordWrap );

	Initialized = true;
}

//==================================
// VRMenuMgrLocal::Shutdown
//
// Shutdown the VRMenu syatem
void VRMenuMgrLocal::Shutdown()
{
	if ( !Initialized )
	{
        return;
	}

	DeleteProgram( GUIProgramDiffuseOnly );
	DeleteProgram( GUIProgramDiffuseAlphaDiscard );
	DeleteProgram( GUIProgramDiffusePlusAdditive );
	DeleteProgram( GUIProgramDiffuseComposite );
	DeleteProgram( GUIProgramDiffuseColorRamp );
	DeleteProgram( GUIProgramDiffuseColorRampTarget );
	DeleteProgram( GUIProgramAlphaDiffuse );

    Initialized = false;
}

//==================================
// VRMenuMgrLocal::CreateObject
// creates a new menu object
menuHandle_t VRMenuMgrLocal::CreateObject( VRMenuObjectParms const & parms )
{
	OVR_PERF_TIMER( CreatObject );

	if ( !Initialized )
	{
		WARN( "VRMenuMgrLocal::CreateObject - manager has not been initialized!" );
		return menuHandle_t();
	}

	// validate parameters
	if ( parms.Type >= VRMENU_MAX )
	{
		WARN( "VRMenuMgrLocal::CreateObject - Invalid menu object type: %i", parms.Type );
		return menuHandle_t();
	}

	// create the handle first so we can enforce setting it be requiring it to be passed to the constructor
	int index = -1;
	if ( FreeList.GetSizeI() > 0 )
	{
		index = FreeList.Back();
		FreeList.PopBack();
	}
	else
	{
		index = ObjectList.GetSizeI();
	}

	UInt32 id = ++CurrentId;
	menuHandle_t handle = ComposeHandle( index, id );
	//LOG( "VRMenuMgrLocal::CreateObject - handle is %llu", handle.Get() );

	VRMenuObject * obj = new VRMenuObject( parms, handle );
	if ( obj == NULL )
	{
		WARN( "VRMenuMgrLocal::CreateObject - failed to allocate menu object!" );
		OVR_ASSERT( obj != NULL );	// this would be bad -- but we're likely just going to explode elsewhere
		return menuHandle_t();
	}

	obj->Init( GuiSys, parms );

	if ( index == ObjectList.GetSizeI() )
	{
		// we have to grow the array
		ObjectList.PushBack( obj );
	}
	else
	{
		// insert in existing slot
		OVR_ASSERT( ObjectList[index] == NULL );
		ObjectList[index ] = obj;
	}

	return handle;
}

//==================================
// VRMenuMgrLocal::FreeObject
// Frees a menu object.  If the object is a child of a parent object, this will
// also remove the child from the parent.
void VRMenuMgrLocal::FreeObject( menuHandle_t const handle )
{
	int index;
	UInt32 id;
	DecomposeHandle( handle, index, id );
	if ( !HandleComponentsAreValid( index, id ) )
	{
		return;
	}
	if ( ObjectList[index] == NULL )
	{
		// already freed
		return;
	}

	VRMenuObject * obj = ObjectList[index];
	// remove this object from its parent's child list
	if ( obj->GetParentHandle().IsValid() )
	{
		VRMenuObject * parentObj = ToObject( obj->GetParentHandle() );
		if ( parentObj != NULL )
		{
			parentObj->RemoveChild( *this, handle );
		}
	}

    // free all of this object's children
    obj->FreeChildren( *this );

	delete obj;

	// empty the slot
	ObjectList[index] = NULL;
	// add the index to the free list
	FreeList.PushBack( index );

	CondenseList();
}

//==================================
// VRMenuMgrLocal::CondenseList
// keeps the free list from growing too large when items are removed
void VRMenuMgrLocal::CondenseList()
{
	// we can only condense the array if we have a significant number of items at the end of the array buffer
	// that are empty (because we cannot move an existing object around without changing its handle, too, which
	// would invalidate any existing references to it).
	// This is the difference between the current size and the array capacity.
	int const MIN_FREE = 64;	// very arbitray number
	if ( ObjectList.GetCapacityI() - ObjectList.GetSizeI() < MIN_FREE )
	{
		return;
	}

	// shrink to current size
	ObjectList.Resize( ObjectList.GetSizeI() );

	// create a new free list of just indices < the new size
	Array< int > newFreeList;
	for ( int i = 0; i < FreeList.GetSizeI(); ++i )
	{
		if ( FreeList[i] <= ObjectList.GetSizeI() )
		{
			newFreeList.PushBack( FreeList[i] );
		}
	}
	FreeList = newFreeList;
}

//==================================
// VRMenuMgrLocal::IsValid
bool VRMenuMgrLocal::IsValid( menuHandle_t const handle ) const
{
	int index;
	UInt32 id;
	DecomposeHandle( handle, index, id );
	return HandleComponentsAreValid( index, id );
}

//==================================
// VRMenuMgrLocal::ToObject
// Return the object for a menu handle.
VRMenuObject * VRMenuMgrLocal::ToObject( menuHandle_t const handle ) const
{
	int index;
	UInt32 id;
	DecomposeHandle( handle, index, id );
	if ( id == INVALID_MENU_OBJECT_ID )
	{
		return NULL;
	}
	if ( !HandleComponentsAreValid( index, id ) )
	{
		WARN( "VRMenuMgrLocal::ToObject - invalid handle." );
		return NULL;
	}
	if ( index >= ObjectList.GetSizeI() )
	{
		WARN( "VRMenuMgrLocal::ToObject - index out of range." );
		return NULL;
	}
	VRMenuObject * object = ObjectList[index];
	if ( object == NULL )
	{
		WARN( "VRMenuMgrLocal::ToObject - slot empty." );
		return NULL;	// this can happen if someone is holding onto the handle of an object that's been freed
	}
	if ( object->GetHandle() != handle )
	{
		// if the handle of the object in the slot does not match, then the object the handle refers to was deleted
		// and a new object is in the slot
		WARN( "VRMenuMgrLocal::ToObject - slot mismatch." );
		return NULL;
	}
	return object;
}

/*
static void LogBounds( const char * name, char const * prefix, Bounds3f const & bounds )
{
	LOG_WITH_TAG( "Spam", "'%s' %s: min( %.2f, %.2f, %.2f ) - max( %.2f, %.2f, %.2f )",
		name, prefix,
		bounds.GetMins().x, bounds.GetMins().y, bounds.GetMins().z,
		bounds.GetMaxs().x, bounds.GetMaxs().y, bounds.GetMaxs().z );
}
*/

OVR_PERF_ACCUMULATOR( SubmitForRenderingRecursive_DrawText3D );
OVR_PERF_ACCUMULATOR( SubmitForRenderingRecursive_submit );

//==============================
// VRMenuMgrLocal::SubmitForRenderingRecursive
void VRMenuMgrLocal::SubmitForRenderingRecursive( OvrGuiSys & guiSys, Matrix4f const & centerViewMatrix,
		VRMenuRenderFlags_t const & flags, VRMenuObject const * obj, Posef const & parentModelPose,
		Vector4f const & parentColor, Vector3f const & parentScale, Bounds3f & cullBounds,
		SubmittedMenuObject * submitted, int const maxIndices, int & curIndex, int const distanceIndex ) const
{
	if ( curIndex >= maxIndices )
	{
		// If this happens we're probably not correctly clearing the submitted surfaces each frame
		// OR we've got a LOT of surfaces.
		LOG( "maxIndices = %i, curIndex = %i", maxIndices, curIndex );
		ASSERT_WITH_TAG( curIndex < maxIndices, "VrMenu" );
		return;
	}

	// check if this object is hidden
	VRMenuObjectFlags_t const oFlags = obj->GetFlags();
	if ( oFlags & VRMENUOBJECT_DONT_RENDER )
	{
		return;
	}

	Posef const & localPose = obj->GetLocalPose();
	Vector3f const & localScale = obj->GetLocalScale();
	Vector4f const & localColor = obj->GetColor();
	Posef curModelPose;
	Vector4f curColor;
	Vector3f scale;

	VRMenuObject::TransformByParent( parentModelPose, parentScale, parentColor, localPose, localScale, localColor, oFlags, curModelPose, scale, curColor );

	OVR_ASSERT( obj != NULL );

	cullBounds = obj->GetLocalBounds( guiSys.GetDefaultFont() ) * parentScale;

	int submissionIndex = -1;
	if ( obj->GetType() != VRMENU_CONTAINER )	// containers never render, but their children may
	{
        Posef const & hilightPose = obj->GetHilightPose();
        Posef itemPose( curModelPose.Rotation * hilightPose.Rotation,
                        curModelPose.Translation + ( curModelPose.Rotation * parentScale.EntrywiseMultiply( hilightPose.Translation ) ) );
		Matrix4f poseMat( itemPose.Rotation );
		curModelPose = itemPose;	// so children like the slider bar caret use our hilight offset and don't end up clipping behind us!
		VRMenuRenderFlags_t rFlags = flags;
		if ( oFlags & VRMENUOBJECT_FLAG_POLYGON_OFFSET )
		{
			rFlags |= VRMENU_RENDER_POLYGON_OFFSET;
		}
		if ( oFlags & VRMENUOBJECT_FLAG_NO_DEPTH )
		{
			rFlags |= VRMENU_RENDER_NO_DEPTH;
		}
		if ( oFlags & VRMENUOBJECT_FLAG_NO_DEPTH_MASK )
		{
			rFlags |= VRMENU_RENDER_NO_DEPTH_MASK;
		}
		if ( oFlags & VRMENUOBJECT_INSTANCE_TEXT )
		{
			rFlags |= VRMENU_RENDER_SUBMIT_TEXT_SURFACE;
		}

		if ( oFlags & VRMENUOBJECT_FLAG_BILLBOARD )
		{
			Matrix4f invViewMatrix = centerViewMatrix.Transposed();
			itemPose.Rotation = Quatf( invViewMatrix );
		}

		if ( ShowPoses )
		{
			Matrix4f const itemPoseMat( itemPose );
			guiSys.GetDebugLines().AddLine( itemPose.Translation, itemPose.Translation + itemPoseMat.GetXBasis() * 0.05f,
					Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), 0, false );
			guiSys.GetDebugLines().AddLine( itemPose.Translation, itemPose.Translation + itemPoseMat.GetYBasis() * 0.05f,
					Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), 0, false );
			guiSys.GetDebugLines().AddLine( itemPose.Translation, itemPose.Translation + itemPoseMat.GetZBasis() * 0.05f,
					Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), 0, false );
		}

		if ( ( oFlags & VRMENUOBJECT_DONT_RENDER_SURFACE ) == 0 )
		{
			// the menu object may have zero or more renderable surfaces (if 0, it may draw only text)
			OVR_PERF_ACCUMULATE( SubmitForRenderingRecursive_submit );
			submissionIndex = curIndex;
			Array< VRMenuSurface > const & surfaces = obj->GetSurfaces();
			for ( int i = 0; i < surfaces.GetSizeI(); ++i )
			{
				VRMenuSurface const & surf = surfaces[i];
				if ( surf.IsRenderable() )
				{
					SubmittedMenuObject & sub = submitted[curIndex];
					sub.SurfaceIndex = i;
					sub.DistanceIndex = distanceIndex >= 0 ? distanceIndex : curIndex;
					sub.Pose = itemPose;
					sub.Scale = scale;
					sub.Flags = rFlags;
					sub.ColorTableOffset = obj->GetColorTableOffset();
					sub.SkipAdditivePass = !obj->IsHilighted();
					sub.Handle = obj->GetHandle();
					// modulate surface color with parent's current color
					sub.Color = surf.GetColor() * curColor;
					sub.Offsets = surf.GetAnchorOffsets();
					sub.FadeDirection = obj->GetFadeDirection();
					sub.ClipUVs = surf.GetClipUVs();
					sub.OffsetUVs = surf.GetOffsetUVs();
#if defined( OVR_BUILD_DEBUG )
					sub.SurfaceName = surf.GetName();
#endif
					sub.LocalBounds = cullBounds;
					curIndex++;
				}
			}
			OVR_PERF_TIMER_STOP( SubmitForRenderingRecursive_submit );
		}		

		OVR::String const & text = obj->GetText();
		if ( ( oFlags & VRMENUOBJECT_DONT_RENDER_TEXT ) == 0 && text.GetLengthI() > 0 )
		{
            Posef const & textLocalPose = obj->GetTextLocalPose();
            Posef curTextPose;
            curTextPose.Translation = itemPose.Translation + ( itemPose.Rotation * textLocalPose.Translation * scale );
            curTextPose.Rotation = itemPose.Rotation * textLocalPose.Rotation;

			Matrix4f textMat( curTextPose );
			Vector3f textUp = textMat.GetYBasis();
            Vector3f textNormal = textMat.GetZBasis();
			Vector3f position = curTextPose.Translation + textNormal * 0.001f; // this is simply to prevent z-fighting right now
            Vector3f textScale = scale * obj->GetTextLocalScale() * obj->GetWrapScale();

            Vector4f textColor = obj->GetTextColor();
            // Apply parent's alpha influence
            textColor.w *= parentColor.w;
			VRMenuFontParms const & fp = obj->GetFontParms();
			fontParms_t fontParms;
			fontParms.AlignHoriz = fp.AlignHoriz;
			fontParms.AlignVert = fp.AlignVert;
			fontParms.Billboard = fp.Billboard;
			fontParms.TrackRoll = fp.TrackRoll;
			fontParms.ColorCenter = fp.ColorCenter;
			fontParms.AlphaCenter = fp.AlphaCenter;

			// We could re-create the text surface here every frame to handle all cases where any font parameters 
			// change, but that would be needlessly expensive.
			if ( rFlags & VRMENU_RENDER_SUBMIT_TEXT_SURFACE )
			{
				OVR_ASSERT( obj->TextSurface != nullptr );
				Matrix4f scaleMatrix;
				scaleMatrix.M[0][0] = parentScale.x;
				scaleMatrix.M[1][1] = parentScale.y;
				scaleMatrix.M[2][2] = parentScale.z;
				obj->TextSurface->ModelMatrix = scaleMatrix * Matrix4f( curTextPose.Rotation );
				obj->TextSurface->ModelMatrix.SetTranslation( curTextPose.Translation );

				// if we didn't submit anything but we have an instanced text surface, submit an invalid surface so that
				// the text surface will be added to the surface list in BuildDrawSurface
				if ( curIndex - submissionIndex == 0 )
				{
					SubmittedMenuObject & sub = submitted[curIndex];
					sub.SurfaceIndex = -1;
					sub.Pose = itemPose;
					sub.Scale = scale;
					sub.Flags = rFlags;
					sub.Handle = obj->GetHandle();
					sub.Color = parentColor;
					sub.LocalBounds = cullBounds;
					curIndex++;
				}
			}
			else
			{
				OVR_PERF_ACCUMULATE( SubmitForRenderingRecursive_DrawText3D );
				guiSys.GetDefaultFontSurface().DrawText3D( guiSys.GetDefaultFont(), fontParms, 
						position, textNormal, textUp, textScale.x * fp.Scale, textColor, text.ToCStr() );
			}

			if ( ShowWrapWidths )
			{
				// this shows a ruler for the wrap width when rendering text
				Vector3f yofs( 0.0f, 0.05f, 0.0f );
				Vector3f xofs( fp.WrapWidth * 0.5f, 0.0f, 0.0f );

				guiSys.GetDebugLines().AddLine( position - xofs - yofs, position - xofs + yofs,
					Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), 0, false );

				guiSys.GetDebugLines().AddLine( position + xofs - yofs, position + xofs + yofs,
					Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), 0, false );

				guiSys.GetDebugLines().AddLine( position - xofs, position + xofs,
					Vector4f( 0.0f, 1.0f, 0.0f, 1.0f ), Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), 0, false );
			}
		}
        //LOG_WITH_TAG( "Spam", "AddPoint for '%s'", text.ToCStr() );
		//GetDebugLines().AddPoint( curModelPose.Position, 0.05f, 1, true );
	}

	// submit all children
    if ( obj->Children.GetSizeI() > 0 )
    {
		// If this object has the render hierarchy order flag, then it and all its children should
		// be depth sorted based on this object's distance + the inverse of the submission index.
		// (inverted because we want a higher submission index to render after a lower submission index)
		int di = distanceIndex;
		if ( di < 0 && ( oFlags & VRMenuObjectFlags_t( VRMENUOBJECT_RENDER_HIERARCHY_ORDER ) ) )
		{
			di = submissionIndex;
		}
	    for ( int i = 0; i < obj->Children.GetSizeI(); ++i )
	    {
		    menuHandle_t childHandle = obj->Children[i];
		    VRMenuObject const * child = static_cast< VRMenuObject const * >( ToObject( childHandle ) );
		    if ( child == NULL )
		    {
			    continue;
		    }

            Bounds3f childCullBounds;
		    SubmitForRenderingRecursive( guiSys, centerViewMatrix, flags, child, curModelPose,
                    curColor, scale, childCullBounds, submitted, maxIndices, curIndex, di );

		    Posef pose = child->GetLocalPose();
		    pose.Translation = pose.Translation * scale;
            childCullBounds = Bounds3f::Transform( pose, childCullBounds );
            cullBounds = Bounds3f::Union( cullBounds, childCullBounds );
	    }
    }

    obj->SetCullBounds( cullBounds );

	//VRMenuId_t debugId( 297 );
	if ( ShowCollision )
	{

		OvrCollisionPrimitive const * cp = obj->GetCollisionPrimitive();
		if ( cp != NULL )
		{
			cp->DebugRender( guiSys.GetDebugLines(), curModelPose, scale, ShowDebugNormals );
		}
		//if ( obj->GetId() == debugId )
		{
			Array< VRMenuSurface > const & surfaces = obj->GetSurfaces();
			for ( int i = 0; i < surfaces.GetSizeI(); ++i )
			{
				VRMenuSurface const & surf = surfaces[i];
				surf.GetTris().DebugRender( guiSys.GetDebugLines(), curModelPose, scale, ShowDebugNormals );
			}
		}
	}
	if ( ShowDebugBounds )//&& obj->GetId() == debugId )
	{

		{
			// for debug drawing, put the cull bounds in world space
			//LogBounds( obj->GetText().ToCStr(), "Transformed CullBounds", myCullBounds );
			guiSys.GetDebugLines().AddBounds( curModelPose, cullBounds, Vector4f( 0.0f, 1.0f, 1.0f, 1.0f ) );
		}
		{
			Bounds3f localBounds = obj->GetLocalBounds( guiSys.GetDefaultFont() ) * parentScale;
			//LogBounds( obj->GetText().ToCStr(), "localBounds", localBounds );
    		guiSys.GetDebugLines().AddBounds( curModelPose, localBounds, Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ) );
			Bounds3f textLocalBounds = obj->GetTextLocalBounds( guiSys.GetDefaultFont() );
			Posef hilightPose = obj->GetHilightPose();
			textLocalBounds = Bounds3f::Transform( Posef( hilightPose.Rotation, hilightPose.Translation * scale ), textLocalBounds );
    		guiSys.GetDebugLines().AddBounds( curModelPose, textLocalBounds * parentScale, Vector4f( 1.0f, 1.0f, 0.0f, 1.0f ) );
		}
	}

	// draw the hierarchy
	if ( ShowDebugHierarchy )
	{
		fontParms_t fp;
		fp.AlignHoriz = HORIZONTAL_CENTER;
		fp.AlignVert = VERTICAL_CENTER;
		fp.Billboard = true;
#if 0
		VRMenuObject const * parent = ToObject( obj->GetParentHandle() );
		if ( parent != NULL )
		{
			Vector3f itemUp = curModelPose.Rotation * Vector3f( 0.0f, 1.0f, 0.0f );
			Vector3f itemNormal = curModelPose.Rotation * Vector3f( 0.0f, 0.0f, 1.0f );
			fontSurface.DrawTextBillboarded3D( font, fp, curModelPose.Translation, itemNormal, itemUp,
					0.5f, Vector4f( 1.0f, 0.0f, 1.0f, 1.0f ), obj->GetSurfaces()[0] ); //parent->GetText().ToCStr() );
		}
#endif
		guiSys.GetDebugLines().AddLine( parentModelPose.Translation, curModelPose.Translation, Vector4f( 1.0f, 0.0f, 0.0f, 1.0f ), Vector4f( 0.0f, 0.0f, 1.0f, 1.0f ), 5, false );
		if ( obj->GetSurfaces().GetSizeI() > 0 )
		{
			guiSys.GetDefaultFontSurface().DrawTextBillboarded3D( guiSys.GetDefaultFont(), fp,
					curModelPose.Translation, 0.5f, Vector4f( 0.8f, 0.8f, 0.8f, 1.0f ),
					obj->GetSurfaces()[0].GetName().ToCStr() );
		}
	}
}

//==============================
// VRMenuMgrLocal::SubmitForRendering
// Submits the specified menu object and it's children
void VRMenuMgrLocal::SubmitForRendering( OvrGuiSys & guiSys, Matrix4f const & centerViewMatrix,
		menuHandle_t const handle, Posef const & worldPose, VRMenuRenderFlags_t const & flags )
{
	//LOG( "VRMenuMgrLocal::SubmitForRendering" );
	if ( NumSubmitted >= MAX_SUBMITTED )
	{
		WARN( "Too many menu objects submitted!" );
		return;
	}
	VRMenuObject * obj = static_cast< VRMenuObject* >( ToObject( handle ) );
	if ( obj == NULL )
	{
		return;
	}

    Bounds3f cullBounds;
	SubmitForRenderingRecursive( guiSys, centerViewMatrix, flags, obj, worldPose, Vector4f( 1.0f ),
			Vector3f( 1.0f ), cullBounds, Submitted, MAX_SUBMITTED, NumSubmitted, -1 );

	OVR_PERF_REPORT( SubmitForRenderingRecursive_submit );
	OVR_PERF_REPORT( SubmitForRenderingRecursive_DrawText3D );
}

//==============================
// VRMenuMgrLocal::Finish
void VRMenuMgrLocal::Finish( Matrix4f const & viewMatrix )
{
	// free any deleted component objects
	ExecutePendingComponentDeletions();

	if ( NumSubmitted == 0 )
	{
		NumToRender = 0;
		return;
	}

	Matrix4f invViewMatrix = viewMatrix.Inverted(); // if the view is never scaled or sheared we could use Transposed() here instead
	Vector3f viewPos = invViewMatrix.GetTranslation();

	// sort surfaces
	SortKeys.Resize( NumSubmitted );
	for ( int i = 0; i < NumSubmitted; ++i )
	{
		// The sort key is a combination of the distance squared, reinterpreted as an integer, and the submission index.
		// This sorts on distance while still allowing submission order to contribute in the equal case.
		// The DistanceIndex is used to force a submitted object to use some other object's distance instead of its own,
		// allowing a group of objects to sort against all other object's based on a single distance. Objects uising the
		// same DistanceIndex will then be sorted against each other based only on their submission index.
		float const distSq = ( Submitted[Submitted[i].DistanceIndex].Pose.Translation - viewPos ).LengthSq();
		int64_t sortKey = *reinterpret_cast< unsigned const* >( &distSq );
		SortKeys[i].Key = ( sortKey << 32ULL ) | ( NumSubmitted - i );	// invert i because we want items submitted sooner to be considered "further away"
	}

	Alg::QuickSort( SortKeys );

	NumToRender = NumSubmitted;
	NumSubmitted = 0;
}

//==============================
// VRMenuMgrLocal::AppendSurfaceList
void VRMenuMgrLocal::AppendSurfaceList( Matrix4f const & centerViewMatrix, Array< ovrDrawSurface > & surfaceList ) const
{
	if ( NumToRender == 0 )
	{
		return;
	}

	for ( int i = 0; i < NumToRender; ++i )
	{
		int idx = abs( static_cast<int>( SortKeys[i].Key & 0xFFFFFFFF ) - NumToRender );
		SubmittedMenuObject const & cur = Submitted[idx];

		VRMenuObject const * obj = static_cast< VRMenuObject const * >( ToObject( cur.Handle ) );
		if ( obj != NULL )
		{
			Vector3f translation( cur.Pose.Translation.x + cur.Offsets.x, cur.Pose.Translation.y + cur.Offsets.y, cur.Pose.Translation.z );

			Matrix4f transform( cur.Pose.Rotation );
			if ( cur.Flags & VRMENU_RENDER_BILLBOARD )
			{
				Matrix4f invViewMatrix = centerViewMatrix.Inverted();
				Vector3f viewPos = invViewMatrix.GetTranslation();

				Vector3f normal = viewPos - cur.Pose.Translation;
				Vector3f up( 0.0f, 1.0f, 0.0f );
				float length = normal.Length();
				if ( length > MATH_FLOAT_SMALLEST_NON_DENORMAL )
				{
					normal.Normalize();
					if ( normal.Dot( up ) > MATH_FLOAT_SMALLEST_NON_DENORMAL )
					{
						transform = Matrix4f::CreateFromBasisVectors( normal, Vector3f( 0.0f, 1.0f, 0.0f ) );
					}
				}
			}

			Matrix4f scaleMatrix;
			scaleMatrix.M[0][0] = cur.Scale.x;
			scaleMatrix.M[1][1] = cur.Scale.y;
			scaleMatrix.M[2][2] = cur.Scale.z;

			transform *= scaleMatrix;
			transform.SetTranslation( translation );

			// TODO: do we need to use SubmittedMenuObject at all now that we can use
			// ovrSurfaceDef? We still need to sort for now but ideally SurfaceRenderer
			// would sort all surfaces before rendering.

			obj->BuildDrawSurface( *this,
					transform,
					cur.SurfaceName.ToCStr(),
					cur.SurfaceIndex,
					cur.Color,
					cur.FadeDirection,
					cur.ColorTableOffset,
					cur.ClipUVs,
					cur.OffsetUVs,
					cur.SkipAdditivePass,
					cur.Flags,
					cur.LocalBounds,
					surfaceList );
		}
	}

	glDisable( GL_POLYGON_OFFSET_FILL );

	if ( ShowStats )
	{
		LOG( "VRMenuMgr: submitted %i surfaces", NumToRender );
	}
}

//==============================
// VRMenuMgrLocal::GetGUIGlProgram
GlProgram const * VRMenuMgrLocal::GetGUIGlProgram( eGUIProgramType const programType ) const
{
    switch( programType )
    {
        case PROGRAM_DIFFUSE_ONLY:
            return &GUIProgramDiffuseOnly;
		case PROGRAM_DIFFUSE_ALPHA_DISCARD:
			return &GUIProgramDiffuseAlphaDiscard;
        case PROGRAM_ADDITIVE_ONLY:
            return &GUIProgramDiffuseOnly;
        case PROGRAM_DIFFUSE_PLUS_ADDITIVE:
            return &GUIProgramDiffusePlusAdditive;
        case PROGRAM_DIFFUSE_COMPOSITE:
            return &GUIProgramDiffuseComposite;
        case PROGRAM_DIFFUSE_COLOR_RAMP:
            return &GUIProgramDiffuseColorRamp;
        case PROGRAM_DIFFUSE_COLOR_RAMP_TARGET:
            return &GUIProgramDiffuseColorRampTarget;
		case PROGRAM_ALPHA_DIFFUSE:
			return &GUIProgramAlphaDiffuse;
        default:
            ASSERT_WITH_TAG( !"Invalid gui program type", "VrMenu" );
            break;
    }
    return NULL;
}

//==============================
// VRMenuMgrLocal::AddComponentToDeletionList
void VRMenuMgrLocal::AddComponentToDeletionList( menuHandle_t const ownerHandle, VRMenuComponent * component )
{
	int index = -1;
	for ( int i = 0; i < PendingDeletions.GetSizeI(); ++i )
	{
		if ( PendingDeletions[i].GetOwnerHandle() == ownerHandle )
		{
			index = i;
			break;
		}
	}

	if ( index < 0 )
	{
		index = PendingDeletions.GetSizeI();
		PendingDeletions.PushBack( ovrComponentList( ownerHandle ) );
	}

	PendingDeletions[index].AddComponent( component );
}

//==============================
// VRMenuMgrLocal::ExecutePendingComponentDeletions
void VRMenuMgrLocal::ExecutePendingComponentDeletions()
{
	for ( int i = PendingDeletions.GetSizeI() - 1; i >= 0; --i )
	{
		ovrComponentList & list = PendingDeletions[i];
		VRMenuObject * object = ToObject( list.GetOwnerHandle() );
		if ( object != nullptr )
		{
			object->FreeComponents( list );
		}
	}
	PendingDeletions.Clear();
}

//==============================
// OvrVRMenuMgr::Create
OvrVRMenuMgr * OvrVRMenuMgr::Create( OvrGuiSys & guiSys )
{
    VRMenuMgrLocal * mgr = new VRMenuMgrLocal( guiSys );
    return mgr;
}

//==============================
// OvrVRMenuMgr::Free
void OvrVRMenuMgr::Destroy( OvrVRMenuMgr * & mgr )
{
    if ( mgr != NULL )
    {
        mgr->Shutdown();
        delete mgr;
        mgr = NULL;
    }
}

} // namespace OVR
