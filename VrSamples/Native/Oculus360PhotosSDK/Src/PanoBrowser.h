/************************************************************************************

Filename    :   PanoBrowser.h
Content     :   Subclass for panoramic image files
Created     :   August 13, 2014
Authors     :   John Carmack, Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#ifndef OVR_PanoSwipeDir_h
#define OVR_PanoSwipeDir_h

#include "FolderBrowser.h"

namespace OVR {

class Oculus360Photos;

class PanoBrowser : public OvrFolderBrowser
{
public:
	// only one of these every needs to be created
	static  PanoBrowser *		Create(
		Oculus360Photos &		photos,
		OvrGuiSys &				guiSys,
		OvrMetaData &			metaData,
		unsigned				thumbWidth,
		float					horizontalPadding,
		unsigned				thumbHeight,
		float					verticalPadding,
		unsigned 				numSwipePanels,
		float					swipeRadius );

	// Swiping when the menu is inactive can cycle through files in
	// the directory.  Step can be 1 or -1.
	virtual	const OvrMetaDatum *	NextFileInDirectory( OvrGuiSys & guiSys, const int step );
	void							AddToFavorites( OvrGuiSys & guiSys, const OvrMetaDatum * panoData );
	void							RemoveFromFavorites( OvrGuiSys & guiSys, const OvrMetaDatum * panoData );

	// Called when a panel is activated
	virtual void				OnPanelActivated( OvrGuiSys & guiSys, const OvrMetaDatum * panelData );

	// Called on opening menu - used to rebuild favorites using FavoritesBuffer
	virtual void				OnBrowserOpen( OvrGuiSys & guiSys );

	// Reload FavoritesBuffer with what's currently in favorites folder in FolderBrowser
	void						ReloadFavoritesBuffer( OvrGuiSys & guiSys );

	// Called on a background thread
	//
	// Create the thumbnail image for the file, which will
	// be saved out as a _thumb.jpg.
	virtual unsigned char *		CreateAndCacheThumbnail( const char * soureFile, const char * cacheDestinationFile, int & width, int & height );

	// Called on a background thread to load thumbnail
	virtual	unsigned char *		LoadThumbnail( const char * filename, int & width, int & height );

	// Adds thumbnail extension to a file to find/create its thumbnail
	virtual String				ThumbName( const String & s );

	// Called when no media found
	virtual void				OnMediaNotFound( OvrGuiSys & guiSys, String & title, String & imageFile, String & message );

	// Called if touch up is activated without a focused item
	virtual bool				OnTouchUpNoFocused( OvrGuiSys & guiSys );

	int							GetNumPanosInActive( OvrGuiSys & guiSys ) const;

protected:
	// Called from the base class when building category.
	virtual String				GetCategoryTitle( OvrGuiSys & guiSys, const char * tag, const char * key ) const;

	// Called from the base class when building a panel.
	virtual String				GetPanelTitle( OvrGuiSys & guiSys, const OvrMetaDatum & panelData ) const;

private:
	PanoBrowser(
		Oculus360Photos & photos,
		OvrGuiSys & guiSys,
		OvrMetaData & metaData,
		float panelWidth,
		float panelHeight,
		float radius,
		unsigned numSwipePanels,
		unsigned thumbWidth,
		unsigned thumbHeight )
		: OvrFolderBrowser( guiSys, metaData, panelWidth, panelHeight, radius, numSwipePanels, thumbWidth, thumbHeight )
		, Photos( photos )
		, BufferDirty( false )
	{
	}

	// private assignment operator to prevent copying
	PanoBrowser &	operator = ( PanoBrowser & );

	virtual ~PanoBrowser()
	{
	}
		
	Oculus360Photos &		Photos;

	// Favorites buffer - this is built by PanoMenu and used to rebuild Favorite's menu
	// if an item in it is null, it's been deleted from favorites
	struct Favorite
	{
		Favorite()
		: Data( NULL )
		, IsFavorite( false )
		{}
		const OvrMetaDatum *	Data;
		bool					IsFavorite;
	};
	Array< Favorite >	FavoritesBuffer;
	bool				BufferDirty;
	bool 				IsFavoritesBufferEmpty();
};

unsigned char * LoadPageJpg( const char * cbzFileName, int pageNum, int & width, int & height );

} // namespace OVR

#endif	// OVR_PanoSwipeDir_h
