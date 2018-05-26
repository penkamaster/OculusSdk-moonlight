/************************************************************************************

Filename    :   VrApp.java
Content     :   Lifecycle manager used by the application framework.
Created     :   5/13/2015
Authors     :   John Carmack, Rob Arnold

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculus.vrappframework;

import android.app.Activity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;

// Class that manages the lifecycle of the native application without
// necessitating that everyone use VrActivity.
public class VrApp implements SurfaceHolder.Callback {
    private static final String TAG = "VrApp";

    private static native void nativeOnCreate(Activity act);
    private static native void nativeOnResume(long appPtr);
    private static native void nativeOnPause(long appPtr);
    private static native void nativeOnDestroy(long appPtr);
    private static native void nativeSurfaceCreated(long appPtr, Surface s);
    private static native void nativeSurfaceChanged(long appPtr, Surface s);
    private static native void nativeSurfaceDestroyed(long appPtr);

    // Raw pointer to the App's OVR::AppLocal implementation
    private long mAppPtr;
    // While there is a valid surface holder, onDestroy can destroy the surface
    // when surfaceDestroyed is not called before onDestroy.
    private SurfaceHolder mSurfaceHolder;

    public VrApp(Activity activity, long appPtr) {
        mAppPtr = appPtr;

        // We do not use the Activity's Surface, but instead make our own.
		final SurfaceView sv = new SurfaceView( activity );
		activity.setContentView( sv );
		sv.getHolder().addCallback( this );
    }

    long getAppPtr() {
        return mAppPtr;
    }

    public static void onCreate(Activity activity) {
      nativeOnCreate( activity );
    }

    public void onResume() {
        nativeOnResume( mAppPtr );
    }

    public void onPause() {
        nativeOnPause( mAppPtr );
    }

    public void onDestroy() {
      if ( mSurfaceHolder != null ) {
          nativeSurfaceDestroyed( mAppPtr );
      }

      nativeOnDestroy( mAppPtr );
      mAppPtr = 0L;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, this + " surfaceCreated()");
        if ( mAppPtr != 0 ) {
            nativeSurfaceCreated( mAppPtr, holder.getSurface() );
            mSurfaceHolder = holder;
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG, this + " surfaceChanged() format: " + format + " width: " + width + " height: " + height);
        if ( mAppPtr != 0 ) {
            nativeSurfaceChanged( mAppPtr, holder.getSurface() );
            mSurfaceHolder = holder;
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, this + " surfaceDestroyed()");
        if ( mAppPtr != 0 ) {
            nativeSurfaceDestroyed( mAppPtr );
            mSurfaceHolder = null;
        }
    }
}
