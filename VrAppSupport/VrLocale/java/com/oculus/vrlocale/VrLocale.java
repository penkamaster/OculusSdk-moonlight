/************************************************************************************

Filename    :   VrLocale.java
Content     :   Locale helper functions for native code.
Created     :   6/22/2015
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

package com.oculus.vrlocale;

import java.util.Locale;
import android.content.Context;
import android.content.res.Resources;
import android.util.Log;

public class VrLocale {
	// FIXME: setting the locale is only necessary if we support using the Horizon locale.
/*
	// we need this string to track when setLocale has changed the language, otherwise if
	// the language is changed with setLocale, we can't determine the current language of
	// the application.
	private static String currentLanguage = null;

	private void setLocale( String localeName )
	{
		Configuration conf = getResources().getConfiguration();
		conf.locale = new Locale( localeName );
		
		currentLanguage = conf.locale.getLanguage();

		DisplayMetrics metrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics( metrics );
		
		// the next line just changes the application's locale. It changes this
		// globally and not just in the newly created resource
		Resources res = new Resources( getAssets(), metrics, conf );
		// since we don't need the resource, just allow it to be garbage collected.
		res = null;

		Log.d( TAG, "setLocale: locale set to " + localeName );
	}

	private void setDefaultLocale()
	{
		setLocale( "en" );
	}
*/
	public static String getCurrentLanguage() {
		return Locale.getDefault().getLanguage();
/*
		// In the case of Unity, the activity onCreate does not set the current langage
		// so we need to assume it is defaulted if setLocale() has never been called
		if ( currentLanguage == null || currentLanguage.isEmpty() ) {
			currentLanguage = Locale.getDefault().getLanguage();
		}
		return currentLanguage;
*/
	}

	private static String getLocalizedString( Context context, String name ) {
		//Log.v("VrLocale", "getLocalizedString for " + name );
		String outString = "";
        final String packageName = context.getPackageName();
        final Resources resources = context.getResources();
		int id = resources.getIdentifier( name, "string", packageName );
		if ( id == 0 )
		{
			// 0 is not a valid resource id
			Log.v("VrLocale", name + " is not a valid resource id!!" );
			return outString;
		} 
		if ( id != 0 ) 
		{
			outString = resources.getText( id ).toString();
			//Log.v("VrLocale", "getLocalizedString resolved " + name + " to " + outString);
		}
		return outString;
	}

	public static String getDisplayLanguageForLocaleCode( String code )
	{
		Locale locale = new Locale( code );
		return locale.getDisplayLanguage();
	}
}