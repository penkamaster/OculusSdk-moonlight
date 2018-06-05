/************************************************************************************

Filename    :   MainActivity.java
Content     :   Media player controller.
Created     :   September 3, 2013
Authors     :	Jim Dose, based on a fork of MainActivity.java from VrVideo by John Carmack.   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/
package com.oculus.cinemasdk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences.Editor;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.media.AudioManager;
import android.media.MediaMetadataRetriever;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceHolder;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;


import com.limelight.LimeLog;
import com.limelight.PcSelector;
import com.limelight.AppSelector;
import com.limelight.StreamInterface;
import com.limelight.nvstream.http.ComputerDetails;
import com.oculus.vrappframework.VrActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import com.oculus.cinemasdk.ModifiableSurfaceHolder;


/*public class MainActivity extends VrActivity implements SurfaceHolder.Callback,
											AudioManager.OnAudioFocusChangeListener*/
public class MainActivity extends VrActivity implements AudioManager.OnAudioFocusChangeListener
{
	public static final String TAG = "Cinema";

	/** Load jni .so on initialization */
	static 
	{
		Log.d( TAG, "LoadLibrary" );
		System.loadLibrary( "cinema" );
	}


	public static native void 			nativeSetVideoSize( long appPtr, int width, int height, int rotation, int duration );
	public static native SurfaceTexture nativePrepareNewVideo( long appPtr );
	public static native long nativeSetAppInterface( VrActivity act, String fromPackageNameString, String commandString, String uriString );

	public static native void nativeDisplayMessage(long appPtr, String text, int time, boolean isError );
	//public static native void nativeAddPc(long appPtr, String name, String s, int pairInt, int reachInt, String uniqueId, boolean b);
	public static native void nativeAddPc(long appPtr, String name, String uuid, int pairState, String binding, int width, int height);
	public static native void nativeRemovePc(long appPtr, String name );
	public static native void nativeAddApp(long appPtr, String appName, String fileName, int appId);
	public static native void nativeRemoveApp(long appPtr, int appId);
	public static native void nativeShowPair(long appPtr, String s);
	public static native void nativePairSuccess(long appPtr);
	public static native void nativeShowError(long appPtr, String message );
	public static native void nativeClearError(long appPtr );

	public static final int MinimumRemainingResumeTime = 60000;	// 1 minute
	public static final int MinimumSeekTimeForResume = 60000;	// 1 minute

	public String 				currentAppName;
	
	boolean				playbackFinished = true;
	boolean				playbackFailed = false;
	
	private boolean 	waitingForSeek = false;
	private boolean 	haveSeekWaiting = false;
	private int 		nextSeekPosition = 0;
	private long       	startTime = 0;
	
	SurfaceTexture 		movieTexture = null;
	Surface 			movieSurface = null;

	StreamInterface 	streamInterface = null;
	AudioManager 		audioManager = null;
	public PcSelector	pcSelector = null;
	public AppSelector	appSelector = null;

	@Override
	protected void onCreate( Bundle savedInstanceState ) 
	{
		Log.d( TAG, "onCreate" );
		super.onCreate( savedInstanceState );

		Intent intent = getIntent();
		String commandString = VrActivity.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrActivity.getPackageStringFromIntent( intent );
		String uriString = VrActivity.getUriStringFromIntent( intent );

		setAppPtr( nativeSetAppInterface( this, fromPackageNameString, commandString, uriString ) );

		audioManager = ( AudioManager )getSystemService( Context.AUDIO_SERVICE );
	}

	@Override
	protected void onDestroy() 
	{
		// Abandon audio focus if we still hold it
		releaseAudioFocus();
		super.onDestroy();
    }

	@Override
	protected void onPause() 
	{
		Log.d( TAG, "onPause()" );
		
		//pauseMovie();

		super.onPause();
	}


	protected void onResume() 
	{
		Log.d( TAG, "onResume()" );
		super.onResume();
	}
	
