from __future__ import annotations

import argparse
from html.parser import HTMLParser
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import shutil
import subprocess
import tarfile


ROOT = Path(__file__).resolve().parent.parent
REQUIRED = {
    "package.json", "package-lock.json", "README.md", ".emmyrc.json",
    "deploy.sh", "deploy.bat", "start.sh", "start.bat",
    "tools/deploy.mjs", "tools/start.mjs",
    "backend/main.js", "backend/management.js", "backend/lua-worker.js",
    "frontend/index.html", "lua/Framework/Net.lua", "lua/Framework/Codec.lua",
    "lua/Modules/Storage.lua", "lua/templates/Main.lua",
    "third-party/README.md", "third-party/wasmoon-LICENSE.txt",
    "third-party/ws-LICENSE.txt", "third-party/lua-LICENSE.txt",
    "third-party/react-LICENSE.txt", "third-party/react-dom-LICENSE.txt",
    "third-party/scheduler-LICENSE.txt",
}
ROOT_FILES = {name for name in REQUIRED if "/" not in name}


class AssetReferences(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.paths: set[str] = set()

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for key, value in attrs:
            if key in {"src", "href"} and value and value.startswith("/assets/"):
                self.paths.add("frontend" + value.split("?", 1)[0].split("#", 1)[0])


def check_path(name: str, directory: bool) -> str:
    path = PurePosixPath(name)
    parts = name.split("/")
    if ("\\" in name or ":" in name or any(part in ("", ".", "..") for part in parts)
            or parts[0] != "LudorkServer"):
        raise ValueError(f"Invalid Server package path: {name}")
    relative = path.relative_to("LudorkServer").as_posix()
    if directory and relative == ".":
        return relative
    top = parts[1] if len(parts) > 1 else ""
    if directory:
        allowed = top in {"backend", "frontend", "lua", "third-party", "tools"}
    else:
        allowed = (relative in ROOT_FILES
                   or (top == "backend" and path.suffix == ".js")
                   or relative == "frontend/index.html"
                   or (relative.startswith("frontend/assets/") and path.suffix in {
                       ".js", ".css", ".svg", ".png", ".jpg", ".jpeg", ".webp", ".ico", ".woff", ".woff2"
                   })
                   or (top == "lua" and path.suffix == ".lua")
                   or (top == "third-party" and path.suffix in {".md", ".txt"})
                   or relative in {"tools/deploy.mjs", "tools/start.mjs"})
    if not allowed:
        raise ValueError(f"Unexpected content in the built Server package: {name}")
    return relative


def verify(archive: Path) -> None:
    files: set[str] = set()
    entries: set[str] = set()
    size = 0
    with tarfile.open(archive, "r:gz") as package:
        for entry in package:
            if not entry.isfile() and not entry.isdir():
                raise ValueError(f"Server package contains a link or special file: {entry.name}")
            relative = check_path(entry.name, entry.isdir())
            if relative in entries:
                raise ValueError(f"Duplicate Server package entry: {entry.name}")
            entries.add(relative)
            size += entry.size
            if len(entries) > 4096 or size > 128 * 1024 * 1024:
                raise ValueError("The built Server package exceeds its packaging limits.")
            if entry.isfile():
                files.add(relative)
                if entry.size == 0:
                    raise ValueError(f"Empty Server package file: {entry.name}")
                if entry.mode != (0o755 if relative.endswith(".sh") else 0o644):
                    raise ValueError(f"Unexpected Server package permissions: {entry.name}")
                with package.extractfile(entry) as content:
                    while content.read(1024 * 1024):
                        pass
        if not REQUIRED.issubset(files) or not any(
            name.startswith("frontend/assets/") and name.endswith(".js") for name in files
        ):
            raise ValueError(f"The Server package is missing build outputs: {sorted(REQUIRED - files)}")
        for name in files:
            if name.startswith(("backend/", "tools/")) and name.endswith((".js", ".mjs")):
                with package.extractfile("LudorkServer/" + name) as stream:
                    contents = stream.read().decode("utf-8")
                for reference in re.findall(r"['\"](\.{1,2}/[^'\"]+\.js)['\"]", contents):
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(name), reference))
                    if target not in files:
                        raise ValueError(f"Missing Server JavaScript dependency: {name} -> {reference}")
        with package.extractfile("LudorkServer/frontend/index.html") as stream:
            assets = AssetReferences()
            assets.feed(stream.read().decode("utf-8"))
        if not assets.paths or not assets.paths.issubset(files):
            raise ValueError(f"Missing frontend build assets: {sorted(assets.paths - files)}")
        with package.extractfile("LudorkServer/package.json") as stream:
            manifest = json.load(stream)
        with package.extractfile("LudorkServer/package-lock.json") as stream:
            lock = json.load(stream)
        if manifest.get("devDependencies") or "build" in manifest.get("scripts", {}):
            raise ValueError("The Server package contains a development manifest.")
        if manifest.get("scripts", {}).get("server") != "node backend/main.js":
            raise ValueError("The Server package must start its compiled JavaScript backend.")
        dependencies = manifest.get("dependencies", {})
        if not {"express", "wasmoon", "ws"}.issubset(dependencies):
            raise ValueError("The Server package is missing runtime dependencies.")
        if lock.get("packages", {}).get("", {}).get("dependencies") != dependencies:
            raise ValueError("The Server package lock does not match its runtime manifest.")
        if any(item.get("dev") for item in lock.get("packages", {}).values()):
            raise ValueError("The Server package lock contains development dependencies.")


