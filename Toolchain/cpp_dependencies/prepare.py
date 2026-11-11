from __future__ import annotations

import hashlib
import os
import pathlib
import shutil
import subprocess
import tempfile
from concurrent.futures import CancelledError, ThreadPoolExecutor, as_completed

from Toolchain.file_replace import remove_file, remove_tree, replace_path
from Toolchain.versions import read_versions

from .archives import extract_archive
from .catalog import DEPENDENCY_NAMES, Dependency, dependency
from .session import PreparationSession, project_lock


class RecoveryError(RuntimeError):
    pass


def _require_owned_path(project: pathlib.Path, path: pathlib.Path) -> None:
    relative = path.relative_to(project)
    current = project
    for part in relative.parts:
        current /= part
        if current.is_symlink() or current.is_junction():
            raise ValueError(f"Dependency paths must not contain links: {current}")


def _remove(path: pathlib.Path) -> None:
    if path.is_symlink() or path.is_file():
        remove_file(path)
    elif path.exists():
        remove_tree(path)


def _ready(spec: Dependency, root: pathlib.Path) -> bool:
    if spec.marker is None or spec.local_archive is not None:
        return False
    for target in spec.targets:
        directory = root / target.destination
        marker = directory / ".ludork-version"
        if not marker.is_file() or marker.read_text(encoding="utf-8-sig").strip() != spec.marker:
            return False
        if not all((directory / name).is_file() for name in target.required_files):
            return False
    return True


def download_archive(spec: Dependency, destination: pathlib.Path, session: PreparationSession) -> None:
    curl = shutil.which("curl.exe" if os.name == "nt" else "curl")
    if curl is None:
        raise RuntimeError("curl was not found.")
    print(f"Downloading {spec.label}...", flush=True)
    session.run([
        curl, "--location", "--fail", "--show-error", "--silent",
        "--retry", "3", "--retry-all-errors", "--retry-delay", "5", "--retry-max-time", "900",
        "--connect-timeout", "15", "--max-time", "300", "--output", str(destination), spec.url,
    ])


def _validate_source(spec: Dependency, extracted: pathlib.Path) -> None:
    for target in spec.targets:
        source = extracted / target.source
        if source.is_symlink() or not source.is_dir():
            raise ValueError(f"{spec.label} source folder was not found after extraction: {target.source}")
        for relative in target.required_files:
            if not (source / relative).is_file():
                raise ValueError(f"{spec.label} source archive is missing {target.source}/{relative}")
        if spec.release_version is not None:
            actual = (source / "RELEASE").read_text(encoding="utf-8").strip()
            if actual != spec.release_version:
                raise ValueError(f"FFmpeg release version mismatch: expected {spec.release_version}, got {actual}.")
        if spec.marker is not None:
            marker = source / ".ludork-version"
            if marker.is_symlink():
                raise ValueError(f"Dependency version marker must not be a link: {marker}")
            marker.write_text(spec.marker + "\n", encoding="utf-8", newline="\n")


def publish_sources(replacements: list[tuple[pathlib.Path, pathlib.Path]], work: pathlib.Path,
                    session: PreparationSession) -> None:
    session.check()
    backup_root = work / "previous"
    backup_root.mkdir()
    backups: list[tuple[pathlib.Path, pathlib.Path]] = []
    published: list[pathlib.Path] = []
    try:
        for index, (source, destination) in enumerate(replacements):
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                backup = backup_root / str(index)
                replace_path(destination, backup)
                backups.append((backup, destination))
            replace_path(source, destination)
            published.append(destination)
    except BaseException as error:
        rollback_errors: list[str] = []
        for destination in reversed(published):
            try:
                _remove(destination)
            except OSError as rollback_error:
                rollback_errors.append(f"{destination}: {rollback_error}")
        for backup, destination in reversed(backups):
            try:
                replace_path(backup, destination)
            except OSError as rollback_error:
                rollback_errors.append(f"{backup} -> {destination}: {rollback_error}")
        if rollback_errors:
            raise RecoveryError(
                f"Dependency publication failed: {error}\nRollback also failed; recovery files remain at {work}:\n"
                + "\n".join(rollback_errors)
            ) from error
        raise


