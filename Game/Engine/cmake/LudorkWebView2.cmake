include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(ludork_webview2
    URL "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4191.47/microsoft.web.webview2.1.0.4191.47.nupkg"
    URL_HASH SHA256=f492bbf547d0da329553b6727435b677579b1e9f91cc9e4a1ad029366d5f23d0
    DOWNLOAD_NAME webview2-sdk.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(ludork_webview2)

if(CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]64"
   OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$"
   OR CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
    set(ludork_webview2_arch arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ludork_webview2_arch x64)
else()
    set(ludork_webview2_arch x86)
endif()

add_library(LudorkWebView2 STATIC IMPORTED GLOBAL)
add_library(Ludork::WebView2 ALIAS LudorkWebView2)
set_target_properties(LudorkWebView2 PROPERTIES
    IMPORTED_LOCATION "${ludork_webview2_SOURCE_DIR}/build/native/${ludork_webview2_arch}/WebView2LoaderStatic.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ludork_webview2_SOURCE_DIR}/build/native/include"
    INTERFACE_LINK_LIBRARIES version)
