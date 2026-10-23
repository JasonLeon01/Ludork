from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import tarfile


def archive(source: pathlib.Path, revision: str, output: pathlib.Path) -> pathlib.Path:
    if re.fullmatch(r"[0-9a-f]{40}", revision) is None:
        raise ValueError("A full Server commit SHA is required.")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if head != revision:
        raise ValueError("The Server checkout does not match its resolved revision.")
    entries = subprocess.check_output(["git", "ls-tree", "-rz", revision], cwd=source).split(b"\0")
    paths = {}
    excluded = {"node_modules", "data", "config", "projects", "logs", "dist", "coverage", ".vite", ".git"}
    for entry in filter(None, entries):
        metadata, name = entry.split(b"\t", 1)
        mode, kind, _ = metadata.split()
        path = pathlib.PurePosixPath(name.decode("utf-8"))
        if (mode not in (b"100644", b"100755") or kind != b"blob"
                or any(part.lower() in excluded for part in path.parts)
                or path.name.startswith(".env") or path.name.lower() in {"start.sh", "start.bat", "buildinfo.json"}
                or path.suffix.lower() in {".log", ".tmp"}):
            raise ValueError(f"Non-source content is tracked in Server: {path}")
        paths[path.as_posix()] = int(mode, 8) & 0o777
    required = {
        "package.json", "package-lock.json", "deploy.sh", "deploy.bat", "README.md",
        "backend/lua-worker.ts", "backend/net-server.ts", "backend/wire.ts",
        "lua/Framework/Codec.lua", "lua/Framework/Net.lua",
        "lua/Modules/Storage.lua", "lua/templates/Main.lua",
        "third-party/README.md", "third-party/wasmoon-LICENSE.txt",
        "third-party/ws-LICENSE.txt", "third-party/lua-LICENSE.txt",
    }
    if not required.issubset(paths) or not any(name.startswith("backend/") for name in paths) \
            or not any(name.startswith("frontend/") for name in paths):
        raise ValueError("The Server revision is missing required deployment sources.")
    output.mkdir(parents=True, exist_ok=True)
    destination = output.resolve() / f"LudorkServer-source-{revision}.tar.gz"
    subprocess.run(["git", "-c", "tar.umask=0022", "archive", "--format=tar.gz", "--prefix=LudorkServer/",
                    f"--output={destination}", revision], cwd=source, check=True)
    with tarfile.open(destination, "r:gz") as package:
        if {entry.name for entry in package.getmembers()} != {
            *(f"LudorkServer/{name}" for name in paths),
            *(f"LudorkServer/{parent.as_posix()}" for name in paths
              for parent in pathlib.PurePosixPath(name).parents if parent.as_posix() != "."),
            "LudorkServer",
        }:
            raise ValueError("The Server source archive does not match its source tree.")
        for name, mode in paths.items():
            entry = package.getmember(f"LudorkServer/{name}")
            if not entry.isfile() or entry.mode != mode:
                raise ValueError(f"The Server source archive changed file permissions: {name}")
            with package.extractfile(entry) as stream:
                while stream.read(1024 * 1024):
                    pass
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("revision")
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    print(archive(args.source, args.revision, args.output))
