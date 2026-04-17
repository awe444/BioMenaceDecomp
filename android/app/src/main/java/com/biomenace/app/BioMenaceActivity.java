package com.biomenace.app;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

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
        copyAssetsIfNeeded();
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
