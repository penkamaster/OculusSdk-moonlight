/************************************************************************************

Filename    :   VideoBrowser.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VideoBrowser.h"
#include "GlTexture.h"
#include "Kernel/OVR_String_Utils.h"
#include "VrCommon.h"
#include "PackageFiles.h"
#include "ImageData.h"
#include "Oculus360Videos.h"
#include "OVR_Locale.h"
#include "OVR_TurboJpeg.h"
#include "GuiSys.h"

#if defined( OVR_OS_ANDROID )
#include "linux/stat.h"
#include <unistd.h>
#endif

namespace OVR
{

VideoBrowser * VideoBrowser::Create(
		Oculus360Videos & videos,
		OvrGuiSys & guiSys,
		OvrVideosMetaData & metaData,
		unsigned thumbWidth,
		float horizontalPadding,
		unsigned thumbHeight,
		float verticalPadding,
		unsigned 	numSwipePanels,
		float SwipeRadius )
{
	return new VideoBrowser( videos, guiSys, metaData,
		thumbWidth + horizontalPadding, thumbHeight + verticalPadding, SwipeRadius, numSwipePanels, thumbWidth, thumbHeight );
}

void VideoBrowser::OnPanelActivated( OvrGuiSys & guiSys, const OvrMetaDatum * panelData )
{
	OVR_UNUSED( guiSys );
	Videos.OnVideoActivated( panelData );
}

unsigned char * VideoBrowser::CreateAndCacheThumbnail( const char * soureFile, const char * cacheDestinationFile, int & outW, int & outH )
{
	// TODO
	OVR_UNUSED( soureFile );
	OVR_UNUSED( cacheDestinationFile );
	OVR_UNUSED( outW );
	OVR_UNUSED( outH );
	return NULL;
}

unsigned char * VideoBrowser::LoadThumbnail( const char * filename, int & width, int & height )
{
	LOG( "VideoBrowser::LoadThumbnail loading on %s", filename );
	unsigned char * orig = NULL;

	if ( strstr( filename, "assets/" ) )
	{
		void * buffer = NULL;
		int length = 0;
		ovr_ReadFileFromApplicationPackage( filename, length, buffer );

		if ( buffer )
		{
			orig = TurboJpegLoadFromMemory( reinterpret_cast< const unsigned char * >( buffer ), length, &width, &height );
			free( buffer );
		}
	}
	else if ( strstr( filename, ".pvr" ) )
	{
		orig = LoadPVRBuffer( filename, width, height );
	}
	else
	{
		orig = TurboJpegLoadFromFile( filename, &width, &height );
	}

	if ( orig )
	{
		const int thumbWidth = GetThumbWidth();
		const int thumbHeight = GetThumbHeight();

		if ( thumbWidth == width && thumbHeight == height )
		{
			LOG( "VideoBrowser::LoadThumbnail skip resize on %s", filename );
			return orig;
		}

		LOG( "VideoBrowser::LoadThumbnail resizing %s to %ix%i", filename, thumbWidth, thumbHeight );
		unsigned char * outBuffer = ScaleImageRGBA( ( const unsigned char * )orig, width, height, thumbWidth, thumbHeight, IMAGE_FILTER_CUBIC );
		free( orig );

		if ( outBuffer )
		{
			width = thumbWidth;
			height = thumbHeight;

			return outBuffer;
		}
	}
	else
	{
		LOG( "Error: VideoBrowser::LoadThumbnail failed to load %s", filename );
	}
	return NULL;
}

String VideoBrowser::ThumbName( const String & s )
{
	String	ts( s );
	ts = OVR::StringUtils::SetFileExtensionString( ts.ToCStr(), ".pvr" );
	return ts;
}

String VideoBrowser::AlternateThumbName( const String & s )
{
	String	ts( s );
	ts = OVR::StringUtils::SetFileExtensionString( ts.ToCStr(), ".thm" );
	return ts;
}


void VideoBrowser::OnMediaNotFound( OvrGuiSys & guiSys, String & title, String & imageFile, String & message )
{
	OVR_UNUSED( imageFile );
	//imageFile = "assets/sdcard.png";

	Videos.GetLocale().GetString( "@string/app_name", "@string/app_name", title );
	Videos.GetLocale().GetString( "@string/media_not_found", "@string/media_not_found", message );

	OVR::Array< OVR::String > wholeStrs;
	wholeStrs.PushBack( "Gear VR" );
	guiSys.GetDefaultFont().WordWrapText( message, 1.4f, wholeStrs );
}

String VideoBrowser::GetCategoryTitle( OvrGuiSys & guiSys, const char * tag, const char * key ) const
{
	OVR_UNUSED( guiSys );
	OVR_UNUSED( tag );

	String outStr;
	Videos.GetLocale().GetString( key, key, outStr );
	return outStr;
}

String VideoBrowser::GetPanelTitle( OvrGuiSys & guiSys, const OvrMetaDatum & panelData ) const
{
	OVR_UNUSED( guiSys );
	const OvrVideosMetaDatum * const videosDatum = static_cast< const OvrVideosMetaDatum * const >( &panelData );
	if ( videosDatum != NULL )
	{
		String outStr;
		Videos.GetLocale().GetString( videosDatum->Title.ToCStr(), videosDatum->Title.ToCStr(), outStr );
		return outStr;
	}
	return String();
}

}