def server_worktree() -> Path:
    output = subprocess.check_output(
        ["git", "worktree", "list", "--porcelain", "-z"], cwd=ROOT
    ).decode("utf-8")
    worktree = None
    for field in output.split("\0"):
        if field.startswith("worktree "):
            worktree = Path(field[len("worktree "):])
        elif field == "branch refs/heads/Server" and worktree is not None:
            return worktree
    raise ValueError("Check out the Server worktree, or select its checkout with --source.")


def build(source: Path | None, output: Path, revision: str | None) -> None:
    source = (source or server_worktree()).resolve(strict=True)
    if revision is not None:
        if re.fullmatch(r"[0-9a-f]{40}", revision) is None:
            raise ValueError("A full Server commit SHA is required.")
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
        status = subprocess.check_output(["git", "status", "--porcelain"], cwd=source, text=True)
        if head != revision or status:
            raise ValueError("The Server checkout must be clean and match its resolved revision.")
    if not (source / "package.json").is_file():
        raise ValueError(f"The Server checkout has no package.json: {source}")
    node = shutil.which("node")
    npm = shutil.which("npm.cmd" if os.name == "nt" else "npm")
    if not node or not npm:
        raise ValueError("Node.js 24 and npm are required to build the Server package.")
    if not subprocess.check_output([node, "--version"], text=True).startswith("v24."):
        raise ValueError("Node.js 24 is required to build the Server package.")
    output = output.resolve()
    built = source / "dist" / "LudorkServer"
    if output.is_relative_to(source / "dist"):
        raise ValueError("The archive must be outside the Server build output directory.")
    subprocess.run([npm, "ci"], cwd=source, check=True)
    subprocess.run([npm, "run", "build"], cwd=source, check=True)
    if not built.is_dir() or built.is_symlink():
        raise ValueError("Server build did not produce dist/LudorkServer.")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    try:
        with tarfile.open(temporary, "w:gz") as package:
            for path in [built, *sorted(built.rglob("*"))]:
                if path.is_symlink() or not (path.is_file() or path.is_dir()):
                    raise ValueError(f"Unexpected Server build entry: {path}")
                name = "LudorkServer" + ("/" + path.relative_to(built).as_posix() if path != built else "")
                check_path(name, path.is_dir())
                entry = package.gettarinfo(str(path), arcname=name)
                entry.uid = entry.gid = 0
                entry.uname = entry.gname = ""
                entry.mode = 0o755 if path.is_dir() or path.suffix == ".sh" else 0o644
                if path.is_file():
                    with path.open("rb") as stream:
                        package.addfile(entry, stream)
                else:
                    package.addfile(entry)
        verify(temporary)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build and validate deployable Ludork Server archives.")
    commands = parser.add_subparsers(dest="command", required=True)
    build_parser = commands.add_parser("build")
    build_parser.add_argument("--source", type=Path)
    build_parser.add_argument("--output", type=Path, required=True)
    build_parser.add_argument("--revision")
    verify_parser = commands.add_parser("verify")
    verify_parser.add_argument("archive", type=Path)
    arguments = parser.parse_args()
    if arguments.command == "build":
        build(arguments.source, arguments.output, arguments.revision)
    else:
        verify(arguments.archive)
        print(f"Verified built Server package: {arguments.archive}")


if __name__ == "__main__":
    main()
