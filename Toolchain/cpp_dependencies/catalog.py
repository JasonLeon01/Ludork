from __future__ import annotations

import pathlib
import re
from dataclasses import dataclass


DEPENDENCY_NAMES = ("luasf", "sfml", "lua", "lua_cjson", "zlib", "ixwebsocket", "ffmpeg")


@dataclass(frozen=True)
class DependencyTarget:
    source: str
    destination: str
    required_files: tuple[str, ...]


@dataclass(frozen=True)
class Dependency:
    name: str
    label: str
    url: str
    archive_name: str
    targets: tuple[DependencyTarget, ...]
    marker: str | None
    sha256: str | None = None
    release_version: str | None = None
    retained_archive: pathlib.PurePath | None = None
    local_archive: pathlib.Path | None = None


def _value(values: dict[str, str], key: str, pattern: str = r"[A-Za-z0-9][A-Za-z0-9.+_-]*") -> str:
    value = values.get(key, "")
    if re.fullmatch(pattern, value) is None:
        raise ValueError(f"A valid {key} is required in versions.conf.")
    return value


def dependency(name: str, values: dict[str, str], *, windows: bool,
               local_luasf_archive: pathlib.Path | None = None) -> Dependency:
    if name == "luasf":
        version = _value(values, "LUASF_VERSION")
        variant = values.get("LUASF_VARIANT", "")
        if variant:
            _value(values, "LUASF_VARIANT")
        suffix = f"-{variant}" if variant else ""
        archive = f"LuaSF-source{suffix}.{'zip' if windows else 'tar.gz'}"
        return Dependency(name, "LuaSF", f"https://github.com/JasonLeon01/LuaSF-AutoGenerator/releases/download/{version}/{archive}",
                          archive, tuple(DependencyTarget(part, part, ("CMakeLists.txt",)) for part in ("LuaSF", "LuaGlue")),
                          version + suffix, local_archive=local_luasf_archive)
    if name == "sfml":
        repository = _value(values, "SFML_REPOSITORY", r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")
        tag = _value(values, "SFML_TAG")
        return Dependency(name, "SFML", f"https://github.com/{repository}/archive/refs/tags/{tag}.tar.gz", "sfml.tar.gz",
                          (DependencyTarget(f"{repository.split('/')[1]}-{tag}", "SFML", ("CMakeLists.txt",)),), tag)
    if name == "lua":
        version = _value(values, "LUA_VERSION")
        digest = _value(values, "LUA_SHA256", r"[0-9a-fA-F]{64}").lower()
        return Dependency(name, "Lua", f"https://www.lua.org/ftp/lua-{version}.tar.gz", "lua.tar.gz",
                          (DependencyTarget(f"lua-{version}", "Lua", ("src/lua.h",)),), version, sha256=digest)
    if name == "lua_cjson":
        version = _value(values, "LUA_CJSON_VERSION")
        return Dependency(name, "lua-cjson", f"https://github.com/openresty/lua-cjson/archive/refs/tags/{version}.zip", "lua-cjson.zip",
                          (DependencyTarget(f"lua-cjson-{version}", "lua-cjson", ("lua_cjson.c",)),), version)
    if name == "zlib":
        version = _value(values, "ZLIB_VERSION")
        return Dependency(name, "zlib", f"https://github.com/madler/zlib/archive/refs/tags/v{version}.zip", "zlib.zip",
                          (DependencyTarget(f"zlib-{version}", "zlib", ("CMakeLists.txt",)),), version)
    if name == "ixwebsocket":
        version = _value(values, "IXWEBSOCKET_VERSION", r"\d+\.\d+\.\d+")
        commit = _value(values, "IXWEBSOCKET_COMMIT", r"[0-9a-f]{40}")
        return Dependency(name, "IXWebSocket", f"https://github.com/machinezone/IXWebSocket/archive/{commit}.tar.gz", "ixwebsocket.tar.gz",
                          (DependencyTarget(f"IXWebSocket-{commit}", "IXWebSocket", ("CMakeLists.txt", "ixwebsocket/IXWebSocket.h")),),
                          f"{version}-{commit}")
    if name == "ffmpeg":
        version = _value(values, "FFMPEG_VERSION")
        commit = _value(values, "FFMPEG_COMMIT", r"[0-9a-f]{40}")
        archive = f"ffmpeg-{version}.tar.gz"
        return Dependency(name, "FFmpeg", f"https://github.com/FFmpeg/FFmpeg/archive/{commit}.tar.gz", archive,
                          (DependencyTarget(f"FFmpeg-{commit}", "ffmpeg", ("configure", "RELEASE")),), None,
                          release_version=version, retained_archive=pathlib.PurePath("ThirdPartySource", archive))
    raise ValueError(f"Unknown native dependency: {name}")
