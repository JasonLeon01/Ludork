#include <PersistentStorage.hpp>
#include <Utf8Path.hpp>

#include <stdexcept>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// clang-format off
EM_ASYNC_JS(int, ludorkRestoreStorage, (const char* path), {
    const directory = UTF8ToString(path);
    const mounted = Module['ludorkStorageMounts'] ||= new Set();
    if (mounted.has(directory)) return 1;
    try {
        const sync = populate => new Promise((resolve, reject) => {
            FS.syncfs(populate, error => error ? reject(error) : resolve());
        });
        if (mounted.size) await sync(false);
        FS.mkdirTree(directory);
        FS.mount(IDBFS, {}, directory);
        try {
            await sync(true);
        } catch (error) {
            FS.unmount(directory);
            throw error;
        }
        mounted.add(directory);
        return 1;
    } catch (error) {
        Module['printErr']('Unable to restore browser storage: ' + error);
        return 0;
    }
});

EM_ASYNC_JS(int, ludorkPersistStorage, (const char* path), {
    const file = UTF8ToString(path);
    const mounts = Module['ludorkStorageMounts'];
    if (!mounts || !Array.from(mounts).some(root => file === root || file.startsWith(root + '/'))) return 1;
    try {
        await new Promise((resolve, reject) => {
            FS.syncfs(false, error => error ? reject(error) : resolve());
        });
        return 1;
    } catch (error) {
        Module['printErr']('Unable to persist browser storage: ' + error);
        return 0;
    }
});
// clang-format on
#endif

namespace ludork::standard {

void restoreStorage(const std::filesystem::path& directory) {
#ifdef __EMSCRIPTEN__
    if (!ludorkRestoreStorage(pathToUtf8(directory).c_str())) {
        throw std::runtime_error("Unable to restore browser storage: " +
                                 pathToUtf8(directory));
    }
#else
    static_cast<void>(directory);
#endif
}

void persistStorage(const std::filesystem::path& path) {
#ifdef __EMSCRIPTEN__
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    if (!ludorkPersistStorage(pathToUtf8(absolute).c_str())) {
        throw std::runtime_error("Unable to persist browser storage: " +
                                 pathToUtf8(path));
    }
#else
    static_cast<void>(path);
#endif
}

}  // namespace ludork::standard
