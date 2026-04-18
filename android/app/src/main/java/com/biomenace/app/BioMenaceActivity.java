package com.biomenace.app;

import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * BioMenace main activity — extends SDL2's SDLActivity which handles
 * the native library loading, surface management, and input events.
 *
 * On first launch (or when assets are missing) the game data files
 * bundled in the APK's assets/ folder are copied to internal storage
 * so the native C code can access them with standard open() calls.
 */
public class BioMenaceActivity extends SDLActivity {

    private static final String TAG = "BioMenace";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        copyAssetsIfNeeded();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    /**
     * Enter immersive sticky fullscreen — hides both the status bar
     * and navigation bar so the game fills the entire display.
     */
    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // API 30+ (Android 11): use WindowInsetsController
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars()
                              | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            // API 19+ (Android 4.4): use system UI flags
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
              | View.SYSTEM_UI_FLAG_FULLSCREEN
              | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
              | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
              | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
              | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "main"
        };
    }

    /**
     * Copy every file from the APK assets/ directory into the app's
     * internal files directory so the game's open() calls can find them.
     * Files that already exist are skipped.
     */
    private void copyAssetsIfNeeded() {
        AssetManager am = getAssets();
        File destDir = getFilesDir();

        try {
            String[] files = am.list("");
            if (files == null) return;

            for (String filename : files) {
                // Skip directories and Android-internal assets
                if (filename.startsWith("images")
                        || filename.startsWith("sounds")
                        || filename.startsWith("webkit")
                        || filename.equals("databases")) {
                    continue;
                }

                File destFile = new File(destDir, filename);
                if (destFile.exists()) continue;

                try (InputStream in = am.open(filename);
                     OutputStream out = new FileOutputStream(destFile)) {
                    byte[] buf = new byte[8192];
                    int len;
                    while ((len = in.read(buf)) > 0) {
                        out.write(buf, 0, len);
                    }
                    Log.i(TAG, "Copied asset: " + filename);
                } catch (IOException e) {
                    // Not a file (probably a directory) — skip
                    Log.d(TAG, "Skipped asset: " + filename);
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to list assets", e);
        }
    }
}
