/************************************************************************************

Filename    :   PhotosMetaData.cpp
Content     :   A class to manage metadata used by FolderBrowser
Created     :   February 19, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "PhotosMetaData.h"

#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_LogUtils.h"

#include "VrCommon.h"

namespace OVR {

const char * const TITLE_INNER			= "title";
const char * const AUTHOR_INNER			= "author";
const char * const DEFAULT_AUTHOR_NAME	= "Unspecified Author";

OvrPhotosMetaDatum::OvrPhotosMetaDatum( const String& url )
	: Author( DEFAULT_AUTHOR_NAME )
{
	Title = ExtractFileBase( url );
}

OvrMetaDatum * OvrPhotosMetaData::CreateMetaDatum( const char* url ) const
{
	return new OvrPhotosMetaDatum( url );
}

void OvrPhotosMetaData::ExtractExtendedData( const JsonReader & jsonDatum, OvrMetaDatum & datum ) const
{
	OvrPhotosMetaDatum * photoData = static_cast< OvrPhotosMetaDatum * >( &datum );
	if ( photoData )
	{
		photoData->Title = jsonDatum.GetChildStringByName( TITLE_INNER );
		photoData->Author = jsonDatum.GetChildStringByName( AUTHOR_INNER );

		if ( photoData->Title.IsEmpty() )
		{
			photoData->Title = ExtractFileBase( datum.Url.ToCStr() );
		}

		if ( photoData->Author.IsEmpty() )
		{
			photoData->Author = DEFAULT_AUTHOR_NAME;
		}
	}
}

void OvrPhotosMetaData::ExtendedDataToJson( const OvrMetaDatum & datum, JSON * outDatumObject ) const
{
	if ( outDatumObject )
	{
		const OvrPhotosMetaDatum * const photoData = static_cast< const OvrPhotosMetaDatum * const >( &datum );
		if ( photoData )
		{
			outDatumObject->AddStringItem( TITLE_INNER, photoData->Title.ToCStr() );
			outDatumObject->AddStringItem( AUTHOR_INNER, photoData->Author.ToCStr() );
		}
	}
}

void OvrPhotosMetaData::SwapExtendedData( OvrMetaDatum * left, OvrMetaDatum * right ) const
{
	OvrPhotosMetaDatum * leftPhotoData = static_cast< OvrPhotosMetaDatum * >( left );
	OvrPhotosMetaDatum * rightPhotoData = static_cast< OvrPhotosMetaDatum * >( right );
	if ( leftPhotoData && rightPhotoData )
	{
		Alg::Swap( leftPhotoData->Title, rightPhotoData->Title );
		Alg::Swap( leftPhotoData->Author, rightPhotoData->Author );
	}
}

}
