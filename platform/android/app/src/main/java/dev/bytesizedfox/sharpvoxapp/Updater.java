package dev.bytesizedfox.sharpvoxapp;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.core.content.FileProvider;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class Updater {
    private static final String RELEASE_URL = "https://github.com/dectalk/SharpVoxTTS/releases/download/latest/";
    private static final String VERSION_FILE = "version.json";
    private static final String DEFAULT_APK = "SharpVoxTTS-release.apk";

    private static final long AUTO_CHECK_INTERVAL_MS = 6L * 60L * 60L * 1000L;
    private static final String PREFS = "updater";
    private static final String PREF_LAST_CHECK = "last_check";

    private static final ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();

    private Updater() {
    }

    private static final class ReleaseInfo {
        long versionCode;
        String versionName = "";
        String commit = "";
        String apk = DEFAULT_APK;
        String sha256 = "";
    }

    public static void autoCheck(Activity activity) {
        SharedPreferences pref = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        long lastCheck = pref.getLong(PREF_LAST_CHECK, 0L);
        if (System.currentTimeMillis() - lastCheck < AUTO_CHECK_INTERVAL_MS) {
            return;
        }
        check(activity, false);
    }

    public static void check(Activity activity, boolean userInitiated) {
        activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                .putLong(PREF_LAST_CHECK, System.currentTimeMillis())
                .apply();

        EXECUTOR.execute(() -> {
            try {
                ReleaseInfo info = fetchReleaseInfo();
                long installed = installedVersionCode(activity);
                if (info.versionCode > installed) {
                    activity.runOnUiThread(() -> {
                        if (!activity.isFinishing()) {
                            showUpdateDialog(activity, info, installed);
                        }
                    });
                } else if (userInitiated) {
                    activity.runOnUiThread(() -> toast(activity, "SharpVox is up to date"));
                }
            } catch (Exception e) {
                if (userInitiated) {
                    activity.runOnUiThread(() -> toast(activity, "Update check failed: " + e.getMessage()));
                }
            }
        });
    }

    public static long installedVersionCode(Context context) {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return info.getLongVersionCode();
        } catch (Exception e) {
            return 0L;
        }
    }

    private static ReleaseInfo fetchReleaseInfo() throws Exception {
        String url = RELEASE_URL + VERSION_FILE + "?t=" + System.currentTimeMillis();
        JSONObject json = new JSONObject(readText(url));

        ReleaseInfo info = new ReleaseInfo();
        info.versionCode = json.getLong("versionCode");
        info.versionName = json.optString("versionName", "");
        info.commit = json.optString("commit", "");
        info.apk = json.optString("apk", DEFAULT_APK);
        info.sha256 = json.optString("sha256", "").toLowerCase(Locale.ROOT);

        if (info.versionCode <= 0) {
            throw new IOException("Invalid version in " + VERSION_FILE);
        }
        return info;
    }

    private static String readText(String url) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(15000);
        conn.setRequestProperty("User-Agent", "SharpVox-Android");
        conn.setRequestProperty("Cache-Control", "no-cache");
        try {
            int code = conn.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + code);
            }
            try (InputStream in = conn.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                return out.toString("UTF-8");
            }
        } finally {
            conn.disconnect();
        }
    }

    private static void showUpdateDialog(Activity activity, ReleaseInfo info, long installed) {
        String message = "Installed build: " + installed
                + "\nAvailable build: " + info.versionCode
                + (info.commit.isEmpty() ? "" : " (" + info.commit + ")");

        new AlertDialog.Builder(activity)
                .setTitle("SharpVox update available")
                .setMessage(message)
                .setPositiveButton("Download", (dialog, which) -> download(activity, info))
                .setNegativeButton("Later", null)
                .show();
    }

    private static void download(Activity activity, ReleaseInfo info) {
        toast(activity, "Downloading update...");
        EXECUTOR.execute(() -> {
            try {
                File apk = downloadApk(activity, info);
                activity.runOnUiThread(() -> {
                    if (!activity.isFinishing()) {
                        install(activity, apk);
                    }
                });
            } catch (Exception e) {
                activity.runOnUiThread(() -> toast(activity, "Download failed: " + e.getMessage()));
            }
        });
    }

    private static File downloadApk(Context context, ReleaseInfo info) throws Exception {
        // The asset name comes from the network, so only ever use its bare file name.
        String apkName = new File(info.apk).getName();
        if (!apkName.endsWith(".apk")) {
            throw new IOException("Unexpected asset name: " + info.apk);
        }

        File dir = new File(context.getCacheDir(), "updates");
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("Cannot create update directory");
        }

        File target = new File(dir, apkName);
        File partial = new File(dir, apkName + ".part");

        String url = RELEASE_URL + apkName + "?t=" + System.currentTimeMillis();
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setConnectTimeout(20000);
        conn.setReadTimeout(30000);
        conn.setRequestProperty("User-Agent", "SharpVox-Android");

        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try {
            int code = conn.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + code);
            }
            try (InputStream in = conn.getInputStream();
                 FileOutputStream out = new FileOutputStream(partial)) {
                byte[] buf = new byte[64 * 1024];
                int len;
                while ((len = in.read(buf)) > 0) {
                    digest.update(buf, 0, len);
                    out.write(buf, 0, len);
                }
            }
        } finally {
            conn.disconnect();
        }

        String actual = toHex(digest.digest());
        if (!info.sha256.isEmpty() && !info.sha256.equals(actual)) {
            partial.delete();
            throw new IOException("Checksum mismatch");
        }

        if (target.exists() && !target.delete()) {
            partial.delete();
            throw new IOException("Cannot replace previous download");
        }
        if (!partial.renameTo(target)) {
            throw new IOException("Cannot finalize download");
        }
        return target;
    }

    private static void install(Activity activity, File apk) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !activity.getPackageManager().canRequestPackageInstalls()) {
            new AlertDialog.Builder(activity)
                    .setTitle("Allow app installs")
                    .setMessage("Android needs your permission to install the update. "
                            + "Enable \"Allow from this source\" for SharpVox, then try again.")
                    .setPositiveButton("Open settings", (dialog, which) -> {
                        Intent intent = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                Uri.parse("package:" + activity.getPackageName()));
                        activity.startActivity(intent);
                    })
                    .setNegativeButton("Cancel", null)
                    .show();
            return;
        }

        Uri uri = FileProvider.getUriForFile(
                activity,
                activity.getApplicationContext().getPackageName() + ".fileprovider",
                apk
        );

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(uri, "application/vnd.android.package-archive");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        activity.startActivity(intent);
    }

    private static String toHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            sb.append(String.format(Locale.ROOT, "%02x", b));
        }
        return sb.toString();
    }

    private static void toast(Activity activity, String message) {
        Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
    }
}
