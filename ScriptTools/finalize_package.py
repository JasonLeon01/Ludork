from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import struct
import zlib

from .compile_lua import compile_scripts, lua_source_paths, resolve_luac
from .ldpak import pack_assets
from .ui_assets import validate_assets


SHADER_MAGIC = b"LDSC"
DATA_MAGIC = b"LDDC"
VERSION = 1
FLAG_ZLIB = 1
HEADER = struct.Struct("<4sBBHIIQ")
KEY_SEED = 0xD6E8FEB86659FD93
STREAM_MULTIPLIER = 0x2545F4914F6CDD1D
STREAM_FALLBACK = 0x9E3779B97F4A7C15
UINT64_MASK = 0xFFFFFFFFFFFFFFFF
MAX_SHADER_SIZE = 64 * 1024 * 1024
MAX_DATA_SIZE = 512 * 1024 * 1024
COMPILE_LUA_DIRECTORIES_ENVIRONMENT = "LUDORK_PACK_COMPILE_LUA_DIRECTORIES"
EXCLUDED_FILES_ENVIRONMENT = "LUDORK_PACK_EXCLUDED_FILES"
SHADER_EXTENSIONS = {
    ".frag": ".fragc",
    ".vert": ".vertc",
    ".geom": ".geomc",
}
UI_PREVIEW_HOST_PREFIXES = (
    "uipreviewhost",
    "uipreviewcurveresolver",
)


class ShaderCodecError(RuntimeError):
    pass


class DataCodecError(RuntimeError):
    pass


def _next_stream_block(state: int) -> tuple[int, bytes]:
    state ^= state >> 12
    state ^= (state << 25) & UINT64_MASK
    state ^= state >> 27
    state &= UINT64_MASK
    value = (state * STREAM_MULTIPLIER) & UINT64_MASK
    return state, value.to_bytes(8, "little")


def _apply_stream(data: bytes, nonce: int) -> bytes:
    state = (nonce ^ KEY_SEED) & UINT64_MASK
    if state == 0:
        state = STREAM_FALLBACK
    result = bytearray(len(data))
    offset = 0
    while offset < len(data):
        state, block = _next_stream_block(state)
        count = min(len(block), len(data) - offset)
        for index in range(count):
            result[offset + index] = data[offset + index] ^ block[index]
        offset += count
    return bytes(result)


def _content_nonce(relative_path: pathlib.PurePath, source: bytes) -> int:
    digest = hashlib.sha256(
        relative_path.as_posix().encode("utf-8") + b"\0" + source
    ).digest()
    return int.from_bytes(digest[:8], "little")


def _encode_bytes(
    relative_path: pathlib.PurePath,
    source: bytes,
    magic: bytes,
    maximum_size: int,
    error_type: type[RuntimeError],
    kind: str,
) -> bytes:
    if len(source) > maximum_size:
        raise error_type(f"{kind} is too large: {relative_path}")
    nonce = _content_nonce(relative_path, source)
    compressed = zlib.compress(source, level=9)
    payload = _apply_stream(compressed, nonce)
    return HEADER.pack(
        magic,
        VERSION,
        FLAG_ZLIB,
        0,
        len(source),
        zlib.crc32(source) & 0xFFFFFFFF,
        nonce,
    ) + payload


def encode_shader_bytes(relative_path: pathlib.PurePath, source: bytes) -> bytes:
    return _encode_bytes(
        relative_path,
        source,
        SHADER_MAGIC,
        MAX_SHADER_SIZE,
        ShaderCodecError,
        "Shader",
    )


