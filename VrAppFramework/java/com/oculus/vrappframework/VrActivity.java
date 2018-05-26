/************************************************************************************

Filename    :   VrActivity.java
Content     :   Activity used by the application framework.
Created     :   9/26/2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculus.vrappframework;

import java.io.File;
import java.util.List;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.ComponentName;
import android.content.ActivityNotFoundException;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.StatFs;
import android.provider.Settings;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;

public class VrActivity extends Activity {

	public static final String TAG = "VrActivity";

	private static native void nativeNewIntent(long appPtr, String fromPackageName, String command, String uriString);
	private static native void nativeKeyEvent(long appPtr, int keyNum, boolean down, int repeatCount );
	private static native void nativeJoypadAxis(long appPtr, float lx, float ly, float rx, float ry);
	private static native void nativeTouch(long appPtr, int action, float x, float y );

	// Pass down to native code so we talk to the right App object.
	// This is set by the subclass in onCreate
	//   setAppPtr( nativeSetAppInterface( this, ... ) );
    private VrApp app;

    public long getAppPtr() {
      return this.app.getAppPtr();
    }

    protected void setAppPtr(long appPtr) {
      if (this.app != null) {
        throw new RuntimeException("Application pointer is already set!");
      }
      this.app = new VrApp(this, appPtr);
    }

	// These variables duplicated in SystemActivitiesReceiver.java!
	public static final String INTENT_KEY_CMD = "intent_cmd";
	public static final String INTENT_KEY_FROM_PKG = "intent_pkg";

	public static String getCommandStringFromIntent( Intent intent ) {
		String commandStr = "";
		if ( intent != null ) {
			commandStr = intent.getStringExtra( INTENT_KEY_CMD );
			if ( commandStr == null ) {
				commandStr = "";
			}
		}
		return commandStr;
	}

	public static String getPackageStringFromIntent( Intent intent ) {
		String packageStr = "";
		if ( intent != null ) {
			packageStr = intent.getStringExtra( INTENT_KEY_FROM_PKG );
			if ( packageStr == null ) {
				packageStr = "";
			}
		}
		return packageStr;
	}

	public static String getUriStringFromIntent( Intent intent ) {
		String uriString = "";
		if ( intent != null ) {
			Uri uri = intent.getData();
			if ( uri != null ) {
				uriString = uri.toString();
				if ( uriString == null ) {
					uriString = "";
				}
			}
		}
		return uriString;
	}

	/*
	================================================================================

	Activity life cycle

	================================================================================
	*/

	@Override protected void onCreate(Bundle savedInstanceState) {
		Log.d( TAG, this + " onCreate()" );
		super.onCreate(savedInstanceState);

        VrApp.onCreate(this);

		// Check preferences
		SharedPreferences prefs = getApplicationContext().getSharedPreferences( "oculusvr", MODE_PRIVATE );
		String username = prefs.getString( "username", "guest" );
		Log.d( TAG, "username = " + username );
		
		// Check for being launched with a particular intent
		Intent intent = getIntent();

		String commandString = getCommandStringFromIntent( intent );
		String fromPackageNameString = getPackageStringFromIntent( intent );
		String uriString = getUriStringFromIntent( intent );

		Log.d( TAG, "action:" + intent.getAction() );
		Log.d( TAG, "type:" + intent.getType() );
		Log.d( TAG, "fromPackageName:" + fromPackageNameString );
		Log.d( TAG, "command:" + commandString );
		Log.d( TAG, "uri:" + uriString );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );
	}	
	
	@Override protected void onRestart() {
		Log.d(TAG, this + " onRestart()");		
		super.onRestart();
	}

	@Override protected void onStart() {
		Log.d(TAG, this + " onStart()");
		super.onStart();
	}

	@Override protected void onResume() {
		Log.d(TAG, this + " onResume()");

		super.onResume();
        app.onResume();
	}
	
	@Override protected void onPause() {
		Log.d(TAG, this + " onPause()");
		
		app.onPause();
		super.onPause();
	}

	@Override protected void onStop() {
		Log.d(TAG, this + " onStop()");
		super.onStop();
	}
	
	@Override protected void onDestroy() {
		Log.d(TAG, this + " onDestroy()");
		if ( app != null )
		{
			app.onDestroy();
			app = null;
		}
		super.onDestroy();
	}
		
	@Override public void onConfigurationChanged(Configuration newConfig) {
		Log.d(TAG, this + " onConfigurationChanged()");
		super.onConfigurationChanged(newConfig);
	}

	@Override protected void onNewIntent( Intent intent ) {
		Log.d(TAG, "onNewIntent()");

		String commandString = getCommandStringFromIntent( intent );
		String fromPackageNameString = getPackageStringFromIntent( intent );
		String uriString = getUriStringFromIntent( intent );

		Log.d(TAG, "action:" + intent.getAction() );
		Log.d(TAG, "type:" + intent.getType() );
		Log.d(TAG, "fromPackageName:" + fromPackageNameString );
		Log.d(TAG, "command:" + commandString );
		Log.d(TAG, "uri:" + uriString );

		nativeNewIntent( getAppPtr(), fromPackageNameString, commandString, uriString );
	}

	public void finishActivity() {
		finish();
	}

	/*
	================================================================================

	Sound

	================================================================================
	*/

	private void adjustVolume(int direction) {
		AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
		audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, direction, 0);
	}

	/*
	================================================================================

	Input

	================================================================================
	*/

	private float deadBand(float f) {
		// The K joypad seems to need a little more dead band to prevent
		// creep.
		if (f > -0.01f && f < 0.01f) {
			return 0.0f;
		}
		return f;
	}

	private int axisButtons(int deviceId, float axisValue, int negativeButton, int positiveButton,
			int previousState) {
		int currentState;
		if (axisValue < -0.5f) {
			currentState = -1;
		} else if (axisValue > 0.5f) {
			currentState = 1;
		} else {
			currentState = 0;
		}
		if (currentState != previousState) {
			if (previousState == -1) {
				// negativeButton up
				buttonEvent(deviceId, negativeButton, false, 0);
			} else if (previousState == 1) {
				// positiveButton up
				buttonEvent(deviceId, positiveButton, false, 0);
			}

			if (currentState == -1) {
				// negativeButton down
				buttonEvent(deviceId, negativeButton, true, 0);
			} else if (currentState == 1) {
				// positiveButton down
				buttonEvent(deviceId, positiveButton, true, 0);
			}
		}
		return currentState;
	}

	private int[] axisState = new int[6];
	private int[] axisAxis = { MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_RX, MotionEvent.AXIS_RY };
	private int[] axisNegativeButton = { JoyEvent.KEYCODE_DPAD_LEFT, JoyEvent.KEYCODE_DPAD_UP, JoyEvent.KEYCODE_LSTICK_LEFT, JoyEvent.KEYCODE_LSTICK_UP, JoyEvent.KEYCODE_RSTICK_LEFT, JoyEvent.KEYCODE_RSTICK_UP };
	private int[] axisPositiveButton = { JoyEvent.KEYCODE_DPAD_RIGHT, JoyEvent.KEYCODE_DPAD_DOWN, JoyEvent.KEYCODE_LSTICK_RIGHT, JoyEvent.KEYCODE_LSTICK_DOWN, JoyEvent.KEYCODE_RSTICK_RIGHT, JoyEvent.KEYCODE_RSTICK_DOWN };
			
	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0
				&& event.getAction() == MotionEvent.ACTION_MOVE) {
			// The joypad sends a single event with all the axis
			
			// Unfortunately, the Moga joypad in HID mode uses AXIS_Z
			// where the Samsnung uses AXIS_RX, and AXIS_RZ instead of AXIS_RY
			
			// Log.d(TAG,
			// String.format("onTouchEvent: %f %f %f %f",
			// event.getAxisValue(MotionEvent.AXIS_X),
			// event.getAxisValue(MotionEvent.AXIS_Y),
			// event.getAxisValue(MotionEvent.AXIS_RX),
			// event.getAxisValue(MotionEvent.AXIS_RY)));
			nativeJoypadAxis(getAppPtr(), deadBand(event.getAxisValue(MotionEvent.AXIS_X)),
					deadBand(event.getAxisValue(MotionEvent.AXIS_Y)),
					deadBand(event.getAxisValue(MotionEvent.AXIS_RX))
						+ deadBand(event.getAxisValue(MotionEvent.AXIS_Z)),		// Moga uses  Z for R-stick X
					deadBand(event.getAxisValue(MotionEvent.AXIS_RY))
						+ deadBand(event.getAxisValue(MotionEvent.AXIS_RZ)));	// Moga uses RZ for R-stick Y

			// Turn the hat and thumbstick axis into buttons
			for ( int i = 0 ; i < 6 ; i++ ) {
				axisState[i] = axisButtons( event.getDeviceId(),
						event.getAxisValue(axisAxis[i]),
						axisNegativeButton[i], axisPositiveButton[i],
						axisState[i]);
			}
					
			return true;
		}
		return super.dispatchGenericMotionEvent(event);
	}

    @Override
	public boolean dispatchKeyEvent(KeyEvent event) {
		//Log.d(TAG, "dispatchKeyEvent  " + event );
		boolean down;
		int keyCode = event.getKeyCode();
		int deviceId = event.getDeviceId();

		if (event.getAction() == KeyEvent.ACTION_DOWN) {
			down = true;
		} else if (event.getAction() == KeyEvent.ACTION_UP) {
			down = false;
		} else {
			Log.d(TAG,
					"source " + event.getSource() + " action "
							+ event.getAction() + " keyCode " + keyCode);

			return super.dispatchKeyEvent(event);
		}

		Log.d(TAG, "source " + event.getSource() + " keyCode " + keyCode + " down " + down + " repeatCount " + event.getRepeatCount() );

		if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
			if ( down ) {
				adjustVolume(1);
			}
			return true;
		}

		if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
			if ( down ) {
				adjustVolume(-1);	
			}
			return true;
		}

		
		// Joypads will register as keyboards, but keyboards won't
		// register as position classes
		// || event.getSource() != 16777232)
		// Volume buttons are source 257
		if (event.getSource() == 1281) {
			keyCode |= JoyEvent.BUTTON_JOYPAD_FLAG;			
		}
		return buttonEvent(deviceId, keyCode, down, event.getRepeatCount() );
	}

	/*
	 * Called for real key events from dispatchKeyEvent(), and also
	 * the synthetic dpad 
	 * 
	 * This is where the framework can intercept a button press before
	 * it is passed on to the application.
	 * 
	 * Keyboard buttons will be standard Android key codes, but joystick buttons
	 * will have BUTTON_JOYSTICK or'd into it, because you may want arrow keys
	 * on the keyboard to perform different actions from dpad buttons on a
	 * joypad.
	 * 
	 * The BUTTON_* values are what you get in joyCode from our reference
	 * joypad.
	 * 
	 * @return Return true if this event was consumed.
	 */
	protected boolean buttonEvent(int deviceId, int keyCode, boolean down, int repeatCount ) {
		//Log.d(TAG, "buttonEvent " + deviceId + " " + keyCode + " " + down);
		
		// DispatchKeyEvent will cause the K joypads to spawn other
		// apps on select and "game", which we don't want, so manually call
		// onKeyDown or onKeyUp
		
		KeyEvent ev = new KeyEvent( 0, 0, down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, keyCode, repeatCount );
		
		// This was confusing because it called VrActivity::onKeyDown.  Activity::onKeyDown is only supposed to be 
		// called if the app views didn't consume any keys. Since we intercept dispatchKeyEvent and always returning true
		// for ACTION_UP and ACTION_DOWN, we effectively consume ALL key events that would otherwise go to Activity::onKeyDown
		// Activity::onKeyUp, so calling them here means they're getting called when we consume events, even though the 
		// VrActivity versions were effectively the consumers by calling nativeKeyEvent.  Instead, call nativeKeyEvent
		// here directly.
		if ( down ) {
			nativeKeyEvent( getAppPtr(), keyCode, true, ev.getRepeatCount() );
		}
		else
		{
			nativeKeyEvent( getAppPtr(), keyCode, false, 0 );
		}
		return true;
	}

	@Override
	public boolean dispatchTouchEvent( MotionEvent e ) {
		// Log.d(TAG, "real:" + e );		
		int action = e.getAction();
		float x = e.getRawX();
		float y = e.getRawY();
		Log.d(TAG, "onTouch dev:" + e.getDeviceId() + " act:" + action + " ind:" + e.getActionIndex() + " @ "+ x + "," + y );
		nativeTouch( getAppPtr(), action, x, y );
		return true;
	}

	/*
	================================================================================

	Misc

	================================================================================
	*/

