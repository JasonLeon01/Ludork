from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import struct
import sys
import tempfile
import zlib
from dataclasses import dataclass
from typing import BinaryIO


MAGIC = b"LDPK"
VERSION = 1
FLAGS = 0
DIRECTORY_FLAG = 1
ALIGNMENT = 8
HEADER = struct.Struct("<4sHHIIQQII")
ENTRY = struct.Struct("<IIQQII")
BUFFER_SIZE = 1024 * 1024
SCRIPT_GROUP = "Scripts"
SCRIPT_ENTRY_PATHS = ("Entry.lua", "Entry.luac")


class LdPakError(RuntimeError):
    pass


@dataclass(frozen=True)
class _SourceEntry:
    source_path: pathlib.Path
    relative_path: str
    path_bytes: bytes
    is_directory: bool


@dataclass(frozen=True)
class _ArchiveEntry:
    relative_path: str
    path_bytes: bytes
    flags: int
    data_offset: int
    data_size: int
    data_crc32: int


def _align(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def _is_link(path: pathlib.Path) -> bool:
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    return bool(is_junction is not None and is_junction())


def _discard_tree(path: pathlib.Path) -> None:
    try:
        shutil.rmtree(path)
    except OSError:
        pass


def _encode_utf8(value: str, kind: str) -> bytes:
    try:
        return value.encode("utf-8")
    except UnicodeEncodeError as exception:
        raise LdPakError(f"{kind} is not valid UTF-8: {value!r}") from exception


def _validate_group_name(group_name: str) -> bytes:
    if (
        not group_name
        or group_name in {".", ".."}
        or "/" in group_name
        or "\\" in group_name
        or "\0" in group_name
    ):
        raise LdPakError(f"Invalid asset package group name: {group_name!r}")
    if group_name.casefold().endswith(".ldpak"):
        raise LdPakError(
            f"Asset directory names must not end with .ldpak: {group_name}"
        )
    group_bytes = _encode_utf8(group_name, "Asset package group name")
    if len(group_bytes) > 0xFFFFFFFF:
        raise LdPakError(f"Asset package group name is too long: {group_name!r}")
    return group_bytes


def _validate_relative_path(relative_path: str) -> None:
    if (
        not relative_path
        or relative_path.startswith("/")
        or relative_path.endswith("/")
        or "\\" in relative_path
        or "\0" in relative_path
    ):
        raise LdPakError(f"Invalid asset package entry path: {relative_path!r}")
    parts = relative_path.split("/")
    if any(part in {"", ".", ".."} for part in parts):
        raise LdPakError(f"Invalid asset package entry path: {relative_path!r}")


def _scan_group(group_root: pathlib.Path) -> list[_SourceEntry]:
    if _is_link(group_root):
        raise LdPakError(
            f"Asset package group must not be a symbolic link: {group_root}"
        )
    if not group_root.is_dir():
        raise LdPakError(f"Asset package group was not found: {group_root}")

    entries: list[_SourceEntry] = []

    def visit(directory: pathlib.Path, parent_parts: tuple[str, ...]) -> None:
        try:
            children = list(os.scandir(directory))
        except OSError as exception:
            raise LdPakError(
                f"Unable to enumerate asset directory: {directory}"
            ) from exception
        for child in children:
            source_path = pathlib.Path(child.path)
            if child.is_symlink() or _is_link(source_path):
                raise LdPakError(
                    f"Asset packages must not contain symbolic links: {source_path}"
                )
            if child.is_file(follow_symlinks=False) and child.name == ".DS_Store":
                continue
            relative_parts = (*parent_parts, child.name)
            relative_path = "/".join(relative_parts)
            _validate_relative_path(relative_path)
            path_bytes = _encode_utf8(relative_path, "Asset package entry path")
            if len(path_bytes) > 0xFFFFFFFF:
                raise LdPakError(
                    f"Asset package entry path is too long: {relative_path!r}"
                )
            if child.is_dir(follow_symlinks=False):
                entries.append(
                    _SourceEntry(source_path, relative_path, path_bytes, True)
                )
                visit(source_path, relative_parts)
            elif child.is_file(follow_symlinks=False):
                entries.append(
                    _SourceEntry(source_path, relative_path, path_bytes, False)
                )
            else:
                raise LdPakError(
                    f"Asset packages support only files and directories: {source_path}"
                )

    visit(group_root, ())
    entries.sort(key=lambda entry: entry.path_bytes)
    folded_paths: dict[str, str] = {}
    for entry in entries:
        folded = entry.relative_path.casefold()
        previous = folded_paths.get(folded)
        if previous is not None:
            raise LdPakError(
                "Asset package paths differ only by case: "
                f"{previous!r} and {entry.relative_path!r}"
            )
        folded_paths[folded] = entry.relative_path
    if len(entries) > 0xFFFFFFFF:
        raise LdPakError(f"Asset package contains too many entries: {group_root}")
    return entries


def _write_padding(stream: BinaryIO, target_offset: int) -> None:
    current_offset = stream.tell()
    if current_offset > target_offset:
        raise LdPakError("Asset package layout moved past its expected offset")
    if current_offset != target_offset:
        stream.write(b"\0" * (target_offset - current_offset))


def write_ldpak(group_root: pathlib.Path, destination: pathlib.Path) -> int:
    group_root = pathlib.Path(group_root)
    destination = pathlib.Path(destination)
    group_name = group_root.name
    group_bytes = _validate_group_name(group_name)
    entries = _scan_group(group_root)
    if os.path.lexists(destination):
        raise LdPakError(f"Asset package output already exists: {destination}")

    archive_entries: list[_ArchiveEntry] = []
    try:
        with destination.open("xb") as stream:
            stream.write(b"\0" * HEADER.size)
            stream.write(group_bytes)
            _write_padding(stream, _align(stream.tell()))

            for entry in entries:
                if entry.is_directory:
                    archive_entries.append(
                        _ArchiveEntry(
                            entry.relative_path,
                            entry.path_bytes,
                            DIRECTORY_FLAG,
                            0,
                            0,
                            0,
                        )
                    )
                    continue
                if _is_link(entry.source_path):
                    raise LdPakError(
                        "Asset packages must not contain symbolic links: "
                        f"{entry.source_path}"
                    )
                _write_padding(stream, _align(stream.tell()))
                data_offset = stream.tell()
                data_size = 0
                data_crc32 = 0
                with entry.source_path.open("rb") as source:
                    while chunk := source.read(BUFFER_SIZE):
                        stream.write(chunk)
                        data_size += len(chunk)
                        data_crc32 = zlib.crc32(chunk, data_crc32)
                if data_size > 0xFFFFFFFFFFFFFFFF:
                    raise LdPakError(
                        f"Asset package entry is too large: {entry.relative_path}"
                    )
                archive_entries.append(
                    _ArchiveEntry(
                        entry.relative_path,
                        entry.path_bytes,
                        0,
                        data_offset,
                        data_size,
                        data_crc32 & 0xFFFFFFFF,
                    )
                )

            _write_padding(stream, _align(stream.tell()))
            index_offset = stream.tell()
            index = bytearray()
            for entry in archive_entries:
                index.extend(
                    ENTRY.pack(
                        len(entry.path_bytes),
                        entry.flags,
                        entry.data_offset,
                        entry.data_size,
                        entry.data_crc32,
                        0,
                    )
                )
                index.extend(entry.path_bytes)
            index_size = len(index)
            stream.write(index)
            header = HEADER.pack(
                MAGIC,
                VERSION,
                FLAGS,
                len(group_bytes),
                len(archive_entries),
                index_offset,
                index_size,
                zlib.crc32(index) & 0xFFFFFFFF,
                0,
            )
            stream.seek(0)
            stream.write(header)
    except BaseException:
        if os.path.lexists(destination):
            destination.unlink()
        raise
    return len(archive_entries)


def _read_exact(stream: BinaryIO, size: int, description: str) -> bytes:
    value = stream.read(size)
    if len(value) != size:
        raise LdPakError(f"Asset package {description} is truncated")
    return value


def _require_zero_bytes(
    stream: BinaryIO,
    offset: int,
    size: int,
    description: str,
) -> None:
    stream.seek(offset)
    remaining = size
    while remaining:
        chunk = _read_exact(
            stream,
            min(remaining, BUFFER_SIZE),
            description,
        )
        if any(chunk):
            raise LdPakError(f"Asset package {description} is not zero-filled")
        remaining -= len(chunk)


def _validate_ldpak_entries(
    package_path: pathlib.Path,
    expected_group: str | None = None,
    require_matching_filename: bool = True,
) -> tuple[_ArchiveEntry, ...]:
    package_path = pathlib.Path(package_path)
    if _is_link(package_path) or not package_path.is_file():
        raise LdPakError(f"Asset package was not found: {package_path}")
    file_size = package_path.stat().st_size
    if file_size < HEADER.size:
        raise LdPakError("Asset package header is truncated")

    with package_path.open("rb") as stream:
        (
            magic,
            version,
            flags,
            group_size,
            entry_count,
            index_offset,
            index_size,
            index_crc32,
            reserved,
        ) = HEADER.unpack(_read_exact(stream, HEADER.size, "header"))
        if magic != MAGIC:
            raise LdPakError("Asset package magic is invalid")
        if version != VERSION:
            raise LdPakError(f"Unsupported asset package version: {version}")
        if flags != FLAGS or reserved != 0:
            raise LdPakError("Asset package header flags are invalid")
        if group_size == 0 or HEADER.size + group_size > file_size:
            raise LdPakError("Asset package group name length is invalid")
        if index_offset > file_size or index_size > file_size - index_offset:
            raise LdPakError("Asset package index bounds are invalid")
        group_bytes = _read_exact(stream, group_size, "group name")
        try:
            group_name = group_bytes.decode("utf-8")
        except UnicodeDecodeError as exception:
            raise LdPakError(
                "Asset package group name is not valid UTF-8"
            ) from exception
        if _validate_group_name(group_name) != group_bytes:
            raise LdPakError("Asset package group name is not canonical UTF-8")
        if expected_group is not None and group_name != expected_group:
            raise LdPakError(
                f"Asset package group is {group_name!r}, expected {expected_group!r}"
            )
        if require_matching_filename and package_path.name != group_name + ".ldpak":
            raise LdPakError(
                "Asset package filename does not match its group: "
                f"{package_path.name!r}"
            )

        data_start = _align(HEADER.size + group_size)
        if index_offset % ALIGNMENT != 0:
            raise LdPakError("Asset package index is not 8-byte aligned")
        if index_offset < data_start:
            raise LdPakError("Asset package index overlaps its header")
        if index_offset + index_size != file_size:
            raise LdPakError("Asset package size does not match its index")
        _require_zero_bytes(
            stream,
            HEADER.size + group_size,
            data_start - (HEADER.size + group_size),
            "group padding",
        )
        stream.seek(index_offset)
        index = _read_exact(stream, index_size, "index")
        if zlib.crc32(index) & 0xFFFFFFFF != index_crc32:
            raise LdPakError("Asset package index checksum does not match")

        archive_entries: list[_ArchiveEntry] = []
        index_position = 0
        previous_path_bytes: bytes | None = None
        folded_paths: dict[str, str] = {}
        declared_types: dict[str, bool] = {}
        for _ in range(entry_count):
            if index_position + ENTRY.size > len(index):
                raise LdPakError("Asset package index entry is truncated")
            (
                path_size,
                entry_flags,
                data_offset,
                data_size,
                data_crc32,
                entry_reserved,
            ) = ENTRY.unpack_from(index, index_position)
            index_position += ENTRY.size
            if path_size == 0 or index_position + path_size > len(index):
                raise LdPakError("Asset package index path is truncated")
            path_bytes = bytes(index[index_position : index_position + path_size])
            index_position += path_size
            try:
                relative_path = path_bytes.decode("utf-8")
            except UnicodeDecodeError as exception:
                raise LdPakError(
                    "Asset package entry path is not valid UTF-8"
                ) from exception
            if _encode_utf8(relative_path, "Asset package entry path") != path_bytes:
                raise LdPakError("Asset package entry path is not canonical UTF-8")
            _validate_relative_path(relative_path)
            if previous_path_bytes is not None and path_bytes <= previous_path_bytes:
                raise LdPakError(
                    "Asset package entry paths are duplicated or not sorted"
                )
            previous_path_bytes = path_bytes
            folded = relative_path.casefold()
            previous = folded_paths.get(folded)
            if previous is not None:
                raise LdPakError(
                    "Asset package paths differ only by case: "
                    f"{previous!r} and {relative_path!r}"
                )
            folded_paths[folded] = relative_path
            if entry_flags & ~DIRECTORY_FLAG or entry_reserved != 0:
                raise LdPakError(
                    f"Asset package entry flags are invalid: {relative_path}"
                )
            is_directory = bool(entry_flags & DIRECTORY_FLAG)
            parts = relative_path.split("/")
            for index_part in range(1, len(parts)):
                parent = "/".join(parts[:index_part])
                parent_is_directory = declared_types.get(parent)
                if parent_is_directory is None:
                    raise LdPakError(
                        f"Asset package entry has an undeclared parent: {relative_path}"
                    )
                if not parent_is_directory:
                    raise LdPakError(
                        f"Asset package entry has a file parent: {relative_path}"
                    )
            declared_types[relative_path] = is_directory
            if is_directory:
                if data_offset != 0 or data_size != 0 or data_crc32 != 0:
                    raise LdPakError(
                        f"Asset package directory has file data: {relative_path}"
                    )
            else:
                if data_offset % ALIGNMENT != 0:
                    raise LdPakError(
                        f"Asset package file is not 8-byte aligned: {relative_path}"
                    )
                if data_offset < data_start or data_offset > index_offset:
                    raise LdPakError(
                        f"Asset package file offset is invalid: {relative_path}"
                    )
                if data_size > index_offset - data_offset:
                    raise LdPakError(
                        f"Asset package file range is invalid: {relative_path}"
                    )
            archive_entries.append(
                _ArchiveEntry(
                    relative_path,
                    path_bytes,
                    entry_flags,
                    data_offset,
                    data_size,
                    data_crc32,
                )
            )
        if index_position != len(index):
            raise LdPakError("Asset package index has trailing data")

        expected_offset = data_start
        for entry in archive_entries:
            if entry.flags & DIRECTORY_FLAG:
                continue
            aligned_offset = _align(expected_offset)
            if entry.data_offset != aligned_offset:
                raise LdPakError(
                    f"Asset package file layout is invalid: {entry.relative_path}"
                )
            _require_zero_bytes(
                stream,
                expected_offset,
                aligned_offset - expected_offset,
                f"padding before {entry.relative_path}",
            )
            stream.seek(entry.data_offset)
            remaining = entry.data_size
            checksum = 0
            while remaining:
                chunk = _read_exact(
                    stream,
                    min(remaining, BUFFER_SIZE),
                    f"file data for {entry.relative_path}",
                )
                checksum = zlib.crc32(chunk, checksum)
                remaining -= len(chunk)
            if checksum & 0xFFFFFFFF != entry.data_crc32:
                raise LdPakError(
                    f"Asset package file checksum does not match: {entry.relative_path}"
                )
            expected_offset = entry.data_offset + entry.data_size
        aligned_index_offset = _align(expected_offset)
        if index_offset != aligned_index_offset:
            raise LdPakError("Asset package file data does not end at its index")
        _require_zero_bytes(
            stream,
            expected_offset,
            index_offset - expected_offset,
            "final data padding",
        )
    return tuple(archive_entries)


def validate_ldpak(
    package_path: pathlib.Path,
    expected_group: str | None = None,
    require_matching_filename: bool = True,
) -> int:
    return len(
        _validate_ldpak_entries(
            package_path,
            expected_group,
            require_matching_filename,
        )
    )


def _validate_group_directories(
    group_root: pathlib.Path,
) -> tuple[pathlib.Path, ...]:
    group_root = pathlib.Path(group_root)
    root_name = group_root.name
    if _is_link(group_root):
        raise LdPakError(
            f"{root_name} root must not be a symbolic link: {group_root}"
        )
    if not group_root.is_dir():
        raise LdPakError(f"{root_name} directory was not found: {group_root}")

    try:
        root_entries = list(os.scandir(group_root))
    except OSError as exception:
        raise LdPakError(
            f"Unable to enumerate {root_name} directory: {group_root}"
        ) from exception
    groups: list[pathlib.Path] = []
    root_files: list[pathlib.Path] = []
    for entry in root_entries:
        path = pathlib.Path(entry.path)
        if entry.is_symlink() or _is_link(path):
            raise LdPakError(
                f"{root_name} must not contain symbolic links: {path}"
            )
        if entry.is_file(follow_symlinks=False) and entry.name == ".DS_Store":
            continue
        if entry.is_dir(follow_symlinks=False):
            _validate_group_name(entry.name)
            groups.append(path)
        elif entry.is_file(follow_symlinks=False):
            root_files.append(path)
        else:
            raise LdPakError(
                f"{root_name} supports only first-level directories when packing: {path}"
            )
    if root_files:
        relative_paths = ", ".join(
            path.relative_to(group_root).as_posix()
            for path in sorted(
                root_files,
                key=lambda path: _encode_utf8(path.name, "Asset root filename"),
            )
        )
        raise LdPakError(
            f"{root_name} root files cannot be packed into directory groups: {relative_paths}"
        )

    groups.sort(key=lambda path: _encode_utf8(path.name, "Asset group name"))
    folded_groups: dict[str, str] = {}
    for group in groups:
        folded = group.name.casefold()
        previous = folded_groups.get(folded)
        if previous is not None:
            raise LdPakError(
                f"Asset group names differ only by case: {previous!r} and {group.name!r}"
            )
        folded_groups[folded] = group.name
        output = group_root / (group.name + ".ldpak")
        if os.path.lexists(output):
            raise LdPakError(f"Asset package output already exists: {output}")
    for group in groups:
        _scan_group(group)
    return tuple(groups)


def _script_file_paths(entries: tuple[_ArchiveEntry, ...] | list[_SourceEntry]) -> set[str]:
    paths: set[str] = set()
    for entry in entries:
        is_directory = (
            entry.is_directory
            if isinstance(entry, _SourceEntry)
            else bool(entry.flags & DIRECTORY_FLAG)
        )
        if not is_directory:
            paths.add(entry.relative_path)
    return paths


def _validate_script_entries(
    entries: tuple[_ArchiveEntry, ...] | list[_SourceEntry],
    description: pathlib.Path,
    expected_entry: str | None = None,
    reject_declarations: bool = False,
) -> None:
    files = _script_file_paths(entries)
    paths = {entry.relative_path for entry in entries}
    entries_found = set(SCRIPT_ENTRY_PATHS) & files
    if not entries_found:
        raise LdPakError(
            f"Scripts source is missing Entry.lua or Entry.luac: {description}"
        )
    if expected_entry is not None and expected_entry not in files:
        raise LdPakError(
            f"Scripts source is missing the expected {expected_entry}: {description}"
        )
    if reject_declarations:
        forbidden = sorted(
            path
            for path in paths
            if path.casefold().endswith(".d.lua")
            or path == "stub"
            or path.startswith("stub/")
        )
        if forbidden:
            raise LdPakError(
                "Scripts runtime contains declaration files or the stub tree: "
                + ", ".join(forbidden)
            )


def validate_ldpak_source(
    runtime_root: pathlib.Path,
) -> tuple[pathlib.Path, ...]:
    runtime_root = pathlib.Path(runtime_root)
    if _is_link(runtime_root):
        raise LdPakError(
            f"Runtime root must not be a symbolic link: {runtime_root}"
        )
    if not runtime_root.is_dir():
        raise LdPakError(f"Runtime root was not found: {runtime_root}")
    scripts_root = runtime_root / SCRIPT_GROUP
    scripts_package = runtime_root / f"{SCRIPT_GROUP}.ldpak"
    if os.path.lexists(scripts_package):
        raise LdPakError(f"Script package output already exists: {scripts_package}")
    groups = (
        *_validate_group_directories(runtime_root / "Assets"),
        *_validate_group_directories(runtime_root / "Data"),
    )
    entries = _scan_group(scripts_root)
    _validate_script_entries(entries, scripts_root)
    return groups


def validate_runtime_scripts(
    runtime_root: pathlib.Path,
    expected_entry: str | None = None,
) -> bool:
    runtime_root = pathlib.Path(runtime_root)
    scripts_root = runtime_root / SCRIPT_GROUP
    scripts_package = runtime_root / f"{SCRIPT_GROUP}.ldpak"
    has_loose_scripts = os.path.lexists(scripts_root)
    has_packed_scripts = os.path.lexists(scripts_package)
    if has_loose_scripts == has_packed_scripts:
        raise LdPakError(
            "Runtime must contain exactly one of Scripts or Scripts.ldpak: "
            f"{runtime_root}"
        )
    if has_packed_scripts:
        entries: tuple[_ArchiveEntry, ...] | list[_SourceEntry] = (
            _validate_ldpak_entries(
                scripts_package,
                expected_group=SCRIPT_GROUP,
            )
        )
        description = scripts_package
    else:
        entries = _scan_group(scripts_root)
        description = scripts_root
    _validate_script_entries(
        entries,
        description,
        expected_entry,
        reject_declarations=True,
    )
    return has_packed_scripts


def _validate_packed_group_root(group_root: pathlib.Path) -> None:
    if _is_link(group_root) or not group_root.is_dir():
        raise LdPakError(f"Packed runtime directory was not found: {group_root}")
    try:
        entries = list(os.scandir(group_root))
    except OSError as exception:
        raise LdPakError(
            f"Unable to enumerate packed runtime directory: {group_root}"
        ) from exception
    names: dict[str, str] = {}
    for entry in entries:
        path = pathlib.Path(entry.path)
        if entry.is_symlink() or _is_link(path):
            raise LdPakError(
                f"Packed runtime directories must not contain symbolic links: {path}"
            )
        if entry.is_file(follow_symlinks=False) and entry.name == ".DS_Store":
            continue
        if (
            not entry.is_file(follow_symlinks=False)
            or not entry.name.casefold().endswith(".ldpak")
        ):
            raise LdPakError(
                f"Packed runtime directories may contain only .ldpak files: {path}"
            )
        group_name = entry.name[: -len(".ldpak")]
        folded = group_name.casefold()
        previous = names.get(folded)
        if previous is not None:
            raise LdPakError(
                f"Packed group names differ only by case: {previous!r} and {group_name!r}"
            )
        names[folded] = group_name
        validate_ldpak(path, expected_group=group_name)


def validate_runtime_ldpak_layout(
    runtime_root: pathlib.Path,
    expected_use_ldpak: bool | None = None,
    expected_entry: str | None = None,
) -> bool:
    runtime_root = pathlib.Path(runtime_root)
    use_ldpak = validate_runtime_scripts(runtime_root, expected_entry)
    if expected_use_ldpak is not None and use_ldpak != expected_use_ldpak:
        raise LdPakError(
            f"Runtime has the wrong loose/packed resource layout: {runtime_root}"
        )
    for name in ("Assets", "Data"):
        group_root = runtime_root / name
        if use_ldpak:
            _validate_packed_group_root(group_root)
        else:
            _validate_group_directories(group_root)
    return use_ldpak


def pack_ldpak(runtime_root: pathlib.Path) -> int:
    runtime_root = pathlib.Path(runtime_root)
    groups = validate_ldpak_source(runtime_root)
    sources = (*groups, runtime_root / SCRIPT_GROUP)
    outputs = (
        *(group.parent / f"{group.name}.ldpak" for group in groups),
        runtime_root / f"{SCRIPT_GROUP}.ldpak",
    )
    stage_root = pathlib.Path(
        tempfile.mkdtemp(
            prefix=f".{runtime_root.name}-ldpak-stage-",
            dir=runtime_root.parent,
        )
    )
    temporary_packages: list[tuple[pathlib.Path, pathlib.Path, pathlib.Path]] = []
    installed_packages: list[pathlib.Path] = []
    moved_sources: list[tuple[pathlib.Path, pathlib.Path]] = []
    try:
        for index, (source, output) in enumerate(zip(sources, outputs, strict=True)):
            temporary = stage_root / f"{index}.ldpak"
            write_ldpak(source, temporary)
            validate_ldpak(
                temporary,
                expected_group=source.name,
                require_matching_filename=False,
            )
            temporary_packages.append((source, temporary, output))

        for _, temporary, output in temporary_packages:
            temporary.replace(output)
            installed_packages.append(output)
        backup_root = stage_root / "source"
        for source, _, _ in temporary_packages:
            backup = backup_root / source.relative_to(runtime_root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            source.replace(backup)
            moved_sources.append((source, backup))
    except BaseException:
        for output in installed_packages:
            if os.path.lexists(output):
                output.unlink()
        for source, backup in reversed(moved_sources):
            if os.path.lexists(backup):
                source.parent.mkdir(parents=True, exist_ok=True)
                backup.replace(source)
        if stage_root.exists():
            _discard_tree(stage_root)
        raise
    _discard_tree(stage_root)
    return len(groups)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools validate-ldpak-source")
    parser.add_argument("runtime_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        groups = validate_ldpak_source(parsed.runtime_root)
    except LdPakError as exception:
        print(str(exception), file=sys.stderr)
        return 1
    print(
        f"Validated {len(groups)} Assets/Data package source directories and Scripts"
    )
    return 0
