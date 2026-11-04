from __future__ import annotations

import os
import pathlib
import stat
import tarfile
import zipfile

from .session import PreparationSession


def _entry_path(name: str, destination: pathlib.Path) -> pathlib.Path:
    relative = pathlib.PurePosixPath(name)
    if not name or "\\" in name or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"Invalid dependency archive path: {name}")
    if relative.parts and ":" in relative.parts[0]:
        raise ValueError(f"Invalid dependency archive path: {name}")
    output = destination.joinpath(*relative.parts)
    if not output.resolve().is_relative_to(destination):
        raise ValueError(f"Dependency archive path escapes extraction directory: {name}")
    return output


def extract_archive(archive: pathlib.Path, destination: pathlib.Path, session: PreparationSession) -> None:
    destination.mkdir(parents=True)
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as package:
            for entry in package.infolist():
                session.check()
                output = _entry_path(entry.filename, destination)
                mode = entry.external_attr >> 16
                if entry.is_dir():
                    output.mkdir(parents=True, exist_ok=True)
                    continue
                output.parent.mkdir(parents=True, exist_ok=True)
                if stat.S_ISLNK(mode):
                    target = package.read(entry).decode("utf-8")
                    if pathlib.PurePosixPath(target).is_absolute() or "\\" in target:
                        raise ValueError(f"Invalid dependency archive link: {entry.filename}")
                    if not (output.parent / target).resolve().is_relative_to(destination):
                        raise ValueError(f"Dependency archive link escapes extraction directory: {entry.filename}")
                    output.symlink_to(target)
                elif not stat.S_IFMT(mode) or stat.S_ISREG(mode):
                    with package.open(entry) as source, output.open("wb") as target:
                        while chunk := source.read(1024 * 1024):
                            session.check()
                            target.write(chunk)
                    if os.name != "nt" and stat.S_IMODE(mode):
                        output.chmod(stat.S_IMODE(mode) & 0o777)
                else:
                    raise ValueError(f"Unsupported dependency archive entry: {entry.filename}")
    else:
        def filtering(entry: tarfile.TarInfo, directory: str) -> tarfile.TarInfo | None:
            session.check()
            _entry_path(entry.name, destination)
            return tarfile.data_filter(entry, directory)

        with tarfile.open(archive, "r:gz") as package:
            package.extractall(destination, filter=filtering)
    session.check()