/*
	public void sendIntent( String packageName, String data ) {
		Log.d(TAG, "sendIntent " + packageName + " : " + data );
		
		Intent launchIntent = getPackageManager().getLaunchIntentForPackage(packageName);
		if ( launchIntent == null )
		{
			Log.d( TAG, "sendIntent null activity" );
			return;
		}
		if ( data.length() > 0 ) {
			launchIntent.setData( Uri.parse( data ) );
		}
		try {
			Log.d(TAG, "startActivity " + launchIntent );
			launchIntent.addFlags( Intent.FLAG_ACTIVITY_NO_ANIMATION );
			startActivity(launchIntent);
		} catch( ActivityNotFoundException e ) {
			Log.d( TAG, "startActivity " + launchIntent + " not found!" );	
			return;
		}
	
		// Make sure we don't leave any background threads running
		// This is not reliable, so it is done with native code.
		// Log.d(TAG, "System.exit" );
		// System.exit( 0 );
	}
*/

	// this creates an explicit intent
	public static void sendIntentFromNative( Activity activity, String actionName, String toPackageName,
			String toClassName, String commandStr, String uriStr ) {

		Log.d( TAG, "SendIntentFromNative: action: '" + actionName + "' toPackage: '" + toPackageName + "/" + toClassName + "' command: '" + commandStr + "' uri: '" + uriStr + "'" );
			
		Intent intent = new Intent( actionName );
		if ( toPackageName != null && !toPackageName.isEmpty() ){
			intent.setPackage( toPackageName );
		}

		if ( toPackageName != null && !toPackageName.isEmpty() && toClassName != null && !toClassName.isEmpty() ) {
			// if there is no class name, this is an implicit intent. For launching another app with an
			// action + data, an implicit intent is required (see: http://developer.android.com/training/basics/intents/sending.html#Build)
			// i.e. we cannot send a non-launch intent
			ComponentName cname = new ComponentName( toPackageName, toClassName );
			intent.setComponent( cname );
			Log.d( TAG, "Sending explicit intent: " + cname.flattenToString() );
		}

		if ( uriStr.length() > 0 ) {
			intent.setData( Uri.parse( uriStr ) );
		}

		intent.putExtra( INTENT_KEY_CMD, commandStr );
		intent.putExtra( INTENT_KEY_FROM_PKG, activity.getApplicationContext().getPackageName() );

		try {
			Log.d( TAG, "startActivity " + intent );
			intent.addFlags( Intent.FLAG_ACTIVITY_NO_ANIMATION );
			activity.startActivity( intent );
			activity.overridePendingTransition( 0, 0 );
		}
		catch( ActivityNotFoundException e ) {
			Log.d( TAG, "startActivity " + intent + " not found!" );	
		}
		catch( Exception e ) {
			Log.e( TAG, "sendIntentFromNative threw exception " + e );
		}
	}

	// this gets the launch intent from the specified package name
	public static void sendLaunchIntent( Activity activity, String packageName,
			String commandStr, String uriStr, String actionString ) {

		Log.d( TAG, "sendLaunchIntent: '" + packageName + "' command: '" + commandStr + "' uri: '" + uriStr + "'" + "action: '" + actionString + "'" );

		Intent launchIntent = activity.getPackageManager().getLaunchIntentForPackage( packageName );
		if ( launchIntent == null ) {
			Log.d( TAG, "sendLaunchIntent: null destination activity" );
			return;
		}

		if ( actionString != null && !actionString.isEmpty() ) {
			launchIntent.setAction( actionString );
		}
		
		launchIntent.putExtra( INTENT_KEY_CMD, commandStr );
		launchIntent.putExtra( INTENT_KEY_FROM_PKG, activity.getApplicationContext().getPackageName() );

		if ( uriStr.length() > 0 ) {
			launchIntent.setData( Uri.parse( uriStr ) );
		}

		try {
			Log.d( TAG, "startActivity " + launchIntent );
			launchIntent.addFlags( Intent.FLAG_ACTIVITY_NO_ANIMATION );
			activity.startActivity( launchIntent );
			activity.overridePendingTransition( 0, 0 );
		}
		catch( ActivityNotFoundException e ) {
			Log.d( TAG, "startActivity " + launchIntent + " not found!" );	
		}
		catch( Exception e ) {
			Log.e( TAG, "sendLaunchIntent threw exception " + e );
		}
	}

	public static void finishOnUiThread( final Activity act ) {
    	act.runOnUiThread( new Runnable()
    	{
		 @Override
    		public void run()
    		{
			 	Log.d(TAG, "finishOnUiThread calling finish()" );
    			act.finish();
    			act.overridePendingTransition(0, 0);
            }
    	});
	}
	
	public static void finishAffinityOnUiThread( final Activity act ) {
    	act.runOnUiThread( new Runnable()
    	{
		 @Override
    		public void run()
    		{
			 	Log.d(TAG, "finishAffinityOnUiThread calling finish()" );
				act.finishAffinity();
    			act.overridePendingTransition(0, 0);
            }
    	});
	}

	public static String getInstalledPackagePath( Context context, String packageName )
	{
		final String notInstalled = "";

		Log.d( TAG, "Searching installed packages for '" + packageName + "'" );
		try {
			ApplicationInfo info = context.getPackageManager().getApplicationInfo( packageName, 0 );
			return info.sourceDir != null ? info.sourceDir : notInstalled;
		} catch ( NameNotFoundException e ) {
			Log.w( TAG, "Package '" + packageName + "' not installed", e );
		}

		return notInstalled;
	}

	public static boolean isWifiConnected( final Activity act ) {
		try {
			ConnectivityManager connManager = (ConnectivityManager) act.getSystemService( Context.CONNECTIVITY_SERVICE );
			NetworkInfo mWifi = connManager.getNetworkInfo( ConnectivityManager.TYPE_WIFI );
			return mWifi.isConnected();
		} catch ( Exception e ) {
			Log.d( TAG, "VrActivity::isWifiConnected exception: Make sure android.permission.ACCESS_NETWORK_STATE is set in the manifest." );
		}
		return false;
	}

	public static boolean isHybridApp( final Activity act ) {
		try {
		    ApplicationInfo appInfo = act.getPackageManager().getApplicationInfo(act.getPackageName(), PackageManager.GET_META_DATA);
		    Bundle bundle = appInfo.metaData;
		    String applicationMode = bundle.getString("com.samsung.android.vr.application.mode");
		    return (applicationMode.equals("dual"));
		} catch( NameNotFoundException e ) {
			e.printStackTrace();
		} catch( NullPointerException e ) {
		    Log.e(TAG, "Failed to load meta-data, NullPointer: " + e.getMessage());         
		} 
		
		return false;
	}

	public static String getExternalStorageDirectory() {
		return Environment.getExternalStorageDirectory().getAbsolutePath();
	}
	
	// Converts some thing like "/sdcard" to "/sdcard/", always ends with "/" to indicate folder path
	public static String toFolderPathFormat( String inStr ) {
		if( inStr == null ||
			inStr.length() == 0	)
		{
			return "/";
		}
		
		if( inStr.charAt( inStr.length() - 1 ) != '/' )
		{
			return inStr + "/";
		}
		
		return inStr;
	}
	
	/*** Internal Storage ***/
	public static String getInternalStorageRootDir( Activity act ) {
		return toFolderPathFormat( Environment.getDataDirectory().getPath() );
	}
	
	public static String getInternalStorageFilesDir( Activity act ) {
		if ( act == null ) {
			Log.e( TAG, "Activity is null in getInternalStorageFilesDir method" );
			return null;	// return null because this is a fatal error
		}

		// there have been cases where a faulty installation (adb or Android bug?) caused
		// the data/data/<package>/ folder to not exist, in which case this act.getFilesDir() 
		// will throw an exception.
		try {
			File filesDir = act.getFilesDir();
			if ( filesDir == null ) {
				Log.d( TAG, "App getFilesDir() path does not exist!" );
				return null;
			}
			return toFolderPathFormat( filesDir.getPath() );
		}
		catch( Exception ex ) {
			Log.d( TAG, "Exception: " + ex );
			return null;	// return null because this is a fatal error
		}
	}
	
	public static String getInternalStorageCacheDir( Activity act ) {
		if ( act == null ) {
			Log.e( TAG, "activity is null getInternalStorageCacheDir method" );
			return null;	// return null because this is a fatal error
		}
		// there have been cases where a faulty installation (adb or Android bug?) caused
		// the data/data/<package>/ folder to not exist, in which case this act.getCacheDir() 
		// will throw an exception.
		try {
			File cacheDir = act.getCacheDir();
			if ( cacheDir == null ) {
				Log.d( TAG, "App getCacheDir() path does not exist!" );
				return null;	// return null because this is a fatal error
			}
			return toFolderPathFormat( cacheDir.getPath() );
		}
		catch( Exception ex ) {
			Log.d( TAG, "Exception: " + ex );
			return null;	// return null because this is a fatal error
		}
	}

	public static long getInternalCacheMemoryInBytes( Activity act )
	{
		if ( act != null )
		{
			String path = getInternalStorageCacheDir( act );
			StatFs stat = new StatFs( path );
			return stat.getAvailableBytes();
		}
		else
		{
			Log.e( TAG, "activity is null getInternalCacheMemoryInBytes method" );
		}
		return 0;
	}
	
	/*** External Storage ***/
	public static String getExternalStorageFilesDirAtIdx( Activity act, int idx ) {
		if ( act != null )
		{
			File[] filesDirs = act.getExternalFilesDirs(null);
			if( filesDirs != null && filesDirs.length > idx && filesDirs[idx] != null )
			{
				return toFolderPathFormat( filesDirs[idx].getPath() );
			}
		}
		else
		{
			Log.e( TAG, "activity is null getExternalStorageFilesDirAtIdx method" );
		}
		return "";
	}
	
	public static String getExternalStorageCacheDirAtIdx( Activity act, int idx ) {
		if ( act != null )
		{
			File[] cacheDirs = act.getExternalCacheDirs();
			if( cacheDirs != null && cacheDirs.length > idx && cacheDirs[idx] != null )
			{
				return toFolderPathFormat( cacheDirs[idx].getPath() );
			}
		}
		else
		{
			Log.e( TAG, "activity is null in getExternalStorageCacheDirAtIdx method with index " + idx );
		}
		return "";
	}
	
	// Primary External Storage
	public static final int PRIMARY_EXTERNAL_STORAGE_IDX = 0;
	public static String getPrimaryExternalStorageRootDir( Activity act ) {
		return toFolderPathFormat( Environment.getExternalStorageDirectory().getPath() );
	}
	
	public static String getPrimaryExternalStorageFilesDir( Activity act ) {
		return getExternalStorageFilesDirAtIdx( act, PRIMARY_EXTERNAL_STORAGE_IDX );
	}
	
	public static String getPrimaryExternalStorageCacheDir( Activity act ) {
		return getExternalStorageCacheDirAtIdx( act, PRIMARY_EXTERNAL_STORAGE_IDX );
	}

	// Secondary External Storage
	public static final int SECONDARY_EXTERNAL_STORAGE_IDX = 1;

	// DEPRECATED : No longer valid with Android-M
	public static String getSecondaryExternalStorageRootDir( Activity act ) {
		Log.d( TAG, "getSecondaryExternalStorageRootDir DEPRECATED" );
		return "/storage/extSdCard/";
	}
	
	public static String getSecondaryExternalStorageFilesDir( Activity act ) {
		return getExternalStorageFilesDirAtIdx( act, SECONDARY_EXTERNAL_STORAGE_IDX );
	}
	
	public static String getSecondaryExternalStorageCacheDir( Activity act ) {
		return getExternalStorageCacheDirAtIdx( act, SECONDARY_EXTERNAL_STORAGE_IDX );
	}
}
