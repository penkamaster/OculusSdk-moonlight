/************************************************************************************

Filename    :   MainActivity.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/
package com.oculus.oculus360photossdk;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

import android.os.Bundle;
import android.util.Log;
import android.content.Intent;
import com.oculus.vrappframework.VrActivity;

public class MainActivity extends VrActivity {

	public static final String TAG = "Oculus360Photos";

	
	/** Load jni .so on initialization */
	static {
		Log.d( TAG, "LoadLibrary" );
		System.loadLibrary( "oculus360photos" );
	}

	public static native long nativeSetAppInterface( VrActivity act, String fromPackageNameString, String commandString, String uriString );
			
	@Override
	protected void onCreate(Bundle savedInstanceState) 
	{
		Log.d(TAG, "onCreate");
		super.onCreate(savedInstanceState);

		Intent intent = getIntent();
		String commandString = VrActivity.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrActivity.getPackageStringFromIntent( intent );
		String uriString = VrActivity.getUriStringFromIntent( intent );

		setAppPtr( nativeSetAppInterface( this, fromPackageNameString, commandString, uriString ) );
	}
	
	public static byte[] LoadHttpUrl( String str ) {
		int totalLen = 0;

		byte[] returnBuffer = new byte[totalLen];
		
		Log.d(TAG, "LoadHttpUrl " + str );
		try {
	         URL aURL = new URL( str );
	         HttpURLConnection conn = (HttpURLConnection)aURL.openConnection();
	         conn.connect();
	         InputStream is = conn.getInputStream();
	         
	         byte[]	readbuffer = new byte[0x100000];
	         
	         while( true ) {
	        	 int count = is.read(readbuffer);
	        	 if ( count < 0 ) {
	        		 break;
	        	 }
	        	 byte[] tempBuffer = new byte[totalLen + count];
	        	 System.arraycopy(returnBuffer,  0,  tempBuffer,  0,  totalLen );
	        	 System.arraycopy(readbuffer,  0, tempBuffer,  totalLen,  count );
	        	 totalLen += count;
	        	 returnBuffer = tempBuffer;
	         }
	         
	         is.close();
         } catch (Exception e) {
            Log.v(TAG, "LoadHttpUrl", e);
         }
		
		Log.d(TAG, "totalLen " + totalLen );
		
		return returnBuffer;
	}
}
