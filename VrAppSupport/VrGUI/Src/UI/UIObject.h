/************************************************************************************

Filename    :   UIObject.h
Content     :
Created     :	1/5/2015
Authors     :   Jim Dose

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( UIObject_h )
#define UIObject_h

#include "VRMenu.h"
#include "GuiSys.h"
#include "UI/UIRect.h"

namespace OVR {

class VrAppInterface;
class UIMenu;
class UITexture;
class VRMenuObject;

class UIObject
{
public:
										UIObject( OvrGuiSys &guiSys );
	virtual							   ~UIObject();

	OvrGuiSys &							GetGuiSys() const { return GuiSys; }

	void 								SetMenuObject( UIMenu * menu, VRMenuObject * vrMenuObject, UIObject * vrMenuObjectParent = NULL );

	UPInt								GetNumChildren() const { return Children.GetSize(); }
	UIObject *							GetChildForIndex( UPInt index ) const { return Children[ index ]; }
	const Array<UIObject *> &			GetChildren() const { return Children; }
	void 								RemoveChild( UIObject *child );

	VRMenuId_t							GetId() const { return Id;  }
	menuHandle_t						GetHandle() const { return Handle; }
	VRMenuObject *						GetMenuObject() const;

	UIObject *							GetParent() const { return Parent; }

	VRMenuObjectFlags_t const &			GetFlags() const;
	void								SetFlags( VRMenuObjectFlags_t const & flags );
	void								AddFlags( VRMenuObjectFlags_t const & flags );
	void								RemoveFlags( VRMenuObjectFlags_t const & flags );

	bool								IsHilighted() const;
	void								SetHilighted( bool const b );
	bool								IsSelected() const;
	virtual void						SetSelected( bool const b );

	void								SetLocalPose( const Posef &pose );
	void								SetLocalPose( const Quatf &orientation, const Vector3f &position );
	Posef const & 						GetLocalPose() const;
	Vector3f const &					GetLocalPosition() const;
	void								SetLocalPosition( Vector3f const & pos );
	Quatf const &						GetLocalRotation() const;
	void								SetLocalRotation( Quatf const & rot );
	Vector3f            				GetLocalScale() const;
	void								SetLocalScale( Vector3f const & scale );
	void								SetLocalScale( float const & scale );

	Posef 								GetWorldPose() const;
	Vector3f 							GetWorldPosition() const;
	Quatf 								GetWorldRotation() const;
	Vector3f            				GetWorldScale() const;

	Vector2f const &					GetColorTableOffset() const;
	void								SetColorTableOffset( Vector2f const & ofs );
	Vector4f const &					GetColor() const;
	void								SetColor( Vector4f const & c );
	bool								GetVisible() const;
	void								SetVisible( const bool visible );

	Vector2f const &					GetDimensions() const { return Dimensions; }	// in local scale
	void								SetDimensions( Vector2f const &dimensions );	// in local scale

	UIRectf const &						GetBorder() const { return Border; }	// in texel scale
	void								SetBorder( UIRectf const &border );		// in texel scale
	UIRectf const &						GetMargin() const { return Margin; }	// in texel scale
	void								SetMargin( UIRectf const &margin );		// in texel scale
	UIRectf const & 					GetPadding() const { return Padding; }	// in texel scale
	void								SetPadding( UIRectf const &padding );	// in texel scale

	UIRectf								GetRect() const;		// in texel scale
	UIRectf								GetPaddedRect() const;	// in texel scale
	UIRectf								GetMarginRect() const;	// in texel scale

	void								AlignTo( const RectPosition myPosition, const UIObject *other, const RectPosition otherPosition, const float zOffset = 0.0f );
	void 								AlignToMargin( const RectPosition myPosition, const UIObject *other, const RectPosition otherPosition, const float zOffset = 0.0f );

	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, char const * imageName );
	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, const GLuint image, const short width, const short height );
	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture &image );
	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture &image, const float dimsX, const float dimsY );
	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture &image, const float dimsX, const float dimsY, const UIRectf &border );
	void								SetImage( const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture &image, const UIRectf &border );
	void 								SetImage( const int surfaceIndex, VRMenuSurfaceParms const & parms );
	void								SetMultiTextureImage( const int surfaceIndex, const eSurfaceTextureType textureType, const UITexture &image1, const UITexture &image2 );

	void 								RegenerateSurfaceGeometry( int const surfaceIndex, const bool freeSurfaceGeometry );
	void 								RegenerateSurfaceGeometry( const bool freeSurfaceGeometry );

	Vector2f const &					GetSurfaceDims( int const surfaceIndex ) const;
	void								SetSurfaceDims( int const surfaceIndex, Vector2f const &dims );	// requires call to RegenerateSurfaceGeometry() to take effect

	UIRectf 							GetSurfaceBorder( int const surfaceIndex );
	void								SetSurfaceBorder( int const surfaceIndex, UIRectf const & border );	// requires call to RegenerateSurfaceGeometry() to take effect

	bool								GetSurfaceVisible( int const surfaceIndex ) const;
	void								SetSurfaceVisible( int const surfaceIndex, const bool visible );

	void								SetLocalBoundsExpand( Vector3f const mins, Vector3f const & maxs );

	Bounds3f							GetLocalBounds( BitmapFont const & font ) const;
	Bounds3f            				GetTextLocalBounds( BitmapFont const & font ) const;

	void								AddComponent( VRMenuComponent * component );
	Array< VRMenuComponent* > const & 	GetComponentList() const;

	void 								WrapChildrenHorizontal();

public:
	static float const					TEXELS_PER_METER;
	static float const					DEFAULT_TEXEL_SCALE;

protected:
	void 								AddToMenuWithParms( UIMenu *menu, UIObject *parent, VRMenuObjectParms &parms );

	OvrGuiSys &							GuiSys;

	UIMenu *							Menu;

	UIObject *							Parent;

	VRMenuId_t 							Id;
	menuHandle_t						Handle;
	VRMenuObject *						Object;

	Vector2f 							Dimensions;
	UIRectf 							Border;
	UIRectf								Margin;
	UIRectf								Padding;

	Array<UIObject *>					Children;

private:
	// private assignment operator to prevent copying
	UIObject &	operator = ( UIObject & );
};

} // namespace OVR

#endif // UIMenu_h