    public void onAudioFocusChange( int focusChange ) 
    {
		switch( focusChange ) 
		{
		case AudioManager.AUDIOFOCUS_GAIN:
			// resume() if coming back from transient loss, raise stream volume if duck applied
			Log.d( TAG, "onAudioFocusChangedListener: AUDIOFOCUS_GAIN" );
			break;
		case AudioManager.AUDIOFOCUS_LOSS:				// focus lost permanently
			// stop() if isPlaying
			Log.d( TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS" );		
			break;
		case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:	// focus lost temporarily
			// pause() if isPlaying
			Log.d( TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT" );	
			break;
		case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:	// focus lost temporarily
			// lower stream volume
			Log.d( TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK" );		
			break;
		default:
			break;
		}
	}
    
	private int getRotationFromMetadata( final String filePath ) 
	{
		MediaMetadataRetriever metaRetriever = new MediaMetadataRetriever();
		metaRetriever.setDataSource( filePath );
		String value = metaRetriever.extractMetadata( MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION );
		metaRetriever.release();

		if ( value == null ) 
		{
			return 0;
		}

		if ( value.equals( "0" ) ) 
		{
			return 0;
		} 
		else if ( value.equals( "90" ) ) 
		{
			return 90;
		} 
		else if ( value.equals( "180" ) ) 
		{
			return 180;
		} 
		else if ( value.equals( "270" ) ) 
		{
			return 270;
		} 

		return 0;
	}
	
	public void onVideoSizeChanged( int width, int height )
	{
		Log.v( TAG, String.format( "onVideoSizeChanged: %dx%d", width, height ) );
		//int rotation = getRotationFromMetadata( currentMovieFilename );
		//int duration = getDuration();
		nativeSetVideoSize( getAppPtr(), width, height, 0,0);
	}

	private void requestAudioFocus()
	{
		// Request audio focus
		int result = audioManager.requestAudioFocus( this, AudioManager.STREAM_MUSIC,
			AudioManager.AUDIOFOCUS_GAIN );
		if ( result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ) 
		{
			Log.d(TAG,"startMovie(): GRANTED audio focus");
		} 
		else if ( result == AudioManager.AUDIOFOCUS_REQUEST_FAILED ) 
		{
			Log.d(TAG,"startMovie(): FAILED to gain audio focus");
		}				
	}
	
	private void releaseAudioFocus()
	{
		audioManager.abandonAudioFocus( this );
	}

	/*
	 * Whenever we pause or switch to another movie, save the current movie
	 * position so we will return there when the same file is viewed again.
	 */


	private String fileNameFromPathName( String pathname ) 
	{
		File f = new File( pathname );
		return f.getName();
	}
	
	private void Fail( final String message )
	{
		Log.e(TAG, message );
		//TODO mandar el error
		streamInterface.connectionTerminated(0);
		streamInterface = null;
		playbackFinished = true;
		playbackFailed = true;
		releaseAudioFocus();		
	}
	
	// ==================================================================================
	//
	//  Callable from native code
	//
	// ==================================================================================

	public String getExternalCacheDirectory() 
	{
		return getExternalCacheDir().getAbsolutePath();
	}
	
	public boolean createVideoThumbnail( final String compUUID, final int appId, final String outputFilePath, final int width, final int height )
	{
		Log.e( TAG, "Create video thumbnail: noutput path: " + outputFilePath );
		ComputerDetails comp = pcSelector.findByUUID(compUUID);
		Bitmap bmp = appSelector.createAppPoster(comp, appId);

		if ( bmp == null )
		{
			return false;
		}

		float desiredAspectRatio = ( float )width / ( float )height;
		float aspectRatio = ( float )bmp.getWidth() / ( float )bmp.getHeight();
		
		int cropWidth = bmp.getWidth();
		int cropHeight = bmp.getHeight();
		boolean shouldCrop = false;
		
		if ( aspectRatio < desiredAspectRatio )
		{
			cropWidth = bmp.getWidth();
			cropHeight = ( int )( ( float )cropWidth / desiredAspectRatio );
			shouldCrop = true;
		}
		else if ( aspectRatio > desiredAspectRatio )
		{
			cropHeight = bmp.getHeight();
			cropWidth = ( int )( ( float )cropHeight * desiredAspectRatio );
			shouldCrop = true;
		}
		
		if ( shouldCrop )
		{
			int cropX = ( bmp.getWidth() - cropWidth ) / 2;
			int cropY = ( bmp.getHeight() - cropHeight ) / 2;
			
			try 
			{
				Bitmap croppedBmp = Bitmap.createBitmap( bmp, cropX, cropY, cropWidth, cropHeight, new Matrix(), false );
				if ( croppedBmp == null )
				{
					return false;
				}
				
				bmp = croppedBmp;
			}
			
			catch ( Exception e ) 
			{
				Log.e( TAG, "Cropping video thumbnail failed: " + e.getMessage() );
				return false;
			}
		}
		
		boolean failed = false;
		FileOutputStream out = null;
		try 
		{
			int sep = outputFilePath.lastIndexOf( '/' );
			if(sep>0)
			{
				File directory = new File( outputFilePath.substring( 0, sep ) );
				if ( directory.mkdirs() )
				{
					Log.d(TAG, "Created directory: " + directory );
				}
			}
		    out = new FileOutputStream( outputFilePath );
		    bmp.compress( Bitmap.CompressFormat.PNG, 100, out );
		}
		
		catch ( Exception e ) 
		{
			failed = true;
			Log.e( TAG, "Writing video thumbnail failed: " + e.getMessage() );
		}
		
		finally 
		{
		    try 
		    {
		        if ( out != null ) 
		        {
		            out.close();
		        }
		    } 
		    
		    catch( IOException e ) 
		    {
				failed = true;
				Log.e( TAG, "Closing video thumbnail failed: " + e.getMessage() );
		    }
		}	
		
		if ( !failed )
		{
			Log.e( TAG, "Wrote " + outputFilePath );
		}
		
		return !failed;
	}



	public boolean isPlaying()
	{
		if ( streamInterface != null )
		{
			return streamInterface.isConnected();
		}
		return false;
	}

	public boolean isPlaybackFinished()
	{
		return playbackFinished;
	}
	
	public boolean hadPlaybackError()
	{
		return playbackFailed;
	}

	public int addPCbyIP(final String IP)
	{
		return pcSelector.addPCbyIP(IP);
	}


	public void startMovie( final String uuid, final String appName, final int appId, final String binder, final int width, final int height, final int fps, final boolean hostAudio )
	{
		// set playbackFinished and playbackFailed to false immediately so it's set when we return to native
		playbackFinished = false;
		playbackFailed = false;

    	runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
			 	startMovieLocal(  uuid, appName, appId, binder, width, height, fps, hostAudio);
			}
    	} );
	}

