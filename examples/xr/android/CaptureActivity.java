package org.ice.xrprobe;

import android.Manifest;
import android.app.NativeActivity;
import android.content.pm.PackageManager;

/** Native OpenXR host; permission UI stays on Android's UI thread. */
public final class CaptureActivity extends NativeActivity {
    private boolean microphoneRequestPending;

    /** Queried by the native owner before an explicit push-to-talk start. */
    public boolean hasMicrophonePermission() {
        return checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
    }

    /** Request only after a controller press; granting never starts capture by itself. */
    public void requestMicrophonePermission() {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                if (!hasMicrophonePermission() && !microphoneRequestPending) {
                    microphoneRequestPending = true;
                    requestPermissions(new String[] {Manifest.permission.RECORD_AUDIO}, 1);
                }
            }
        });
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == 1) microphoneRequestPending = false;
    }
}
