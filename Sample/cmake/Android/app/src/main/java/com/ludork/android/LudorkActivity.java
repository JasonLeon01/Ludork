package com.ludork.android;

import android.app.NativeActivity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class LudorkActivity extends NativeActivity {
    private static final String TAG = "Ludork";
    private static final String MANIFEST_ASSET = "ludork-runtime-manifest.json";
    private static final String COMPLETE_MARKER = ".complete";
    private static final int BUFFER_SIZE = 64 * 1024;

    private static final class RuntimeFile {
        final String path;
        final long size;
        final String sha256;

        RuntimeFile(String path, long size, String sha256) {
            this.path = path;
            this.size = size;
            this.sha256 = sha256;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            prepareRuntime();
            writeSystemLocale();
        } catch (Exception exception) {
            Log.e(TAG, "Unable to prepare the Ludork Android runtime", exception);
            throw new IllegalStateException("Unable to prepare the Ludork Android runtime", exception);
        }
        super.onCreate(savedInstanceState);
    }

    private void prepareRuntime() throws Exception {
        JSONObject manifest = new JSONObject(readAssetText(MANIFEST_ASSET));
        if (manifest.getInt("version") != 1) {
            throw new IOException("Unsupported Ludork runtime manifest version");
        }
        String runtimeHash = manifest.getString("hash");
        requireSha256(runtimeHash, "runtime hash");
        JSONArray values = manifest.getJSONArray("files");
        if (values.length() == 0) {
            throw new IOException("The Ludork runtime manifest is empty");
        }
        List<RuntimeFile> files = new ArrayList<>(values.length());
        String previousPath = "";
        boolean hasAssets = false;
        boolean hasData = false;
        boolean hasScripts = false;
        boolean hasEntry = false;
        for (int index = 0; index < values.length(); ++index) {
            JSONObject value = values.getJSONObject(index);
            String path = value.getString("path");
            long size = value.getLong("size");
            String sha256 = value.getString("sha256");
            requireRelativeAssetPath(path);
            requireSha256(sha256, path);
            if (size < 0 || (!previousPath.isEmpty() && path.compareTo(previousPath) <= 0)) {
                throw new IOException("Invalid or unsorted runtime manifest entry: " + path);
            }
            hasAssets |= path.startsWith("Assets/");
            hasData |= path.startsWith("Data/");
            hasScripts |= path.startsWith("Scripts/");
            hasEntry |= path.equals("Scripts/Entry.lua") || path.equals("Scripts/Entry.luac");
            files.add(new RuntimeFile(path, size, sha256));
            previousPath = path;
        }
        if (!hasAssets || !hasData || !hasScripts || !hasEntry) {
            throw new IOException("The Ludork runtime manifest is incomplete");
        }
        if (!runtimeHash.equals(runtimeManifestDigest(files))) {
            throw new IOException("The Ludork runtime manifest hash is invalid");
        }

        File ludorkRoot = new File(getFilesDir(), "ludork");
        requireDirectory(ludorkRoot);
        requireDirectory(new File(ludorkRoot, "user-data"));
        File runtimeRoot = new File(ludorkRoot, "runtime");
        requireDirectory(runtimeRoot);
        File destination = new File(runtimeRoot, runtimeHash);
        File marker = new File(destination, COMPLETE_MARKER);
        if (destination.isDirectory() && runtimeHash.equals(readSmallText(marker))) {
            removeOtherRuntimeDirectories(runtimeRoot, destination);
            return;
        }

        File temporary = new File(runtimeRoot, "." + runtimeHash + ".tmp");
        deleteTree(temporary);
        deleteTree(destination);
        requireDirectory(temporary);
        String temporaryPath = temporary.getCanonicalPath() + File.separator;
        AssetManager assets = getAssets();
        for (RuntimeFile runtimeFile : files) {
            File output = new File(temporary, runtimeFile.path).getCanonicalFile();
            if (!output.getPath().startsWith(temporaryPath)) {
                throw new IOException("Runtime asset escapes the extraction root: " + runtimeFile.path);
            }
            File parent = output.getParentFile();
            if (parent == null) {
                throw new IOException("Runtime asset has no parent: " + runtimeFile.path);
            }
            requireDirectory(parent);
            extractAsset(assets, runtimeFile, output);
        }
        writeSyncedText(new File(temporary, COMPLETE_MARKER), runtimeHash);
        Os.rename(temporary.getAbsolutePath(), destination.getAbsolutePath());
        removeOtherRuntimeDirectories(runtimeRoot, destination);
    }

    private void writeSystemLocale() throws Exception {
        String locale = Locale.getDefault().toLanguageTag();
        if (locale.isEmpty()) {
            throw new IOException("Android returned an empty system locale");
        }
        File ludorkRoot = new File(getFilesDir(), "ludork");
        requireDirectory(ludorkRoot);
        File destination = new File(ludorkRoot, "system-locale");
        File temporary = new File(ludorkRoot, ".system-locale.tmp");
        if (temporary.exists() && !temporary.delete()) {
            throw new IOException("Unable to remove the stale system locale file");
        }
        writeSyncedText(temporary, locale);
        Os.rename(temporary.getAbsolutePath(), destination.getAbsolutePath());
    }

    private String readAssetText(String path) throws IOException {
        try (InputStream input = getAssets().open(path, AssetManager.ACCESS_STREAMING);
             ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
            return output.toString(StandardCharsets.UTF_8.name());
        }
    }

    private static void extractAsset(
            AssetManager assets,
            RuntimeFile runtimeFile,
            File output) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        long size = 0;
        try (InputStream input = assets.open(runtimeFile.path, AssetManager.ACCESS_STREAMING);
             FileOutputStream destination = new FileOutputStream(output)) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int count;
            while ((count = input.read(buffer)) != -1) {
                destination.write(buffer, 0, count);
                digest.update(buffer, 0, count);
                size += count;
            }
            destination.getFD().sync();
        }
        if (size != runtimeFile.size || !hex(digest.digest()).equals(runtimeFile.sha256)) {
            throw new IOException("Runtime asset integrity check failed: " + runtimeFile.path);
        }
    }

    private static String runtimeManifestDigest(List<RuntimeFile> files) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        for (RuntimeFile file : files) {
            digest.update(file.path.getBytes(StandardCharsets.UTF_8));
            digest.update((byte) 0);
            digest.update(Long.toString(file.size).getBytes(StandardCharsets.US_ASCII));
            digest.update((byte) 0);
            digest.update(file.sha256.getBytes(StandardCharsets.US_ASCII));
            digest.update((byte) '\n');
        }
        return hex(digest.digest());
    }

    private static String hex(byte[] value) {
        StringBuilder result = new StringBuilder(value.length * 2);
        for (byte item : value) {
            result.append(Character.forDigit((item >>> 4) & 0x0f, 16));
            result.append(Character.forDigit(item & 0x0f, 16));
        }
        return result.toString();
    }

    private static void requireRelativeAssetPath(String path) throws IOException {
        if (path.isEmpty() || path.startsWith("/") || path.contains("\\")) {
            throw new IOException("Invalid runtime asset path: " + path);
        }
        String[] parts = path.split("/", -1);
        for (String part : parts) {
            if (part.isEmpty() || part.equals(".") || part.equals("..")) {
                throw new IOException("Invalid runtime asset path: " + path);
            }
        }
    }

    private static void requireSha256(String value, String description) throws IOException {
        if (value.length() != 64) {
            throw new IOException("Invalid SHA-256 for " + description);
        }
        for (int index = 0; index < value.length(); ++index) {
            char character = value.charAt(index);
            if (!((character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f'))) {
                throw new IOException("Invalid SHA-256 for " + description);
            }
        }
    }

    private static void requireDirectory(File directory) throws IOException {
        if ((!directory.isDirectory() && !directory.mkdirs()) || !directory.isDirectory()) {
            throw new IOException("Unable to create directory: " + directory);
        }
    }

    private static void writeSyncedText(File path, String value) throws IOException {
        try (FileOutputStream output = new FileOutputStream(path)) {
            output.write(value.getBytes(StandardCharsets.UTF_8));
            output.getFD().sync();
        }
    }

    private static String readSmallText(File path) throws IOException {
        if (!path.isFile() || path.length() > 128) {
            return "";
        }
        byte[] value = new byte[(int) path.length()];
        try (FileInputStream input = new FileInputStream(path)) {
            int offset = 0;
            while (offset < value.length) {
                int count = input.read(value, offset, value.length - offset);
                if (count < 0) {
                    break;
                }
                offset += count;
            }
            if (offset != value.length) {
                return "";
            }
        }
        return new String(value, StandardCharsets.UTF_8);
    }

    private static void removeOtherRuntimeDirectories(File root, File current) throws IOException {
        File[] children = root.listFiles();
        if (children == null) {
            throw new IOException("Unable to enumerate the Ludork runtime directory");
        }
        String currentPath = current.getCanonicalPath();
        for (File child : children) {
            if (!child.getCanonicalPath().equals(currentPath)) {
                deleteTree(child);
            }
        }
    }

    private static void deleteTree(File path) throws IOException {
        if (!path.exists()) {
            return;
        }
        if (path.isDirectory()) {
            File[] children = path.listFiles();
            if (children == null) {
                throw new IOException("Unable to enumerate directory: " + path);
            }
            for (File child : children) {
                deleteTree(child);
            }
        }
        if (!path.delete()) {
            throw new IOException("Unable to delete: " + path);
        }
    }
}