def decode_shader_bytes(encoded: bytes) -> bytes:
    if len(encoded) < HEADER.size:
        raise ShaderCodecError("Encrypted shader header is truncated")
    magic, version, flags, reserved, source_size, checksum, nonce = (
        HEADER.unpack_from(encoded)
    )
    if magic != SHADER_MAGIC:
        raise ShaderCodecError("Encrypted shader magic is invalid")
    if version != VERSION:
        raise ShaderCodecError(f"Unsupported encrypted shader version: {version}")
    if flags != FLAG_ZLIB or reserved != 0:
        raise ShaderCodecError("Encrypted shader flags are invalid")
    compressed = _apply_stream(encoded[HEADER.size :], nonce)
    if source_size > MAX_SHADER_SIZE:
        raise ShaderCodecError("Encrypted shader source is too large")
    try:
        decompressor = zlib.decompressobj()
        source = decompressor.decompress(compressed, source_size + 1)
        if (
            not decompressor.eof
            or decompressor.unconsumed_tail
            or decompressor.unused_data
        ):
            raise ShaderCodecError(
                "Encrypted shader payload could not be decompressed"
            )
        source += decompressor.flush()
    except zlib.error as exception:
        raise ShaderCodecError(
            "Encrypted shader payload could not be decompressed"
        ) from exception
    if len(source) != source_size:
        raise ShaderCodecError("Encrypted shader size does not match its header")
    if zlib.crc32(source) & 0xFFFFFFFF != checksum:
        raise ShaderCodecError("Encrypted shader checksum does not match")
    return source


def _replace_sources(
    jobs: list[tuple[pathlib.Path, pathlib.Path, bytes]],
    error_type: type[RuntimeError],
    kind: str,
) -> None:
    temporary_paths: list[pathlib.Path] = []
    try:
        for _, target_path, encoded in jobs:
            temporary_path = target_path.with_name(target_path.name + ".tmp")
            if temporary_path.exists():
                raise error_type(
                    f"Encrypted {kind} temporary file already exists: {temporary_path}"
                )
            temporary_path.write_bytes(encoded)
            temporary_paths.append(temporary_path)
        for (_, target_path, _), temporary_path in zip(jobs, temporary_paths):
            temporary_path.replace(target_path)
        for source_path, _, _ in jobs:
            source_path.unlink()
    finally:
        for temporary_path in temporary_paths:
            if temporary_path.exists():
                temporary_path.unlink()


def encrypt_shaders(shader_root: pathlib.Path) -> int:
    if not shader_root.is_dir():
        return 0
    jobs: list[tuple[pathlib.Path, pathlib.Path, bytes]] = []
    for source_path in sorted(
        path for path in shader_root.rglob("*") if path.is_file()
    ):
        target_extension = SHADER_EXTENSIONS.get(source_path.suffix.lower())
        if target_extension is None:
            continue
        target_path = source_path.with_suffix(target_extension)
        if target_path.exists():
            raise ShaderCodecError(
                f"Encrypted shader target already exists: {target_path}"
            )
        relative_path = source_path.relative_to(shader_root)
        jobs.append(
            (
                source_path,
                target_path,
                encode_shader_bytes(relative_path, source_path.read_bytes()),
            )
        )

    _replace_sources(jobs, ShaderCodecError, "shader")
    return len(jobs)


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"Invalid JSON constant: {value}")


def _compact_json(source_path: pathlib.Path, source: bytes) -> bytes:
    try:
        value = json.loads(
            source.decode("utf-8-sig"),
            parse_constant=_reject_json_constant,
        )
        compact = json.dumps(
            value,
            ensure_ascii=False,
            allow_nan=False,
            separators=(",", ":"),
        )
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exception:
        raise DataCodecError(f"Invalid JSON data file: {source_path}") from exception
    return compact.encode("utf-8")


