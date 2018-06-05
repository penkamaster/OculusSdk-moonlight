package com.limelight;



import com.oculus.cinemasdk.R;
import com.oculus.cinemasdk.MainActivity;
import java.io.FileNotFoundException;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import com.limelight.LimeLog;
import com.limelight.binding.PlatformBinding;
import com.limelight.binding.crypto.AndroidCryptoProvider;
import com.limelight.computers.ComputerManagerListener;
import com.limelight.computers.ComputerManagerService;
import com.limelight.nvstream.http.ComputerDetails;
import com.limelight.nvstream.http.NvApp;
import com.limelight.nvstream.http.NvHTTP;
import com.limelight.nvstream.http.PairingManager;
import com.limelight.nvstream.http.PairingManager.PairState;
import com.limelight.nvstream.wol.WakeOnLanSender;
import com.limelight.preferences.AddComputerManually;
import com.limelight.preferences.GlPreferences;
import com.limelight.preferences.PreferenceConfiguration;
import com.limelight.preferences.StreamSettings;
import com.limelight.ui.AdapterFragment;
import com.limelight.ui.AdapterFragmentCallbacks;
import com.limelight.utils.Dialog;
import com.limelight.utils.HelpLauncher;
import com.limelight.utils.ServerHelper;
import com.limelight.utils.ShortcutHelper;
import com.limelight.utils.UiHelper;

