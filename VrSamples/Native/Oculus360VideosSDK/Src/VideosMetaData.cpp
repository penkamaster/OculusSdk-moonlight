/************************************************************************************

Filename    :   VideosMetaData.cpp
Content     :   A class to manage metadata used by FolderBrowser
Created     :   February 19, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "VideosMetaData.h"

#include "Kernel/OVR_JSON.h"
#include "VrCommon.h"

namespace OVR {

const char * const TITLE_INNER						= "title";
const char * const AUTHOR_INNER						= "author";
const char * const THUMBNAIL_URL_INNER 				= "thumbnail_url";
const char * const STREAMING_TYPE_INNER 			= "streaming_type";
const char * const STREAMING_PROXY_INNER 			= "streaming_proxy";
const char * const STREAMING_SECURITY_LEVEL_INNER 	= "streaming_security_level";
const char * const DEFAULT_AUTHOR_NAME				= "Unspecified Author";

OvrVideosMetaDatum::OvrVideosMetaDatum( const String& url )
	: Author( DEFAULT_AUTHOR_NAME )
{
	Title = ExtractFileBase( url );
}

OvrMetaDatum * OvrVideosMetaData::CreateMetaDatum( const char* url ) const
{
	return new OvrVideosMetaDatum( url );
}

void OvrVideosMetaData::ExtractExtendedData( const JsonReader & jsonDatum, OvrMetaDatum & datum ) const
{
	OvrVideosMetaDatum * videoData = static_cast< OvrVideosMetaDatum * >( &datum );
	if ( videoData )
	{
		videoData->Title 					= jsonDatum.GetChildStringByName( TITLE_INNER );
		videoData->Author 					= jsonDatum.GetChildStringByName( AUTHOR_INNER );
		videoData->ThumbnailUrl 			= jsonDatum.GetChildStringByName( THUMBNAIL_URL_INNER );
		videoData->StreamingType 			= jsonDatum.GetChildStringByName( STREAMING_TYPE_INNER );
		videoData->StreamingProxy 			= jsonDatum.GetChildStringByName( STREAMING_PROXY_INNER );
		videoData->StreamingSecurityLevel 	= jsonDatum.GetChildStringByName( STREAMING_SECURITY_LEVEL_INNER );

		if ( videoData->Title.IsEmpty() )
		{
			videoData->Title = ExtractFileBase( datum.Url.ToCStr() );
		}

		if ( videoData->Author.IsEmpty() )
		{
			videoData->Author = DEFAULT_AUTHOR_NAME;
		}
	}
}

void OvrVideosMetaData::ExtendedDataToJson( const OvrMetaDatum & datum, JSON * outDatumObject ) const
{
	if ( outDatumObject )
	{
		const OvrVideosMetaDatum * const videoData = static_cast< const OvrVideosMetaDatum * const >( &datum );
		if ( videoData )
		{
			outDatumObject->AddStringItem( TITLE_INNER, 					videoData->Title.ToCStr() );
			outDatumObject->AddStringItem( AUTHOR_INNER, 					videoData->Author.ToCStr() );
			outDatumObject->AddStringItem( THUMBNAIL_URL_INNER, 			videoData->ThumbnailUrl.ToCStr() );
			outDatumObject->AddStringItem( STREAMING_TYPE_INNER, 			videoData->StreamingType.ToCStr() );
			outDatumObject->AddStringItem( STREAMING_PROXY_INNER, 			videoData->StreamingProxy.ToCStr() );
			outDatumObject->AddStringItem( STREAMING_SECURITY_LEVEL_INNER, 	videoData->StreamingSecurityLevel.ToCStr() );
		}
	}
}

void OvrVideosMetaData::SwapExtendedData( OvrMetaDatum * left, OvrMetaDatum * right ) const
{
	OvrVideosMetaDatum * leftVideoData = static_cast< OvrVideosMetaDatum * >( left );
	OvrVideosMetaDatum * rightVideoData = static_cast< OvrVideosMetaDatum * >( right );
	if ( leftVideoData && rightVideoData )
	{
		Alg::Swap( leftVideoData->Title, rightVideoData->Title );
		Alg::Swap( leftVideoData->Author, rightVideoData->Author );
	}
}

}