def encrypt_data(data_root: pathlib.Path) -> int:
    if not data_root.is_dir():
        return 0
    jobs: list[tuple[pathlib.Path, pathlib.Path, bytes]] = []
    for source_path in sorted(
        path for path in data_root.rglob("*") if path.is_file()
    ):
        if source_path.suffix.lower() != ".json":
            continue
        target_path = source_path.with_suffix(".ldc")
        if target_path.exists():
            raise DataCodecError(
                f"Encrypted data target already exists: {target_path}"
            )
        relative_path = source_path.relative_to(data_root)
        compact = _compact_json(source_path, source_path.read_bytes())
        jobs.append(
            (
                source_path,
                target_path,
                _encode_bytes(
                    relative_path,
                    compact,
                    DATA_MAGIC,
                    MAX_DATA_SIZE,
                    DataCodecError,
                    "JSON data",
                ),
            )
        )

    _replace_sources(jobs, DataCodecError, "data")
    return len(jobs)


def _strip_ui_editor_values(value: object) -> int:
    if isinstance(value, list):
        return sum(_strip_ui_editor_values(item) for item in value)
    if not isinstance(value, dict):
        return 0
    removed = 0
    if "editor" in value:
        del value["editor"]
        removed += 1
    for item in value.values():
        removed += _strip_ui_editor_values(item)
    return removed


def strip_ui_editor_data(data_root: pathlib.Path) -> int:
    assets_root = data_root / "UI" / "Assets"
    if not assets_root.is_dir():
        return 0
    removed = 0
    for source_path in sorted(assets_root.rglob("*.json")):
        try:
            value = json.loads(
                source_path.read_text(encoding="utf-8-sig"),
                parse_constant=_reject_json_constant,
            )
        except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exception:
            raise DataCodecError(
                f"Invalid UI asset JSON file: {source_path}"
            ) from exception
        if not isinstance(value, dict) or value.get("type") != "uiAsset":
            raise DataCodecError(f"Invalid UI asset data file: {source_path}")
        stripped = _strip_ui_editor_values(value)
        if stripped == 0:
            continue
        source_path.write_text(
            json.dumps(
                value,
                ensure_ascii=False,
                allow_nan=False,
                separators=(",", ":"),
            ),
            encoding="utf-8",
        )
        removed += stripped
    return removed


def _remove_path(path: pathlib.Path) -> bool:
    if path.is_symlink() or path.is_file():
        path.unlink()
        return True
    if path.is_dir():
        shutil.rmtree(path)
        return True
    return False


def _environment_relative_paths(name: str) -> tuple[pathlib.PurePosixPath, ...]:
    paths: list[pathlib.PurePosixPath] = []
    for text in os.environ.get(name, "").splitlines():
        value = text.strip()
        if not value:
            continue
        path = pathlib.PurePosixPath(value)
        if (
            path.is_absolute()
            or "\\" in value
            or not path.parts
            or any(part in {"", ".", ".."} for part in path.parts)
        ):
            raise RuntimeError(f"Invalid package-relative path: {value}")
        paths.append(path)
    return tuple(paths)


def _package_path(
    resource_root: pathlib.Path,
    relative_path: pathlib.PurePosixPath,
) -> pathlib.Path:
    target = (resource_root / pathlib.Path(*relative_path.parts)).resolve()
    try:
        target.relative_to(resource_root)
    except ValueError as exception:
        raise RuntimeError(
            f"Package path escapes the resource root: {relative_path}"
        ) from exception
    return target


def prune_package(
    resource_root: pathlib.Path,
    excluded_files: tuple[pathlib.PurePosixPath, ...] = (),
) -> int:
    removed = 0
    for relative_path in (pathlib.Path("Scripts") / "stub",):
        target = resource_root / relative_path
        if _remove_path(target):
            removed += 1

    preview_host_entries = sorted(
        (
            path
            for path in resource_root.rglob("*")
            if path.name.casefold().startswith(UI_PREVIEW_HOST_PREFIXES)
        ),
        key=lambda path: len(path.parts),
        reverse=True,
    )
    for path in preview_host_entries:
        if _remove_path(path):
            removed += 1

    vscode_directories = sorted(
        (
            path
            for path in resource_root.rglob(".vscode")
            if path.is_dir() or path.is_symlink()
        ),
        key=lambda path: len(path.parts),
        reverse=True,
    )
    for directory in vscode_directories:
        if _remove_path(directory):
            removed += 1

    for name in (".emmyrc.json", ".gitignore"):
        for path in sorted(resource_root.rglob(name)):
            if _remove_path(path):
                removed += 1

    for relative_path in excluded_files:
        if _remove_path(_package_path(resource_root, relative_path)):
            removed += 1
    return removed