import android.app.Activity;
import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.preference.PreferenceManager;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View.OnClickListener;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.widget.Toast;
import android.widget.AdapterView.AdapterContextMenuInfo;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class PcSelector {
    private MainActivity activity;
    private List<ComputerDetails> compList;

    private ShortcutHelper shortcutHelper;
    private ComputerManagerService.ComputerManagerBinder managerBinder;
    private boolean freezeUpdates, runningPolling, inForeground, completeOnCreateCalled;
    private final ServiceConnection serviceConnection = new ServiceConnection() {
        public void onServiceConnected(ComponentName className, IBinder binder) {
            final ComputerManagerService.ComputerManagerBinder localBinder =
                    ((ComputerManagerService.ComputerManagerBinder)binder);

            new Thread() {
                @Override
                public void run() {
                    // Wait for the binder to be ready
                    localBinder.waitForReady();

                    // Now make the binder visible
                    managerBinder = localBinder;

                    // Start updates
                    startComputerUpdates();

                    // Force a keypair to be generated early to avoid discovery delays
                    new AndroidCryptoProvider(activity).getClientCertificate();
                }
            }.start();
        }

        public void onServiceDisconnected(ComponentName className) {
            managerBinder = null;
        }
    };



    /*
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
    
            // Assume we're in the foreground when created to avoid a race
            // between binding to CMS and onResume()
            inForeground = true;
    
            // Create a GLSurfaceView to fetch GLRenderer unless we have
            // a cached result already.
            final GlPreferences glPrefs = GlPreferences.readPreferences(this);
            if (!glPrefs.savedFingerprint.equals(Build.FINGERPRINT) || glPrefs.glRenderer.isEmpty()) {
                GLSurfaceView surfaceView = new GLSurfaceView(this);
                surfaceView.setRenderer(new GLSurfaceView.Renderer() {
                    @Override
                    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
                        // Save the GLRenderer string so we don't need to do this next time
                        glPrefs.glRenderer = gl10.glGetString(GL10.GL_RENDERER);
                        glPrefs.savedFingerprint = Build.FINGERPRINT;
                        glPrefs.writePreferences();
    
                        LimeLog.info("Fetched GL Renderer: " + glPrefs.glRenderer);
    
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                completeOnCreate();
                            }
                        });
                    }
    
                    @Override
                    public void onSurfaceChanged(GL10 gl10, int i, int i1) {
                    }
    
                    @Override
                    public void onDrawFrame(GL10 gl10) {
                    }
                });
                setContentView(surfaceView);
            }
            else {
                LimeLog.info("Cached GL Renderer: " + glPrefs.glRenderer);
                completeOnCreate();
            }
        }
    */
    public PcSelector(MainActivity creatingActivity) {
        activity = creatingActivity;

        completeOnCreateCalled = true;

        //shortcutHelper = new ShortcutHelper(this);

        //UiHelper.setLocale(this);

        // Bind to the computer manager service
        activity.bindService(new Intent(activity, ComputerManagerService.class), serviceConnection,
                Service.BIND_AUTO_CREATE);
        compList = new ArrayList<ComputerDetails>();


        //PreferenceManager.setDefaultValues(creatingActivity, R.xml.preferences, false);
    }

    private void startComputerUpdates() {
        // Only allow polling to start if we're bound to CMS, polling is not already running,
        // and our activity is in the foreground.
        if (managerBinder != null && !runningPolling ) {
            freezeUpdates = false;
            managerBinder.startPolling(new ComputerManagerListener() {
                @Override
                public void notifyComputerUpdated(final ComputerDetails details) {
                    if (!freezeUpdates) {
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                updateComputer(details);
                            }
                        });
                    }
                }
            });
            runningPolling = true;
        }
    }

    private void stopComputerUpdates(boolean wait) {
        if (managerBinder != null) {
            if (!runningPolling) {
                return;
            }

            freezeUpdates = true;

            managerBinder.stopPolling();

            if (wait) {
                managerBinder.waitForPollingStopped();
            }

            runningPolling = false;
        }
    }

    public ComputerDetails findByUUID(String compUUID)
    {
        int i = 0;
        for (; i< compList.size(); i++)
        {
            if(compList.get(i).uuid.equals(UUID.fromString(compUUID)))
            {
                return compList.get(i);
            }

        }
        return null;
    }


    static final int PS_NOT_PAIRED = 0;
    static final int PS_PAIRED = 1;
    static final int PS_PIN_WRONG = 2;
    static final int PS_FAILED = 3;
    public int pairStateFromUUID(final String compUUID)
    {
        ComputerDetails comp = findByUUID(compUUID);
        if(comp != null && comp.pairState != null)
        {
            if(comp.pairState == PairingManager.PairState.NOT_PAIRED)		return PS_NOT_PAIRED;
            else if(comp.pairState == PairingManager.PairState.PAIRED)		return PS_PAIRED;
            else if(comp.pairState == PairingManager.PairState.PIN_WRONG)	return PS_PIN_WRONG;
            else return PS_FAILED;
        }
        return PS_FAILED;
    }

    static final int CS_ONLINE = 0;
    static final int CS_OFFLINE = 1;
    static final int CS_UNKNOWN = 2;
    public int stateFromUUID(final String compUUID)
    {
        ComputerDetails comp = findByUUID(compUUID);
        if(comp != null)
        {
            if(comp.state == ComputerDetails.State.ONLINE)	return CS_ONLINE;
            else if(comp.state == ComputerDetails.State.OFFLINE)	return CS_OFFLINE;
            else return CS_UNKNOWN;
        }
        return CS_UNKNOWN;
    }

    static final int RS_LOCAL = 0;
    static final int RS_REMOTE = 1;
    static final int RS_OFFLINE = 2;
    static final int RS_UNKNOWN = 3;
    public int reachabilityStateFromUUID(final String compUUID)
    {
        ComputerDetails comp = findByUUID(compUUID);
        if(comp != null && comp.reachability != null)
        {
            return comp.reachability.ordinal();
        }
        return RS_UNKNOWN;
    }

    public void pairWithUUID(final String compUUID)
    {
        doPair(findByUUID(compUUID));
    }




    private void doPair(final ComputerDetails computer) {
        if (computer.reachability == ComputerDetails.Reachability.OFFLINE) {
            MainActivity.nativeShowError(activity.getAppPtr(), activity.getResources().getString(R.string.pair_pc_offline));
            return;
        }
        if (computer.runningGameId != 0) {
            MainActivity.nativeShowError(activity.getAppPtr(), activity.getResources().getString(R.string.pair_pc_ingame));
            return;
        }
        if (managerBinder == null) {
            MainActivity.nativeShowError(activity.getAppPtr(), activity.getResources().getString(R.string.error_manager_not_running));
            return;
        }

        MainActivity.nativeShowError(activity.getAppPtr(), activity.getResources().getString(R.string.pairing));
        new Thread(new Runnable() {
            @Override
            public void run() {
                NvHTTP httpConn;
                String message;
                boolean success = false;
                try {
                    // Stop updates and wait while pairing
                    stopComputerUpdates(true);

                    httpConn = new NvHTTP(ServerHelper.getCurrentAddressFromComputer(computer),
                            managerBinder.getUniqueId(),
                            PlatformBinding.getDeviceName(),
                            PlatformBinding.getCryptoProvider(activity));
                    if (httpConn.getPairState() == PairingManager.PairState.PAIRED) {
                        // Don't display any toast, but open the app list
                        message = null;
                        success = true;
                    }
                    else {
                        final String pinStr = PairingManager.generatePinString();

                        // Spin the dialog off in a thread because it blocks


                        MainActivity.nativeShowPair(activity.getAppPtr(), activity.getResources().getString(R.string.pair_pairing_msg) + " " + pinStr);

                        PairingManager.PairState pairState = httpConn.pair(httpConn.getServerInfo(), pinStr);
                        if (pairState == PairingManager.PairState.PIN_WRONG) {
                            message = activity.getResources().getString(R.string.pair_incorrect_pin);
                        }
                        else if (pairState == PairingManager.PairState.FAILED) {
                            message = activity.getResources().getString(R.string.pair_fail);
                        }
                        else if (pairState == PairingManager.PairState.ALREADY_IN_PROGRESS) {
                            message = activity.getResources().getString(R.string.pair_already_in_progress);
                        }
                        else if (pairState == PairingManager.PairState.PAIRED) {
                            // Just navigate to the app view without displaying a toast
                            message = null;
                            success = true;

                            // Invalidate reachability information after pairing to force
                            // a refresh before reading pair state again
                            managerBinder.invalidateStateForComputer(computer.uuid);
                        }
                        else {
                            // Should be no other values
                            message = null;
                        }
                    }
                } catch (UnknownHostException e) {
                    message = activity.getResources().getString(R.string.error_unknown_host);
                } catch (FileNotFoundException e) {
                    message = activity.getResources().getString(R.string.error_404);
                } catch (Exception e) {
                    e.printStackTrace();
                    message = e.getMessage();
                }

                if (success == true)
                    MainActivity.nativePairSuccess( activity.getAppPtr() );

                if (message != null)
                {
                    MainActivity.nativeShowError(activity.getAppPtr(), message);
                }
                // Start polling again
                startComputerUpdates();

            }
        }).start();
    }

    public int addPCbyIP(final String host)
    {

        //InetAddress addr = InetAddress.getByName(host);

        if (!managerBinder.addComputerBlocking( host,true))
        {
            return 1;
        }
        else
        {
            return 0;
        }

    }

    private void updateComputer(ComputerDetails details) {
        //prefConfig = PreferenceConfiguration.readPreferences(activity);

        //prefConfig.width = 1920;
        //prefConfig.height = 1080;

        int i = 0;
        boolean found = false;
        for (; i< compList.size(); i++)
        {
            if(compList.get(i).uuid == details.uuid)
            {
                found = true;
                break;
            }

        }
        if(found)
            compList.set(i, details);
        else
            compList.add(details);

        LimeLog.info("Found PC " + details.toString());
        int pairInt=0;
        if(details.pairState == PairingManager.PairState.NOT_PAIRED)		pairInt= PS_NOT_PAIRED;
        else if(details.pairState == PairingManager.PairState.PAIRED)		pairInt= PS_PAIRED;
        else if(details.pairState == PairingManager.PairState.PIN_WRONG)	pairInt= PS_PIN_WRONG;
        else pairInt= PS_FAILED;

        int reachInt=0;
        if(details.reachability == ComputerDetails.Reachability.LOCAL) reachInt = RS_LOCAL;
        else if(details.reachability == ComputerDetails.Reachability.REMOTE) reachInt = RS_REMOTE;
        else if(details.reachability == ComputerDetails.Reachability.OFFLINE) reachInt = RS_OFFLINE;
        else reachInt = RS_UNKNOWN;
        //MainActivity.nativeAddPc(activity.getAppPtr(), details.name, details.uuid.toString(), pairInt, reachInt, managerBinder.getUniqueId(), details.runningGameId != 0);
        MainActivity.nativeAddPc(activity.getAppPtr(), details.name, details.uuid.toString(), pairInt, managerBinder.getUniqueId(),1920,1080);
    }

    public void closeApp(final String compUUID)
    {
        ComputerDetails comp = findByUUID(compUUID);
        if(comp != null)
        {
            ServerHelper.doQuit(activity,
                    ServerHelper.getCurrentAddressFromComputer(comp),
                    new NvApp(), managerBinder, null);
        }
    }


    public class ComputerObject {
        public ComputerDetails details;

        public ComputerObject(ComputerDetails details) {
            if (details == null) {
                throw new IllegalArgumentException("details must not be null");
            }
            this.details = details;
        }

        @Override
        public String toString() {
            return details.name;
        }
    }

    /*@Override
    public int getAdapterFragmentLayoutId() {
        return PreferenceConfiguration.readPreferences(activity).listMode ?
                R.layout.list_view : (PreferenceConfiguration.readPreferences(activity).smallIconMode ?
                R.layout.pc_grid_view_small : R.layout.pc_grid_view);
        return 0;
    }

    @Override
    public void receiveAbsListView(AbsListView listView) {

        listView.setAdapter(pcGridAdapter);
        listView.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> arg0, View arg1, int pos,
                                    long id) {
                ComputerObject computer = (ComputerObject) pcGridAdapter.getItem(pos);
                if (computer.details.reachability == ComputerDetails.Reachability.UNKNOWN ||
                        computer.details.reachability == ComputerDetails.Reachability.OFFLINE) {
                    // Open the context menu if a PC is offline or refreshing
                    openContextMenu(arg1);
                } else if (computer.details.pairState != PairState.PAIRED) {
                    // Pair an unpaired machine by default
                    doPair(computer.details);
                } else {
                    doAppList(computer.details);
                }
            }
        });
        registerForContextMenu(listView);

        String a = "";
    }
    */

}
