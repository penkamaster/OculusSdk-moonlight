/************************************************************************************

Filename    :   VideosMetaData.h
Content     :   A class to manage metadata used by 360 videos folder browser
Created     :   February 20, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/


#if !defined( OVR_VideosMetaData_h )
#define OVR_VideosMetaData_h

#include "Kernel/OVR_String.h"
#include "MetaDataManager.h"

namespace OVR {

//==============================================================
// OvrVideosMetaDatum
struct OvrVideosMetaDatum : public OvrMetaDatum
{
	String	Author;
	String	Title;
	String	ThumbnailUrl;
	String  StreamingType;
	String  StreamingProxy;
	String  StreamingSecurityLevel;

	OvrVideosMetaDatum( const String& url );
};

//==============================================================
// OvrVideosMetaData
class OvrVideosMetaData : public OvrMetaData
{
public:
	virtual ~OvrVideosMetaData() {}

protected:
	virtual OvrMetaDatum *	CreateMetaDatum( const char* url ) const;
	virtual	void			ExtractExtendedData( const JsonReader & jsonDatum, OvrMetaDatum & outDatum ) const;
	virtual	void			ExtendedDataToJson( const OvrMetaDatum & datum, JSON * outDatumObject ) const;
	virtual void			SwapExtendedData( OvrMetaDatum * left, OvrMetaDatum * right ) const;
};

}

#endif // OVR_VideosMetaData_h
