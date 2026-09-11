from __future__ import annotations

import argparse
import contextlib
import json
import pathlib
import sys

from ScriptTools import native_build_state
from ScriptTools.ui_asset_generation import generation_manifest


PROTOCOL_VERSION = 1


def execute(project: pathlib.Path, request: dict) -> dict:
    if request.get("version") != PROTOCOL_VERSION:
        raise ValueError("Unsupported project state protocol version.")
    operation = request.get("operation")
    if operation == "ready":
        return {}
    if operation == "native-check":
        configuration = request.get("configuration")
        if configuration not in ("Debug", "Release"):
            raise ValueError("Invalid native build configuration.")
        if not (project / "CMakeLists.txt").is_file():
            raise ValueError(f"CMakeLists.txt was not found: {project}")
        current, detail = native_build_state.check(project, configuration)
        return {"current": current, "detail": detail}
    if operation == "ui-manifest":
        return generation_manifest(project)
    raise ValueError(f"Unknown project state operation: {operation}")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools project-state-worker")
    parser.add_argument("project", type=pathlib.Path)
    args = parser.parse_args(arguments)
    project = args.project.expanduser().resolve()
    if not project.is_dir():
        print(f"Project directory was not found: {project}", file=sys.stderr)
        return 2
    for line in sys.stdin:
        request_id = None
        try:
            request = json.loads(line)
            if not isinstance(request, dict) or type(request.get("id")) is not int:
                raise ValueError("Project state requests require an integer id.")
            request_id = request["id"]
            with contextlib.redirect_stdout(sys.stderr):
                result = execute(project, request)
            response = {"id": request_id, "result": result}
        except Exception as error:
            response = {"id": request_id, "error": str(error)}
        print(json.dumps(response, ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