def compile_package_lua(
    resource_root: pathlib.Path,
    relative_directories: tuple[pathlib.PurePosixPath, ...],
) -> int:
    compiled = 0
    luac: pathlib.Path | None = None
    for relative_directory in relative_directories:
        directory = _package_path(resource_root, relative_directory)
        if not directory.is_dir() or not lua_source_paths(directory):
            continue
        if luac is None:
            luac = resolve_luac()
        compiled += compile_scripts(directory, luac)
    return compiled


def reject_declaration_files(resource_root: pathlib.Path) -> None:
    declaration_files = sorted(resource_root.rglob("*.d.lua"))
    if declaration_files:
        relative_paths = ", ".join(
            path.relative_to(resource_root).as_posix()
            for path in declaration_files
        )
        raise RuntimeError(
            f"Lua declaration files remain in the game package: {relative_paths}"
        )


def finalize_package(
    resource_root: pathlib.Path,
    encrypt_shaders_enabled: bool,
    encrypt_data_enabled: bool,
    compile_lua_directories: tuple[pathlib.PurePosixPath, ...] | None = None,
    excluded_files: tuple[pathlib.PurePosixPath, ...] | None = None,
    pack_assets_enabled: bool = False,
) -> tuple[int, int, int, int, int]:
    root = resource_root.expanduser().resolve()
    if not root.is_dir():
        raise RuntimeError(f"Package resource root was not found: {root}")
    if compile_lua_directories is None:
        compile_lua_directories = _environment_relative_paths(
            COMPILE_LUA_DIRECTORIES_ENVIRONMENT
        )
    if excluded_files is None:
        excluded_files = _environment_relative_paths(EXCLUDED_FILES_ENVIRONMENT)
    validate_assets(root)
    removed = prune_package(root, excluded_files)
    removed += strip_ui_editor_data(root / "Data")
    compiled_lua = compile_package_lua(root, compile_lua_directories)
    encrypted_shaders = (
        encrypt_shaders(root / "Assets" / "Shaders")
        if encrypt_shaders_enabled
        else 0
    )
    encrypted_data = (
        encrypt_data(root / "Data") if encrypt_data_enabled else 0
    )
    reject_declaration_files(root)
    packed_assets = pack_assets(root / "Assets") if pack_assets_enabled else 0
    return removed, encrypted_shaders, encrypted_data, compiled_lua, packed_assets


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools finalize-package")
    parser.add_argument("--encrypt-shaders", action="store_true")
    parser.add_argument("--encrypt-data", action="store_true")
    parser.add_argument("--pack-assets", action="store_true")
    parser.add_argument("resource_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    (
        removed,
        encrypted_shaders,
        encrypted_data,
        compiled_lua,
        packed_assets,
    ) = finalize_package(
        parsed.resource_root,
        parsed.encrypt_shaders,
        parsed.encrypt_data,
        pack_assets_enabled=parsed.pack_assets,
    )
    print(f"Removed {removed} development-only package entries")
    if compiled_lua:
        print(f"Compiled and renamed {compiled_lua} plug-in package Lua files")
    if parsed.encrypt_shaders:
        print(f"Encrypted {encrypted_shaders} shader files")
    if parsed.encrypt_data:
        print(f"Encrypted {encrypted_data} JSON data files")
    if parsed.pack_assets:
        print(f"Packed {packed_assets} asset directories into .ldpak archives")
    return 0
