include_guard(GLOBAL)

include(FetchContent)

function(ludork_add_unicode)
    set(BUILD_SHARED_LIBS OFF)
    set(UTF8PROC_INSTALL OFF CACHE BOOL "" FORCE)
    set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ludork_utf8proc
        URL "https://codeload.github.com/JuliaStrings/utf8proc/tar.gz/refs/tags/v2.11.3"
        URL_HASH SHA256=abfed50b6d4da51345713661370290f4f4747263ee73dc90356299dfc7990c78
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(ludork_utf8proc)
endfunction()

ludork_add_unicode()
