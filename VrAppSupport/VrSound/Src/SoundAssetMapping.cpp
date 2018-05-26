/************************************************************************************

Filename    :   SoundAssetMapping.cpp
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#include "SoundAssetMapping.h"

#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_LogUtils.h"
#include "Kernel/OVR_String_Utils.h"

#include "PathUtils.h"
#include "PackageFiles.h"
#include "OVR_FileSys.h"

namespace OVR {

static const char * DEV_SOUNDS_RELATIVE = "Oculus/sound_assets.json";
static const char * VRLIB_SOUNDS = "res/raw/sound_assets.json";
static const char * APP_SOUNDS = "assets/sound_assets.json";

void ovrSoundAssetMapping::LoadSoundAssets( ovrFileSys * fileSys )
{
#if defined( OVR_OS_ANDROID )
	Array<String> searchPaths;
	searchPaths.PushBack( "/storage/extSdCard/" );	// FIXME: This does not work for Android-M
	searchPaths.PushBack( "/sdcard/" );

	// First look for sound definition using SearchPaths for dev
	String foundPath;
	if ( GetFullPath( searchPaths, DEV_SOUNDS_RELATIVE, foundPath ) )
	{
		JSON * dataFile = JSON::Load( foundPath.ToCStr() );
		if ( dataFile == NULL )
		{
			FAIL( "ovrSoundAssetMapping::LoadSoundAssets failed to load JSON meta file: %s", foundPath.ToCStr() );
		}
		foundPath.StripTrailing( "sound_assets.json" );
		LoadSoundAssetsFromJsonObject( foundPath, dataFile );
	}
	else // if that fails, we are in release - load sounds from vrappframework/res/raw and the assets folder
	{
		if ( ovr_PackageFileExists( VRLIB_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "res/raw/", VRLIB_SOUNDS );
		}
		if ( ovr_PackageFileExists( APP_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "", APP_SOUNDS );
		}
	}
#else
	const char  * soundAssets[]   = { VRLIB_SOUNDS, APP_SOUNDS };
	const size_t soundAssetCount = sizeof( soundAssets ) / sizeof( soundAssets[0] );
	for ( size_t soundAssetIndex = 0; soundAssetIndex < soundAssetCount; soundAssetIndex++ )
	{
		const String filename = StringUtils::Va( "apk:///%s", soundAssets[soundAssetIndex] );
		MemBufferT< uint8_t > buffer;
		if ( fileSys != nullptr && fileSys->ReadFile( filename.ToCStr(), buffer ) )
		{
			String foundPath = filename;
			foundPath.StripTrailing( "sound_assets.json" );
			const char * perror = nullptr;
			JSON * dataFile = JSON::Parse( reinterpret_cast< char const * >( static_cast< uint8_t const * >( buffer ) ), &perror );
			LoadSoundAssetsFromJsonObject( foundPath, dataFile );	// this releases the JSON memory...
		}
	}
#endif

	if ( SoundMap.IsEmpty() )
	{
		LOG( "SoundManger - failed to load any sound definition files!" );
	}
}

bool ovrSoundAssetMapping::HasSound( const char * soundName ) const
{
	StringHash< String >::ConstIterator soundMapping = SoundMap.Find( soundName );
	return ( soundMapping != SoundMap.End() );
}

bool ovrSoundAssetMapping::GetSound( const char * soundName, String & outSound ) const
{
	StringHash< String >::ConstIterator soundMapping = SoundMap.Find( soundName );
	if ( soundMapping != SoundMap.End() )
	{
		outSound = soundMapping->Second;
		return true;
	}
	else
	{
		WARN( "ovrSoundAssetMapping::GetSound failed to find %s", soundName );
	}

	return false;
}

void ovrSoundAssetMapping::LoadSoundAssetsFromPackage( const String & url, const char * jsonFile )
{
	int bufferLength = 0;
	void * 	buffer = NULL;
	ovr_ReadFileFromApplicationPackage( jsonFile, bufferLength, buffer );
	if ( !buffer )
	{
		FAIL( "ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed to read %s", jsonFile );
	}

	JSON * dataFile = JSON::Parse( reinterpret_cast< char * >( buffer ) );
	if ( !dataFile )
	{
		FAIL( "ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed json parse on %s", jsonFile );
	}
	free( buffer );

	LoadSoundAssetsFromJsonObject( url, dataFile );
}

void ovrSoundAssetMapping::LoadSoundAssetsFromJsonObject( const String & url, JSON * dataFile )
{
	OVR_ASSERT( dataFile );

	// Read in sounds - add to map
	JSON* sounds = dataFile->GetItemByName( "Sounds" );
	OVR_ASSERT( sounds );
	
	const unsigned numSounds = sounds->GetItemCount();

	for ( unsigned i = 0; i < numSounds; ++i )
	{
		const JSON* sound = sounds->GetItemByIndex( i );
		OVR_ASSERT( sound );

		String fullPath( url );
		fullPath.AppendString( sound->GetStringValue().ToCStr() );

		// Do we already have this sound?
		StringHash< String >::ConstIterator soundMapping = SoundMap.Find( sound->Name );
		if ( soundMapping != SoundMap.End() )
		{
			LOG( "SoundManger - adding Duplicate sound %s with asset %s", sound->Name.ToCStr(), fullPath.ToCStr() );
			SoundMap.Set( sound->Name, fullPath );
		}
		else // add new sound
		{
			LOG( "SoundManger read in: %s -> %s", sound->Name.ToCStr(), fullPath.ToCStr() );
			SoundMap.Add( sound->Name, fullPath );
		}
	}

	dataFile->Release();
}

}
