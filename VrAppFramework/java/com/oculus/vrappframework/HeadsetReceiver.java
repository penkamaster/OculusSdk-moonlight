// Copyright 2014 Oculus VR, LLC. All Rights reserved.
package com.oculus.vrappframework;

import android.app.Activity;
import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.media.AudioManager;

class HeadsetReceiver extends BroadcastReceiver
{
	private static final String TAG = "HeadsetReceiver";
	private static final boolean DEBUG = false;

	public static HeadsetReceiver receiver = null;
	public static IntentFilter filter = null;

	private static native void headsetStateChanged( int state );

	private static void startReceiver( Context context )
	{
		// initialize with the current headset state
		headsetStateChanged( getCurrentHeadsetState( context ) );

		//Log.d( TAG, "Registering headset receiver" );
		if ( filter == null )
		{
			filter = new IntentFilter( Intent.ACTION_HEADSET_PLUG );
		}
		if ( receiver == null )
		{
			receiver = new HeadsetReceiver();
		}

		context.registerReceiver( receiver, filter );
	}

	private static void stopReceiver( Context context )
	{
		//Log.d( TAG, "Unregistering headset receiver" );
		context.unregisterReceiver( receiver );
	}

	@SuppressWarnings("deprecation")
	private static int getCurrentHeadsetState( Context context )
	{
		AudioManager manager = (AudioManager) context.getSystemService( Context.AUDIO_SERVICE );
		// NOTE: isWiredHeadsetOn() was marked deprecated with API level 14, however, it is still
		// valid to use for checking whether the headset is plugged or not.
		boolean state = manager != null && manager.isWiredHeadsetOn();
		//Log.d( TAG, "getCurrentHeadsetState: " + state );
		return state ? 1 : 0;
	}

	@Override
	public void onReceive( final Context context, final Intent intent )
	{
		// Log.d( TAG, "!$$$$$$! headsetReceiver::onReceive" );

		if ( intent.hasExtra("state") )
		{
			int state = intent.getIntExtra( "state", 0 );
			headsetStateChanged( state );
		}
	}
}
