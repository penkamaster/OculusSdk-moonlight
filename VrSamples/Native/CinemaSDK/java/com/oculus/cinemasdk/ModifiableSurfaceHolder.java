package com.oculus.cinemasdk;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

public class ModifiableSurfaceHolder implements SurfaceHolder {
	private static final String TAG = "ModifiableSurfaceHolder";
	private Surface surface = null;
	
	public void setSurface(Surface newSurface)
	{
		surface = newSurface;
	}
	
	@Override
	public void addCallback(Callback callback) {
		// TODO Auto-generated method stub
		if(surface == null) return;
		

	}

	@Override
	public void removeCallback(Callback callback) {
		Log.e(TAG, "removeCallback");
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean isCreating() {
		Log.e(TAG, "isCreating");
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	@Deprecated
	public void setType(int type) {
		Log.e(TAG, "setType");
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setFixedSize(int width, int height) {
		Log.e(TAG, "setFixedSize");
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setSizeFromLayout() {
		Log.e(TAG, "setSizeFromLayout");
		// TODO Auto-generated method stub
		
	}

	@Override
	public void setFormat(int format) {
		Log.e(TAG, "setFormat");
		// TODO WTF how do I even?
	}

	@Override
	public void setKeepScreenOn(boolean screenOn) {
		Log.e(TAG, "setKeepScreenOn");
		// TODO Auto-generated method stub
		
	}

	@Override
	public Canvas lockCanvas() {
		Log.v(TAG, "lockCanvas");
		if(surface != null)
		{
			return surface.lockCanvas(null);
		}
		return null;
	}

	@Override
	public Canvas lockCanvas(Rect dirty) {
		Log.v(TAG, "lockCanvas");
		if(surface != null)
		{
			return surface.lockCanvas(dirty);
		}
		return null;
	}

	@Override
	public void unlockCanvasAndPost(Canvas canvas) {
		Log.v(TAG, "unlockCanvasAndPost");
		if(surface != null)
		{
			surface.unlockCanvasAndPost(canvas);
		}
	}

	@Override
	public Rect getSurfaceFrame() {
		Log.v(TAG, "getSurfaceFrame");
		if (surface != null)
		{
			Canvas canvas = surface.lockCanvas(null);
			Rect frame = new Rect(0,0,canvas.getWidth(),canvas.getHeight());
			surface.unlockCanvasAndPost(canvas);
			return frame;
		}
		return null;
	}

	@Override
	public Surface getSurface() {
		Log.v(TAG, "getSurface");
		return surface;
	}

}
