/************************************************************************************

Filename    :   PhotosMetaData.h
Content     :   A class to manage metadata used by FolderBrowser
Created     :   February 19, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_PhotosMetaData_h )
#define OVR_PhotosMetaData_h 

#include "Kernel/OVR_String.h"
#include "MetaDataManager.h"

namespace OVR {

//==============================================================
// OvrPhotosMetaDatum
struct OvrPhotosMetaDatum : public OvrMetaDatum
{
	String	Author;
	String	Title;

	OvrPhotosMetaDatum( const String& url );
};

//==============================================================
// OvrPhotosMetaData
class OvrPhotosMetaData : public OvrMetaData
{
public:
	virtual ~OvrPhotosMetaData() {}

protected:
	virtual OvrMetaDatum *	CreateMetaDatum( const char* url ) const;
	virtual	void			ExtractExtendedData( const JsonReader & jsonDatum, OvrMetaDatum & outDatum ) const;
	virtual	void			ExtendedDataToJson( const OvrMetaDatum & datum, JSON * outDatumObject ) const;
	virtual void			SwapExtendedData( OvrMetaDatum * left, OvrMetaDatum * right ) const;
	virtual bool			IsRemote( const OvrMetaDatum * /*datum*/ ) const	{ return false; } 
};

}

#endif // OVR_PhotosMetaData_h 
