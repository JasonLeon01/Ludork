from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SERVER_KEYS = ("SERVER_NODE_VERSION", "SERVER_NPM_VERSION", "SERVER_PYTHON_VERSION")


def read_versions(root: Path = ROOT) -> dict[str, str]:
    return dict(re.findall(r"^([A-Z][A-Z0-9_]*)=([0-9][0-9A-Za-z.+-]*)$",
                           (root / "versions.conf").read_text(encoding="utf-8-sig"), re.MULTILINE))


def write_editor_sdk(root: Path = ROOT) -> None:
    version = read_versions(root).get("DOTNET_SDK_VERSION", "")
    if re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version) is None:
        raise ValueError("DOTNET_SDK_VERSION in versions.conf must be a full release SDK version.")
    content = (json.dumps({"sdk": {
        "version": version,
        "rollForward": "disable",
        "allowPrerelease": False,
    }}, indent=2) + "\n").encode("utf-8")
    path = root / "global.json"
    if not path.is_file() or path.read_bytes() != content:
        path.write_bytes(content)
    print(f"Editor .NET SDK {version}: {path}", flush=True)


def require_version(name: str, actual: str, expected: str) -> None:
    if actual != expected:
        raise ValueError(f"{name}: expected {expected}, received {actual or '<missing>'}. "
                         "Use the toolchain configured in versions.conf.")
    print(f"{name}: {actual}", flush=True)


def command_version(command: list[str], pattern: str) -> str:
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace", check=False)
    match = re.search(pattern, result.stdout)
    if match is None or result.returncode != 0:
        raise ValueError(f"Unable to read the version of {command[0]}: {result.stdout.strip()}")
    return match[1]


def verify_server(versions: dict[str, str]) -> None:
    require_version("Python", platform.python_version(), versions["SERVER_PYTHON_VERSION"])
    require_version("Node.js", command_version(["node", "--version"], r"^v([\d.]+)"), versions["SERVER_NODE_VERSION"])
    npm = shutil.which("npm.cmd" if os.name == "nt" else "npm")
    if npm is None:
        raise ValueError("npm was not found.")
    require_version("npm", command_version([npm, "--version"], r"^([\d.]+)"), versions["SERVER_NPM_VERSION"])


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepare editor SDK configuration or verify the independent Server toolchain.")
    operations = parser.add_subparsers(dest="operation", required=True)
    operations.add_parser("editor-sdk", help="Generate the ignored global.json from versions.conf")
    for operation in ("export", "verify"):
        operations.add_parser(operation).add_argument("target", choices=("server",))
    args = parser.parse_args()
    if args.operation == "editor-sdk":
        write_editor_sdk()
        return
    versions = read_versions()
    keys = SERVER_KEYS
    for key in keys:
        if key not in versions:
            raise ValueError(f"Missing {key} in versions.conf.")
    if args.operation == "export":
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8", newline="\n") as output:
            for key in keys:
                output.write(f"{key}={versions[key]}\n")
    else:
        verify_server(versions)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