def prepare_dependency(project: pathlib.Path, spec: Dependency, session: PreparationSession) -> None:
    session.check()
    root = project / "Engine" / "ThirdParty"
    if _ready(spec, root):
        print(f"Using existing {spec.label} {spec.marker}.", flush=True)
        return
    work = pathlib.Path(tempfile.mkdtemp(prefix=f".ludork-{spec.name}-", dir=root))
    preserve_work = False
    try:
        archive = work / spec.archive_name
        retained = project / spec.retained_archive if spec.retained_archive is not None else None
        publish_archive = False
        if spec.local_archive is not None:
            shutil.copyfile(spec.local_archive, archive)
        elif retained is not None and retained.is_file():
            print(f"Using existing FFmpeg {spec.release_version} source archive.", flush=True)
            shutil.copyfile(retained, archive)
        else:
            download_archive(spec, archive, session)
            publish_archive = retained is not None
        session.check()
        if spec.sha256 is not None:
            with archive.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
            if actual != spec.sha256:
                raise ValueError(f"{spec.label} SHA-256 mismatch: expected {spec.sha256}, got {actual}")
        extracted = work / "extracted"
        extract_archive(archive, extracted, session)
        _validate_source(spec, extracted)
        replacements = [(extracted / target.source, root / target.destination) for target in spec.targets]
        if publish_archive:
            assert retained is not None
            replacements.append((archive, retained))
        publish_sources(replacements, work, session)
        print(f"{spec.label} source is ready.", flush=True)
    except RecoveryError:
        preserve_work = True
        raise
    finally:
        if not preserve_work:
            remove_tree(work)


def _gnu_make(repository: pathlib.Path, session: PreparationSession) -> None:
    script = repository / "tools" / "init_gnu_make.bat"
    if not script.is_file():
        raise ValueError(f"GNU Make preparation script was not found: {script}")
    environment = os.environ.copy()
    environment["LUDORK_GNU_MAKE_PREPARATION"] = str(script)
    command = (subprocess.list2cmdline([environment.get("COMSPEC", "cmd.exe")])
               + ' /d /s /c ""%LUDORK_GNU_MAKE_PREPARATION%""')
    session.run(command, cwd=repository, environment=environment)


def prepare_dependencies(project: pathlib.Path, repository_root: pathlib.Path,
                         selected: list[str] | None = None) -> None:
    project = project.expanduser().resolve(strict=True)
    repository = repository_root.expanduser().resolve(strict=True)
    if not (project / "CMakeLists.txt").is_file():
        raise ValueError(f"CMakeLists.txt was not found: {project}")
    names = list(dict.fromkeys(selected if selected is not None else DEPENDENCY_NAMES))
    if not names or any(name not in DEPENDENCY_NAMES for name in names):
        raise ValueError("Select at least one known native dependency.")
    values = read_versions(repository / "versions.conf")
    local_name = os.environ.get("LUASF_SOURCE_ARCHIVE", "").strip()
    local = pathlib.Path(local_name).expanduser().resolve(strict=True) if local_name and "luasf" in names else None
    specs = [dependency(name, values, windows=os.name == "nt", local_luasf_archive=local) for name in names]
    root = project / "Engine" / "ThirdParty"
    _require_owned_path(project, root)
    for spec in specs:
        for target in spec.targets:
            _require_owned_path(project, root / target.destination)
        if spec.retained_archive is not None:
            _require_owned_path(project, project / spec.retained_archive)
    session = PreparationSession()
    with project_lock(project), session.signals():
        root.mkdir(parents=True, exist_ok=True)
        failures: list[str] = []
        with ThreadPoolExecutor(max_workers=len(specs), thread_name_prefix="ludork-dependency") as executor:
            pending = {executor.submit(prepare_dependency, project, spec, session): spec for spec in specs}
            try:
                for future in as_completed(pending):
                    try:
                        future.result()
                    except CancelledError:
                        session.cancelled.set()
                    except Exception as error:
                        failures.append(f"{pending[future].label}: {error}")
            except BaseException:
                session.cancelled.set()
                raise
        session.check()
        if failures:
            raise RuntimeError("Native dependency preparation failed:\n" + "\n".join(failures))
        if os.name == "nt" and selected is None:
            _gnu_make(repository, session)
        session.check()
    print(f"C++ dependencies are ready in {project}", flush=True)
