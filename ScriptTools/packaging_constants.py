from __future__ import annotations

import re


EXIT_TOOLCHAIN = 20
EXIT_DEVICE = 21
EXIT_SIGNING = 22
EXIT_PROJECT = 23
EXIT_APP_NAME_UNCHANGED = 24
EDITOR_CACHE_DIRECTORY = "EditorCache"
PACKAGE_CACHE_DIRECTORIES = (EDITOR_CACHE_DIRECTORY, "Cache")
GAME_BUILD_TOOL_FILES = ("LudorkNativeStubDump", "LudorkNativeStubDump.exe")
COMPILE_LUA_DIRECTORIES_ENVIRONMENT = "LUDORK_PACK_COMPILE_LUA_DIRECTORIES"
EXCLUDED_FILES_ENVIRONMENT = "LUDORK_PACK_EXCLUDED_FILES"
TEMPLATE_TOKEN_PATTERN = re.compile(r"__LUDORK_[A-Z0-9_]+__")
FILE_BUFFER_SIZE = 1024 * 1024
RUNTIME_LEGAL_FILES = (
    "LICENSE.md",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES_zh_CN.md",
)
MOBILE_PROJECT_DIRECTORIES = (
    "Assets", "Engine/Source", "Engine/Runtime", "Data", "Application",
    "Engine/ThirdParty/LuaSF", "Engine/ThirdParty/lua-cjson", "Scripts",
    "Engine/Standard", "Engine/ThirdParty/zlib",
    "Engine/ThirdParty/SFML", "Engine/ThirdParty/LuaGlue", "Engine/ThirdParty/Lua",
)
MOBILE_DEPENDENCY_NAMES = (
    "flac", "freetype", "harfbuzz", "libssh2", "ludork_utf8proc", "mbedtls", "ogg", "sheenbidi", "vorbis",
)
COMMON_DEPENDENCY_CACHE_DIRECTORIES = ("build/_deps", "build/Release/_deps", "build/Debug/_deps")
CPP_TEMPLATE_NAMES = ("Cpp", "Cpp-ffmpeg")
STANDALONE_TEMPLATE_NAMES = ("Standalone", "Standalone-ffmpeg")
TEMPLATE_NAMES = CPP_TEMPLATE_NAMES + STANDALONE_TEMPLATE_NAMES
PLAIN_TEMPLATE_NAMES = (CPP_TEMPLATE_NAMES[0], STANDALONE_TEMPLATE_NAMES[0])
FFMPEG_TEMPLATE_NAMES = (CPP_TEMPLATE_NAMES[1], STANDALONE_TEMPLATE_NAMES[1])
NATIVE_LUA_FILES = (
    "stub/Engine.d.lua", "stub/GlobalCore.d.lua", "stub/GlobalFunctions.d.lua",
    "stub/LuaSF.d.lua", "Engine_meta.lua", "GlobalCore_meta.lua", "GlobalFunctions_meta.lua",
)
