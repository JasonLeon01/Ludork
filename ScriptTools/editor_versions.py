from __future__ import annotations

import json
import pathlib
import re
import shlex
from dataclasses import dataclass

from .pack_error import PackError
from .packaging_metadata import read_release_version


@dataclass(frozen=True)
class EditorVersions:
    base_version: str
    game_version: str
    supported_game_versions: tuple[str, ...]


def read_editor_versions(path: pathlib.Path, version: str | None = None) -> EditorVersions:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        key, separator, value = line.partition("=")
        if separator and key.strip() in ("EDITOR_VERSION", "GAME_VERSIONS"):
            tokens = shlex.split(value, comments=True)
            if len(tokens) != 1:
                raise PackError(f"Invalid {key} in {path}")
            values[key.strip()] = tokens[0]
    base_version = version if version is not None else values.get("EDITOR_VERSION", "")
    read_release_version(base_version)
    game_version = "v" + ".".join(base_version.split(".")[:2])
    try:
        supported = json.loads(values.get("GAME_VERSIONS", ""))
    except ValueError as error:
        raise PackError(f"GAME_VERSIONS must be a JSON array in {path}") from error
    if not isinstance(supported, list) or not supported or any(
        not isinstance(value, str)
        or re.fullmatch(r"v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", value) is None
        for value in supported
    ):
        raise PackError(f"GAME_VERSIONS must contain versions such as v1.0 in {path}")
    if len(supported) != len(set(supported)) or game_version not in supported:
        raise PackError(f"GAME_VERSIONS must contain {game_version} and have no duplicate entries in {path}")
    return EditorVersions(base_version, game_version, tuple(supported))
