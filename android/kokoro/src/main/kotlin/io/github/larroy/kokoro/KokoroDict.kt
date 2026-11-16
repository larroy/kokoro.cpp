package io.github.larroy.kokoro

import android.content.Context
import android.content.res.AssetManager
import java.io.File
import java.io.IOException
import java.nio.channels.FileChannel
import java.nio.file.StandardOpenOption

/** The G2P dictionaries bundled in this library's assets. */
public object KokoroDict {
    private const val ASSET_DIR = "kokoro-dict"
    private const val STAMP = "kokoro-dict.stamp"
    private const val LOCK = "kokoro-dict.lock"

    /**
     * Extracts the bundled dictionaries to `noBackupFilesDir/kokoro-dict` unless that copy is already current, and
     * returns the directory; pass its `path` as `dictDir` to [KokoroContext]. The first call copies about 39 MB, so
     * call it off the main thread.
     *
     * @throws IOException the dictionaries could not be extracted.
     */
    @JvmStatic
    @Synchronized
    public fun install(context: Context): File {
        val assets = context.assets
        val expected = assets.open(STAMP).use { it.readBytes().decodeToString() }
        val root = context.noBackupFilesDir
        val target = File(root, ASSET_DIR)
        if (isCurrent(target, expected)) return target
        // Every process of the app stages into the same directory: serialize them across processes too.
        val lockFile = File(root, LOCK)
        FileChannel.open(lockFile.toPath(), StandardOpenOption.CREATE, StandardOpenOption.WRITE).use { channel ->
            channel.lock().use {
                if (isCurrent(target, expected)) return target
                replace(assets, root, target, expected)
                return target
            }
        }
    }

    private fun isCurrent(target: File, expected: String): Boolean =
        File(target, STAMP).takeIf { it.isFile }?.readText() == expected

    private fun replace(assets: AssetManager, root: File, target: File, expected: String) {
        val staging = File(root, "$ASSET_DIR.tmp")
        staging.deleteRecursively()
        copyAssetTree(assets, ASSET_DIR, staging)
        File(staging, STAMP).writeText(expected)
        target.deleteRecursively()
        if (!staging.renameTo(target)) throw IOException("Could not move $staging to $target")
    }

    // AssetManager.list returns no children for files; dict/ has no empty directories.
    private fun copyAssetTree(assets: AssetManager, path: String, dest: File) {
        val children = assets.list(path).orEmpty()
        if (children.isEmpty()) {
            dest.parentFile?.mkdirs()
            assets.open(path).use { input -> dest.outputStream().use { input.copyTo(it) } }
            return
        }
        dest.mkdirs()
        children.forEach { copyAssetTree(assets, "$path/$it", File(dest, it)) }
    }
}