	private void startMovieLocal( final String uuid, final String appName, int appId, final String binder, int width, int height, int fps, boolean hostAudio )
	{
		Log.v(TAG, "startMovie " + appName + " on " + uuid );
		
		synchronized( this ) 
		{
			requestAudioFocus();
	
			playbackFinished = false;
			playbackFailed = false;
			
			waitingForSeek = false;
			haveSeekWaiting = false;
			nextSeekPosition = 0;

			currentAppName = appName;
			
			// Have native code pause any playing movie,
			// allocate a new external texture,
			// and create a surfaceTexture with it.
			movieTexture = nativePrepareNewVideo( getAppPtr() );
			movieSurface = new Surface( movieTexture );

			if (streamInterface != null)
			{
				streamInterface.stop();
				streamInterface = null;
			}

			Log.v( TAG, "StreamInterface create" );

			ModifiableSurfaceHolder surfaceHolder = new ModifiableSurfaceHolder();
			surfaceHolder.setSurface(movieSurface);
			streamInterface = new StreamInterface(this, uuid, currentAppName, appId, binder, surfaceHolder, width, height, fps, hostAudio );
			streamInterface.surfaceCreated(surfaceHolder);
			// Manually poke this - originally the movie player would call it

			onVideoSizeChanged(width,height);

			// Save the current movie now that it was successfully started
			Editor edit = getPreferences( MODE_PRIVATE ).edit();
			edit.putString( "currentAppName", currentAppName );
			edit.commit();
		}
		
		Log.v( TAG, "exiting startMovie" );
	}



	public void stopMovie()
	{
		Log.v( TAG, "stopMovie" );
		
		synchronized (this) 
		{
			if ( streamInterface != null )
			{
				streamInterface.stop();
				streamInterface = null;
			}
			releaseAudioFocus();
			
			playbackFailed = false;
			playbackFinished = true;
		}
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
			if(streamInterface != null && streamInterface.isConnected())
			{
				boolean ret = streamInterface.handleMotionEvent(event);
				if (ret) return true;
			}

		}
		return super.dispatchGenericMotionEvent(event);
	}

	public boolean dispatchKeyEvent(KeyEvent event) {

		if ((event.getSource() & ( InputDevice.SOURCE_GAMEPAD | InputDevice.SOURCE_KEYBOARD )) != 0) {
			Log.e("INPUT dispatchKeyEvent GAMEPAD OR KEYBOARD", "KeyEvent source: " + event.getSource());
			if(streamInterface != null && streamInterface.isConnected())
			{
				boolean ret = false;
				if (event.getAction() == KeyEvent.ACTION_DOWN)
					ret = streamInterface.onKeyDown(event.getKeyCode(), event);
				else if (event.getAction() == KeyEvent.ACTION_UP)
					ret = streamInterface.onKeyUp(event.getKeyCode(), event);
				if (ret) return true;
			}
		}else{
			Log.e("INPUT dispatchKeyEvent OTRO", "KeyEvent source: " + event.getSource());
		}
		return super.dispatchKeyEvent(event);
	}


	public void initPcSelector()
	{
		if(pcSelector != null) return;
		pcSelector = new PcSelector(this);
	}

	public void pairPc(final String compUUID)
	{
		pcSelector.pairWithUUID(compUUID);
	}

	public int getPcPairState(final String compUUID)
	{
		return pcSelector.pairStateFromUUID(compUUID);
	}

	public int getPcState(final String compUUID)
	{
		return pcSelector.stateFromUUID(compUUID);
	}

	public int getPcReachability(final String compUUID)
	{
		return pcSelector.reachabilityStateFromUUID(compUUID);
	}

	/*
	 *	Functions for App selection
	 */
	public void initAppSelector(final String computerUUID)
	{
		appSelector = new AppSelector(this, computerUUID);
	}

	public void mouseMove( int deltaX, int deltaY)
	{
		if (streamInterface!=null)
			streamInterface.mouseMove(deltaX, deltaY);

	}

	public void mouseClick(int buttonId, boolean down)
	{
		if (streamInterface!=null)
			streamInterface.mouseButtonEvent(buttonId, down);

	}

	public void mouseScroll( byte amount)
	{
		streamInterface.mouseScroll(amount);
	}


}
