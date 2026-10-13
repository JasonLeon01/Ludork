from __future__ import annotations

import argparse
import json
import pathlib

from .editor_versions import read_editor_versions


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools configure-project-template")
    parser.add_argument("project_file", type=pathlib.Path)
    parser.add_argument("cpp", choices=("true", "false"))
    parser.add_argument("ffmpeg", choices=("true", "false"))
    parser.add_argument("--versions-config", type=pathlib.Path, required=True)
    parser.add_argument("--version")
    parsed = parser.parse_args(arguments)
    projectFile = parsed.project_file.resolve()
    data: dict[str, object] = {}
    if projectFile.is_file():
        data = json.loads(projectFile.read_text(encoding="utf-8"))
    data["Cpp"] = parsed.cpp == "true"
    data["version"] = read_editor_versions(parsed.versions_config, parsed.version).game_version
    data["packaging"] = {"version": "1.0.0", "dev": False}
    if parsed.ffmpeg == "true":
        data["ffmpeg"] = True
    else:
        data.pop("ffmpeg", None)
    projectFile.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return 0
