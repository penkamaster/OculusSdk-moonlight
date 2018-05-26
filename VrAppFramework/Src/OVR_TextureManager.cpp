/************************************************************************************

Filename    :   OVR_TextureManager.cpp
Content     :   Keeps track of textures so they don't need to be loaded more than once.
Created     :   1/22/2016
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

// Make sure we get PRIu64
#define __STDC_FORMAT_MACROS 1

#include "OVR_TextureManager.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_LogUtils.h"
#include "Kernel/OVR_Hash.h"

#include "OVR_FileSys.h"
#include "PackageFiles.h"

//#define OVR_USE_PERF_TIMER
#include "OVR_PerfTimer.h"

namespace OVR {

#define USE_HASH

//==============================================================================================
// ovrManagedTexture
//==============================================================================================

//==============================
// ovrManagedTexture::Free
void ovrManagedTexture::Free()
{
	FreeTexture( Texture );
	Source = TEXTURE_SOURCE_MAX;
	Uri = "";
	IconId = -1;
	Handle = textureHandle_t();
}

//==============================================================================================
// ovrTextureManagerImpl
//==============================================================================================

// DJB2 string hash function
static unsigned long DJB2Hash( char const * str )
{
    unsigned long hash = 5381;
    int c;

	for ( c = *str; c != 0; str++, c = *str )
    //while ( c = *str++ )
	{
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
    return hash;
}

//==============================================================
// Hash functor for OVR::String

template< class C >
class ovrUriHash
{
public:
	UPInt operator()( const C & data ) const 
	{
		return DJB2Hash( data.ToCStr() );
	}
};

//==============================================================
// ovrTextureManagerImpl
class ovrTextureManagerImpl : public ovrTextureManager
{
public:
	friend class ovrTextureManager;

	virtual void				Init() OVR_OVERRIDE;
	virtual void				Shutdown() OVR_OVERRIDE;

	virtual textureHandle_t		LoadTexture( ovrFileSys & fileSys, char const * uri,
										ovrTextureFilter const filterType = FILTER_DEFAULT,
										ovrTextureWrap const wrapType = WRAP_DEFAULT ) OVR_OVERRIDE;
	virtual textureHandle_t		LoadTexture( char const * uri, void const * buffer, size_t const bufferSize,
										ovrTextureFilter const filterType = FILTER_DEFAULT,
										ovrTextureWrap const wrapType = WRAP_DEFAULT ) OVR_OVERRIDE;
	virtual textureHandle_t		LoadRGBATexture( char const * uri, void const * imageData, 
										int const imageWidth, int const imageHeight,
										ovrTextureFilter const filterType = FILTER_DEFAULT,
										ovrTextureWrap const wrapType = WRAP_DEFAULT ) OVR_OVERRIDE;
	virtual textureHandle_t		LoadRGBATexture( int const iconId, void const * imageData, 
										int const imageWidth, int const imageHeight,
										ovrTextureFilter const filterType = FILTER_DEFAULT,
										ovrTextureWrap const wrapType = WRAP_DEFAULT ) OVR_OVERRIDE;

	virtual void				FreeTexture( textureHandle_t const handle ) OVR_OVERRIDE;

	virtual ovrManagedTexture	GetTexture( textureHandle_t const handle ) const OVR_OVERRIDE;
	virtual GlTexture			GetGlTexture( textureHandle_t const handle ) const OVR_OVERRIDE;

	virtual textureHandle_t		GetTextureHandle( char const * uri ) const OVR_OVERRIDE;
	virtual textureHandle_t		GetTextureHandle( int const iconId ) const OVR_OVERRIDE;

	virtual void				PrintStats() const OVR_OVERRIDE;

private:
	Array< ovrManagedTexture >	Textures;
	Array< int >				FreeTextures;
	bool						Initialized;

#if defined( USE_HASH )
	OVR::Hash< String, int, ovrUriHash< String > >	UriHash;
#endif

	mutable int					NumUriLoads;
	mutable int					NumActualUriLoads;
	mutable int					NumBufferLoads;
	mutable int					NumActualBufferLoads;
	mutable int					NumStringSearches;
	mutable int					NumStringCompares;
	mutable int					NumSearches;
	mutable int					NumCompares;

private:
	ovrTextureManagerImpl();
	virtual ~ovrTextureManagerImpl();

	int				FindTextureIndex( char const * uri ) const;
	int				FindTextureIndex( int const iconId ) const;
	int				IndexForHandle( textureHandle_t const handle ) const;
	textureHandle_t AllocTexture();

	static void		SetTextureWrapping( GlTexture & tex, ovrTextureWrap const wrapType );
	static void		SetTextureFiltering( GlTexture & tex, ovrTextureFilter const filterType );
};

//==============================
// ovrTextureManagerImpl::
ovrTextureManagerImpl::ovrTextureManagerImpl()
	: Initialized( false )
	, NumUriLoads( 0 )
	, NumActualUriLoads( 0 )
	, NumBufferLoads( 0 )
	, NumActualBufferLoads( 0 )
	, NumStringSearches( 0 )
	, NumStringCompares( 0 )
	, NumSearches( 0 )
	, NumCompares( 0 )
{
}

//==============================
// ovrTextureManagerImpl::
ovrTextureManagerImpl::~ovrTextureManagerImpl()
{
	OVR_ASSERT( !Initialized );	// call Shutdown() explicitly
}

//==============================
// ovrTextureManagerImpl::
void ovrTextureManagerImpl::Init()
{
#if defined( USE_HASH )
	UriHash.SetCapacity( 512 );
#endif
	Initialized = true;
}

//==============================
// ovrTextureManagerImpl::
void ovrTextureManagerImpl::Shutdown()
{
	for ( int i = 0; i < Textures.GetSizeI(); ++i )
	{
		if ( Textures[i].IsValid() )
		{
			Textures[i].Free();
		}
	}

	Textures.Resize( 0 );
	FreeTextures.Resize( 0 );
#if defined( USE_HASH )
	UriHash.Clear();
#endif

	Initialized = false;
}

//==============================
// ovrTextureManagerImpl::SetTextureWrapping
void ovrTextureManagerImpl::SetTextureWrapping( GlTexture & tex, ovrTextureWrap const wrapType )
{
	OVR_PERF_TIMER( SetTextureWrapping );
	switch( wrapType )
	{
		case WRAP_DEFAULT:
			return;
		case WRAP_CLAMP:
			MakeTextureClamped( tex );
			break;
		case WRAP_REPEAT:
			glBindTexture( tex.target, tex.texture );
			glTexParameteri( tex.target, GL_TEXTURE_WRAP_S, GL_REPEAT );
			glTexParameteri( tex.target, GL_TEXTURE_WRAP_T, GL_REPEAT );
			glBindTexture( tex.target, 0 );
			break;
		default:
			OVR_ASSERT( false );
			break;
	}
}

//==============================
// ovrTextureManagerImpl::SetTextureFiltering
void ovrTextureManagerImpl::SetTextureFiltering( GlTexture & tex, ovrTextureFilter const filterType )
{
	OVR_PERF_TIMER( SetTextureFiltering );
	switch( filterType )
	{
		case FILTER_DEFAULT:
			return;
		case FILTER_LINEAR:
			MakeTextureLinear( tex );
			break;
		case FILTER_MIPMAP_LINEAR:
			MakeTextureLinearNearest( tex );
			break;
		case FILTER_MIPMAP_TRILINEAR:
			MakeTextureTrilinear( tex );
			break;
		case FILTER_MIPMAP_ANISOTROPIC2:
			MakeTextureAniso( tex, 2.0f );
			break;
		case FILTER_MIPMAP_ANISOTROPIC4:
			MakeTextureAniso( tex, 4.0f );
			break;
		case FILTER_MIPMAP_ANISOTROPIC8:
			MakeTextureAniso( tex, 8.0f );
			break;
		case FILTER_MIPMAP_ANISOTROPIC16:
			MakeTextureAniso( tex, 16.0f );
			break;
		default:
			OVR_ASSERT( false );
	}
}

//==============================
// ovrTextureManagerImpl::LoadTexture
textureHandle_t ovrTextureManagerImpl::LoadTexture( ovrFileSys & fileSys, char const * uri,
	ovrTextureFilter const filterType, ovrTextureWrap const wrapType )
{
	OVR_PERF_TIMER( LoadTexture_FromFile );

	NumUriLoads++;

	int idx = FindTextureIndex( uri );
	if ( idx >= 0 )
	{
		return Textures[idx].GetHandle();
	}

	int w;
	int h;
	GlTexture tex = LoadTextureFromUri( fileSys, uri, TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), w, h );
	if ( !tex.IsValid() )
	{
		LOG( "LoadTextureFromUri( '%s' ) failed!", uri );
		return textureHandle_t();
	}

	textureHandle_t handle = AllocTexture();
	if ( handle.IsValid() )
	{
		SetTextureWrapping( tex, wrapType );
		SetTextureFiltering( tex, filterType );

		idx = IndexForHandle( handle );
		Textures[idx] = ovrManagedTexture( handle, uri, tex );
#if defined( USE_HASH )
		UriHash.Add( String( uri ), idx );
#endif

		NumActualUriLoads++;
	}

	return handle;
}

//==============================
// ovrTextureManagerImpl::LoadTexture
textureHandle_t	ovrTextureManagerImpl::LoadTexture( char const * uri, void const * buffer, size_t const bufferSize,
		ovrTextureFilter const filterType, ovrTextureWrap const wrapType )
{
	OVR_PERF_TIMER( LoadTexture_FromBuffer );

	NumBufferLoads++;

	int idx = FindTextureIndex( uri );
	if ( idx >= 0 )
	{
		return Textures[idx].GetHandle();
	}

	int width = 0;
	int height = 0;
	// NOTE: MemBuffer doesn't delete on destruction
	MemBuffer buff( static_cast< uint8_t const * >( buffer ), static_cast< int >( bufferSize ) );
	GlTexture tex = LoadTextureFromBuffer( uri, buff, TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );

	if ( !tex.IsValid() )
	{
		LOG( "LoadTextureFromBuffer( '%s', %p, %" PRIu64 " ) failed!", uri, buffer, static_cast< uint64_t >( bufferSize ) );
		return textureHandle_t();
	}

	textureHandle_t handle = AllocTexture();
	if ( handle.IsValid() )
	{
		OVR_PERF_TIMER( LoadTexture_FromBuffer_IsValid );
		SetTextureWrapping( tex, wrapType );
		SetTextureFiltering( tex, filterType );

		idx = IndexForHandle( handle );
		Textures[idx] = ovrManagedTexture( handle, uri, tex );
#if defined( USE_HASH )
		{
			OVR_PERF_TIMER( LoadTexture_FromBuffer_Hash );
			UriHash.Add( String( uri ), idx );
		}
#endif

		NumActualBufferLoads++;
	}

	return handle;
}

//==============================
// ovrTextureManagerImpl::LoadRGBATexture
textureHandle_t	ovrTextureManagerImpl::LoadRGBATexture( char const * uri, void const * imageData, 
		int const imageWidth, int const imageHeight,
		ovrTextureFilter const filterType, ovrTextureWrap const wrapType )
{
	OVR_PERF_TIMER( LoadRGBATexture_uri );

	LOG( "LoadRGBATexture: uri = '%s' ", uri );

	NumBufferLoads++;
	if ( imageData == nullptr || imageWidth <= 0 || imageHeight <= 0 )
	{
		return textureHandle_t();
	}

	int idx = FindTextureIndex( uri );
	if ( idx >= 0 )
	{
		return Textures[idx].GetHandle();
	}

	GlTexture tex;
	{
		OVR_PERF_TIMER( LoadRGBATexture_uri_LoadRGBATextureFromMemory );
		tex = LoadRGBATextureFromMemory( static_cast< const unsigned char* >( imageData ), imageWidth, imageHeight, false );
		if ( !tex.IsValid() )
		{
			//LOG( "LoadRGBATextureFromMemory( '%s', %p, %i, %i ) failed!", uri, imageData, imageWidth, imageHeight );
			return textureHandle_t();
		}
	}

	textureHandle_t handle = AllocTexture();
	if ( handle.IsValid() )
	{
		OVR_PERF_TIMER( LoadRGBATexture_uri_IsValid );
		SetTextureWrapping( tex, wrapType );
		SetTextureFiltering( tex, filterType );

		idx = IndexForHandle( handle );
		Textures[idx] = ovrManagedTexture( handle, uri, tex );
#if defined( USE_HASH )
		{
			OVR_PERF_TIMER( LoadRGBATexture_uri_Hash );
			UriHash.Add( String( uri ), idx );
		}
#endif
		NumActualBufferLoads++;
	}
	return handle;
}

//==============================
// ovrTextureManagerImpl::LoadRGBATexture
textureHandle_t	ovrTextureManagerImpl::LoadRGBATexture( int const iconId, void const * imageData, 
		int const imageWidth, int const imageHeight, 
		ovrTextureFilter const filterType, ovrTextureWrap const wrapType )
{
	OVR_PERF_TIMER( LoadRGBATexture_icon );

	NumBufferLoads++;
	if ( imageData == nullptr || imageWidth <= 0 || imageHeight <= 0 )
	{
		return textureHandle_t();
	}

	int idx = FindTextureIndex( iconId );
	if ( idx >= 0 )
	{
		return Textures[idx].GetHandle();
	}

	GlTexture tex;
	{
		OVR_PERF_TIMER( LoadRGBATexture_icon_LoadRGBATextureFromMemory );
		tex = LoadRGBATextureFromMemory( static_cast< const unsigned char* >( imageData ), imageWidth, imageHeight, false );
		if ( !tex.IsValid() )
		{
			//LOG( "LoadRGBATextureFromMemory( %d, %p, %i, %i ) failed!", iconId, imageData, imageWidth, imageHeight );
			return textureHandle_t();
		}
	}

	textureHandle_t handle = AllocTexture();
	if ( handle.IsValid() )
	{
		SetTextureWrapping( tex, wrapType );
		SetTextureFiltering( tex, filterType );

		idx = IndexForHandle( handle );
		Textures[idx] = ovrManagedTexture( handle, iconId, tex );

		NumActualBufferLoads++;
	}
	return handle;
}

//==============================
// ovrTextureManagerImpl::GetTexture
ovrManagedTexture ovrTextureManagerImpl::GetTexture( textureHandle_t const handle ) const
{
	int idx = IndexForHandle( handle );
	if ( idx < 0 )
	{
		return ovrManagedTexture();
	}
	return Textures[idx];
}

//==============================
// ovrTextureManagerImpl::GetGlTexture
GlTexture ovrTextureManagerImpl::GetGlTexture( textureHandle_t const handle ) const
{
	int idx = IndexForHandle( handle );
	if ( idx < 0 )
	{
		return GlTexture();
	}
	return Textures[idx].GetTexture();
}

//==============================
// ovrTextureManagerImpl::FreeTexture
void ovrTextureManagerImpl::FreeTexture( textureHandle_t const handle )
{
	int idx = IndexForHandle( handle );
	if ( idx >= 0 )
	{
#if defined( USE_HASH )
		if ( Textures[idx].GetUri().IsEmpty() )
		{
			UriHash.Remove( Textures[idx].GetUri() );
		}
#endif
		Textures[idx].Free();
		FreeTextures.PushBack( idx );
	}
}

//==============================
// ovrTextureManagerImpl::FindTextureIndex
int ovrTextureManagerImpl::FindTextureIndex( char const * uri ) const
{
	OVR_PERF_TIMER( FindTextureIndex_uri );

	NumStringSearches++;

	// enable this to always return the first texture. This basically allows 
	// texture file loads to be eliminated during perf testing for purposes
	// of comparison	
#if 0
	if ( Textures.GetSizeI() > 0 )
	{
		return 0;
	}
#endif

#if defined( USE_HASH )
	int index = -1;
	if ( UriHash.Get( String( uri ), &index ) )
	{
		return index;
	}
#else
	for ( int i = 0; i < Textures.GetSizeI(); ++i )
	{
		if ( Textures[i].IsValid() && OVR_stricmp( uri, Textures[i].GetUri() ) == 0 )
		{
			NumCompares += i;
			return i;
		}
	}
	NumStringCompares += Textures.GetSizeI();
#endif
	return -1;
}

//==============================
// ovrTextureManagerImpl::FindTextureIndex
int ovrTextureManagerImpl::FindTextureIndex( int const iconId ) const
{
	OVR_PERF_TIMER( FindTextureIndex_iconId );

	NumSearches++;
	if ( Textures.GetSizeI() > 0 )
	{
		return 0;
	}
	for ( int i = 0; i < Textures.GetSizeI(); ++i )
	{
		if ( Textures[i].IsValid() && Textures[i].GetIconId() == iconId )
		{
			NumCompares += i;
			return i;
		}
	}
	NumCompares += Textures.GetSizeI();
	return -1;
}

//==============================
// ovrTextureManagerImpl::IndexForHandle
int ovrTextureManagerImpl::IndexForHandle( textureHandle_t const handle ) const 
{
	if ( !handle.IsValid() )
	{
		return -1;
	}
	return handle.Get();
}

//==============================
// ovrTextureManagerImpl::AllocTexture
textureHandle_t ovrTextureManagerImpl::AllocTexture()
{
	OVR_PERF_TIMER( AllocTexture );

	if ( FreeTextures.GetSizeI() > 0 )
	{
		int idx = FreeTextures[FreeTextures.GetSizeI() - 1];
		FreeTextures.PopBack();
		Textures[idx] = ovrManagedTexture();
		return textureHandle_t( idx );
	}

	int idx = Textures.GetSizeI();
	Textures.PushBack( ovrManagedTexture() );

	return textureHandle_t( idx );
}

//==============================
// ovrTextureManagerImpl::GetTextureHandle
textureHandle_t ovrTextureManagerImpl::GetTextureHandle( char const * uri ) const 
{
	int idx = FindTextureIndex( uri );
	if ( idx < 0 )
	{
		return textureHandle_t();
	}
	return Textures[idx].GetHandle();
}

//==============================
// ovrTextureManagerImpl::GetTextureHandle
textureHandle_t ovrTextureManagerImpl::GetTextureHandle( int const iconId ) const
{
	int idx = FindTextureIndex( iconId );
	if ( idx < 0 )
	{
		return textureHandle_t();
	}
	return Textures[idx].GetHandle();
}

//==============================
// ovrTextureManagerImpl::PrintStats
void ovrTextureManagerImpl::PrintStats() const 
{
	LOG( "NumUriLoads:          %i",	NumUriLoads );
	LOG( "NumBufferLoads:       %i",	NumBufferLoads );
	LOG( "NumActualUriLoads:    %i",	NumActualUriLoads );
	LOG( "NumActualBufferLoads: %i",	NumActualBufferLoads );

	LOG( "NumStringSearches: %i", NumStringSearches );
	LOG( "NumStringCompares: %i", NumStringCompares );

	LOG( "NumSearches: %i", NumSearches );
	LOG( "NumCompares: %i", NumCompares );
}

//==============================================================================================
// ovrTextureManager
//==============================================================================================

//==============================
// ovrTextureManager::Create
ovrTextureManager * ovrTextureManager::Create()
{
	ovrTextureManagerImpl * m = new ovrTextureManagerImpl();
	m->Init();
	return m;
}

//==============================
// ovrTextureManager::Destroy
void ovrTextureManager::Destroy( ovrTextureManager * & m )
{
	if ( m != nullptr )
	{
		m->Shutdown();
		delete m;
		m = nullptr;
	}
}

} // namespace OVR 
