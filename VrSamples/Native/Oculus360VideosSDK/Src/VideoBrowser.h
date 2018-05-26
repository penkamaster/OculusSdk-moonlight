/************************************************************************************

Filename    :   VideoBrowser.h
Content     :   Subclass for pano video files
Created     :   October 9, 2014
Authors     :   John Carmack, Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#ifndef OVR_VideoBrowser_h
#define OVR_VideoBrowser_h

#include "FolderBrowser.h"
#include "VideosMetaData.h"

namespace OVR
{

class Oculus360Videos;

class VideoBrowser : public OvrFolderBrowser
{
public:
	// only one of these every needs to be created
	static  VideoBrowser *  Create(
				Oculus360Videos &	videos,
				OvrGuiSys &			guiSys,
				OvrVideosMetaData &	metaData,
				unsigned			thumbWidth,  
				float				horizontalPadding,
				unsigned			thumbHeight, 
				float				verticalPadding,
				unsigned 			numSwipePanels,
				float				SwipeRadius );

	// Called when a panel is activated
	virtual void OnPanelActivated( OvrGuiSys & guiSys, const OvrMetaDatum * panelData );

	// Called on a background thread
	//
	// Create the thumbnail image for the file, which will
	// be saved out as a _thumb.jpg.
	virtual unsigned char * CreateAndCacheThumbnail( const char * soureFile, const char * cacheDestinationFile, int & width, int & height );

	// Called on a background thread to load thumbnail
	virtual	unsigned char * LoadThumbnail( const char * filename, int & width, int & height );

	// Adds thumbnail extension to a file to find/create its thumbnail
	virtual String	ThumbName( const String & s );

	// If we fail to load one type of thumbnail, try an alternative
	virtual String	AlternateThumbName( const String & s );

	// Display appropriate info if we fail to find media
	virtual void OnMediaNotFound( OvrGuiSys & guiSys, String & title, String & imageFile, String & message );

protected:
	// Called from the base class when building a cateory.
	virtual String				GetCategoryTitle( OvrGuiSys & guiSys, const char * tag, const char * key ) const;

	// Called from the base class when building a panel
	virtual String				GetPanelTitle( OvrGuiSys & guiSys, const OvrMetaDatum & panelData ) const;

private:
	VideoBrowser(
		Oculus360Videos &	videos,
		OvrGuiSys & guiSys,
		OvrVideosMetaData & metaData,
		float panelWidth,
		float panelHeight,
		float radius,
		unsigned numSwipePanels,
		unsigned thumbWidth,
		unsigned thumbHeight )
		: OvrFolderBrowser( guiSys, metaData, panelWidth, panelHeight, radius, numSwipePanels, thumbWidth, thumbHeight )
		, Videos( videos )
	{
	}

	// private assignment operator to prevent copying
	VideoBrowser &	operator = ( VideoBrowser & );

	virtual ~VideoBrowser()
	{
	}

private:
	Oculus360Videos &	Videos;
};

}

#endif	// OVR_VideoBrowser_h
