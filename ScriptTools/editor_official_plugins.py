from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import stat
import tempfile


OFFICIAL_PLUGIN_DIRECTORIES = (
    "OfficialBlueprintAI",
    "OfficialLocaleTools",
    "OfficialRandomMap",
)

EXCLUDED_DIRECTORY_NAMES = frozenset(
    name.casefold()
    for name in (
        ".data",
        "bin",
        "obj",
        ".git",
        ".vs",
        ".idea",
        ".cache",
        "__pycache__",
    )
)

EXCLUDED_FILE_NAMES = frozenset({".ds_store"})
EXCLUDED_FILE_SUFFIXES = frozenset({".cache", ".pdb", ".pyc", ".pyo"})
VERSION_PATTERN = re.compile(r"[0-9]+(?:\.[0-9]+){1,3}")


def rejectDuplicateKeys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON property: {key}")
        result[key] = value
    return result


def readJsonObject(path: pathlib.Path) -> dict[str, object]:
    try:
        raw = path.read_bytes()
    except OSError as exception:
        raise RuntimeError(f"Failed to read JSON file '{path}': {exception}") from exception
    if raw.startswith(b"\xef\xbb\xbf"):
        raise RuntimeError(f"JSON file must not contain a UTF-8 BOM: {path}")
    try:
        text = raw.decode("utf-8")
        value = json.loads(text, object_pairs_hook=rejectDuplicateKeys)
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exception:
        raise RuntimeError(f"Invalid JSON file '{path}': {exception}") from exception
    if not isinstance(value, dict):
        raise RuntimeError(f"JSON root must be an object: {path}")
    return value


def requireString(manifest: dict[str, object], key: str, path: pathlib.Path) -> str:
    value = manifest.get(key)
    if not isinstance(value, str) or not value.strip():
        raise RuntimeError(f"Plugin manifest {key} is required: {path}")
    return value


def isValidPluginId(value: str) -> bool:
    if not value.strip() or value in (".", "..") or value.endswith("."):
        return False
    return all(character.isalnum() or character in ".-_" for character in value)


def isValidVersion(value: str) -> bool:
    if VERSION_PATTERN.fullmatch(value) is None:
        return False
    return all(int(component) <= 2147483647 for component in value.split("."))


def readManifest(pluginDirectory: pathlib.Path) -> dict[str, object]:
    manifestPath = pluginDirectory / "plugin.json"
    if not manifestPath.is_file() or manifestPath.is_symlink():
        raise RuntimeError(f"Plugin manifest was not found: {manifestPath}")
    manifest = readJsonObject(manifestPath)
    schemaVersion = manifest.get("schemaVersion")
    if type(schemaVersion) is not int or schemaVersion != 1:
        raise RuntimeError(f"Unsupported plugin manifest schema in {manifestPath}: {schemaVersion}")
    pluginId = requireString(manifest, "id", manifestPath)
    if not isValidPluginId(pluginId):
        raise RuntimeError(f"Invalid plugin ID in {manifestPath}: {pluginId}")
    requireString(manifest, "name", manifestPath)
    version = requireString(manifest, "version", manifestPath)
    if not isValidVersion(version):
        raise RuntimeError(f"Invalid plugin version in {manifestPath}: {version}")
    minimumEditorVersion = requireString(manifest, "minimumEditorVersion", manifestPath)
    if not isValidVersion(minimumEditorVersion):
        raise RuntimeError(
            f"Invalid minimum editor version in {manifestPath}: {minimumEditorVersion}"
        )
    requireString(manifest, "entryType", manifestPath)
    return manifest


def isLink(path: pathlib.Path) -> bool:
    try:
        information = path.lstat()
    except OSError as exception:
        raise RuntimeError(
            f"Failed to inspect filesystem entry '{path}': {exception}"
        ) from exception
    reparsePoint = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    fileAttributes = getattr(information, "st_file_attributes", 0)
    return stat.S_ISLNK(information.st_mode) or bool(fileAttributes & reparsePoint)


def requireSafeDirectory(path: pathlib.Path) -> None:
    if not path.exists():
        raise RuntimeError(f"Directory does not exist: {path}")
    if isLink(path):
        raise RuntimeError(f"Symbolic links are not allowed: {path}")
    if not path.is_dir():
        raise RuntimeError(f"Expected a directory: {path}")


def ensureTreeIsSafe(root: pathlib.Path) -> None:
    requireSafeDirectory(root)
    pending = [root]
    while pending:
        directory = pending.pop()
        try:
            entries = sorted(directory.iterdir(), key=lambda path: path.name)
        except OSError as exception:
            raise RuntimeError(
                f"Failed to enumerate directory '{directory}': {exception}"
            ) from exception
        for entry in entries:
            if isLink(entry):
                raise RuntimeError(f"Symbolic links are not allowed: {entry}")
            information = entry.stat(follow_symlinks=False)
            if stat.S_ISDIR(information.st_mode):
                pending.append(entry)
            elif not stat.S_ISREG(information.st_mode):
                raise RuntimeError(f"Unsupported filesystem entry: {entry}")


def shouldExcludeDirectory(path: pathlib.Path) -> bool:
    return path.name.casefold() in EXCLUDED_DIRECTORY_NAMES


def shouldExcludeFile(path: pathlib.Path) -> bool:
    return (
        path.name.casefold() in EXCLUDED_FILE_NAMES
        or path.suffix.casefold() in EXCLUDED_FILE_SUFFIXES
    )


def copyPlugin(source: pathlib.Path, destination: pathlib.Path) -> None:
    destination.mkdir()
    pending = [(source, destination)]
    while pending:
        sourceDirectory, destinationDirectory = pending.pop()
        entries = sorted(sourceDirectory.iterdir(), key=lambda path: path.name, reverse=True)
        for entry in entries:
            if isLink(entry):
                raise RuntimeError(f"Symbolic links are not allowed: {entry}")
            if entry.is_dir():
                if shouldExcludeDirectory(entry):
                    continue
                childDestination = destinationDirectory / entry.name
                childDestination.mkdir()
                pending.append((entry, childDestination))
            elif entry.is_file():
                if not shouldExcludeFile(entry):
                    shutil.copy2(entry, destinationDirectory / entry.name)
            else:
                raise RuntimeError(f"Unsupported filesystem entry: {entry}")


def expectedRegistry(manifests: list[dict[str, object]]) -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "plugins": [
            {
                "id": manifest["id"],
                "directory": directory,
            }
            for directory, manifest in zip(
                OFFICIAL_PLUGIN_DIRECTORIES,
                manifests,
                strict=True,
            )
        ],
        "pendingDelete": [],
    }


def serializeRegistry(registry: dict[str, object]) -> str:
    return json.dumps(registry, ensure_ascii=False, indent=2) + "\n"


def writeRegistry(path: pathlib.Path, manifests: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(serializeRegistry(expectedRegistry(manifests)))


def inspectManifests(pluginsDirectory: pathlib.Path) -> list[dict[str, object]]:
    manifests: list[dict[str, object]] = []
    ids: set[str] = set()
    for directory in OFFICIAL_PLUGIN_DIRECTORIES:
        pluginDirectory = pluginsDirectory / directory
        ensureTreeIsSafe(pluginDirectory)
        manifest = readManifest(pluginDirectory)
        pluginId = manifest["id"]
        if not isinstance(pluginId, str):
            raise RuntimeError(f"Invalid plugin ID in {pluginDirectory / 'plugin.json'}")
        if pluginId in ids:
            raise RuntimeError(f"Duplicate plugin ID: {pluginId}")
        ids.add(pluginId)
        manifests.append(manifest)
    return manifests


def validate(outputRoot: pathlib.Path) -> None:
    requireSafeDirectory(outputRoot)
    pluginsDirectory = outputRoot / "Plugins"
    ensureTreeIsSafe(pluginsDirectory)
    actualEntries = {path.name for path in pluginsDirectory.iterdir()}
    expectedEntries = set(OFFICIAL_PLUGIN_DIRECTORIES)
    if actualEntries != expectedEntries:
        missing = sorted(expectedEntries - actualEntries)
        unexpected = sorted(actualEntries - expectedEntries)
        details = []
        if missing:
            details.append(f"missing: {', '.join(missing)}")
        if unexpected:
            details.append(f"unexpected: {', '.join(unexpected)}")
        raise RuntimeError(
            f"Official plugin directory contents are invalid ({'; '.join(details)})"
        )
    manifests = inspectManifests(pluginsDirectory)
    for directory in OFFICIAL_PLUGIN_DIRECTORIES:
        pluginDirectory = pluginsDirectory / directory
        if not any(
            path.is_file() and path.suffix.casefold() == ".cs"
            for path in pluginDirectory.rglob("*")
        ):
            raise RuntimeError(f"Official plugin contains no C# source: {pluginDirectory}")
    for path in pluginsDirectory.rglob("*"):
        if path.is_dir() and shouldExcludeDirectory(path):
            raise RuntimeError(f"Excluded plugin directory was found: {path}")
        if path.is_file() and shouldExcludeFile(path):
            raise RuntimeError(f"Excluded plugin file was found: {path}")

    registryPath = outputRoot / "plugins.json"
    if not registryPath.is_file() or registryPath.is_symlink():
        raise RuntimeError(f"Plugin registry was not found: {registryPath}")
    actualRegistry = readJsonObject(registryPath)
    registry = expectedRegistry(manifests)
    if actualRegistry != registry:
        raise RuntimeError(
            f"Official plugin registry does not match the packaged manifests: {registryPath}"
        )
    expectedBytes = serializeRegistry(registry).encode("utf-8")
    if registryPath.read_bytes() != expectedBytes:
        raise RuntimeError(f"Official plugin registry is not canonically formatted: {registryPath}")


def pathsOverlap(first: pathlib.Path, second: pathlib.Path) -> bool:
    firstResolved = first.resolve()
    secondResolved = second.resolve()
    return (
        firstResolved == secondResolved
        or firstResolved.is_relative_to(secondResolved)
        or secondResolved.is_relative_to(firstResolved)
    )


def managedDestinationExists(path: pathlib.Path, expectedDirectory: bool) -> bool:
    if not path.exists() and not path.is_symlink():
        return False
    if isLink(path):
        raise RuntimeError(f"Refusing to replace a symbolic link: {path}")
    if expectedDirectory:
        if not path.is_dir():
            raise RuntimeError(f"Expected a directory at managed destination: {path}")
    elif not path.is_file():
        raise RuntimeError(f"Expected a regular file at managed destination: {path}")
    return True


def removeManagedDestination(path: pathlib.Path, expectedDirectory: bool) -> None:
    if not managedDestinationExists(path, expectedDirectory):
        return
    if expectedDirectory:
        shutil.rmtree(path)
    else:
        path.unlink()


def prepare(source: pathlib.Path, outputRoot: pathlib.Path) -> None:
    requireSafeDirectory(source)
    destinationPlugins = outputRoot / "Plugins"
    if pathsOverlap(source, destinationPlugins):
        raise RuntimeError(
            f"Plugin source and destination must not overlap: {source}, {destinationPlugins}"
        )
    manifests = inspectManifests(source)

    if outputRoot.exists():
        requireSafeDirectory(outputRoot)
    else:
        outputRoot.mkdir(parents=True)
    temporaryRoot = pathlib.Path(
        tempfile.mkdtemp(prefix=".official-plugins-", dir=outputRoot)
    )
    preserveTemporary = False
    try:
        temporaryPlugins = temporaryRoot / "Plugins"
        temporaryPlugins.mkdir()
        for directory in OFFICIAL_PLUGIN_DIRECTORIES:
            copyPlugin(source / directory, temporaryPlugins / directory)
        writeRegistry(temporaryRoot / "plugins.json", manifests)
        validate(temporaryRoot)

        destinationRegistry = outputRoot / "plugins.json"
        pluginsExisted = managedDestinationExists(destinationPlugins, True)
        registryExisted = managedDestinationExists(destinationRegistry, False)
        previousPlugins = temporaryRoot / "previous-Plugins"
        previousRegistry = temporaryRoot / "previous-plugins.json"
        pluginsBackedUp = False
        registryBackedUp = False
        pluginsInstalled = False
        registryInstalled = False
        try:
            if pluginsExisted:
                destinationPlugins.replace(previousPlugins)
                pluginsBackedUp = True
            if registryExisted:
                destinationRegistry.replace(previousRegistry)
                registryBackedUp = True
            temporaryPlugins.replace(destinationPlugins)
            pluginsInstalled = True
            (temporaryRoot / "plugins.json").replace(destinationRegistry)
            registryInstalled = True
            validate(outputRoot)
        except BaseException as exception:
            rollbackErrors: list[str] = []
            for path, expectedDirectory, installed in (
                (destinationRegistry, False, registryInstalled),
                (destinationPlugins, True, pluginsInstalled),
            ):
                if not installed:
                    continue
                try:
                    removeManagedDestination(path, expectedDirectory)
                except Exception as rollbackException:
                    rollbackErrors.append(f"failed to remove '{path}': {rollbackException}")
            for backup, destination, backedUp in (
                (previousRegistry, destinationRegistry, registryBackedUp),
                (previousPlugins, destinationPlugins, pluginsBackedUp),
            ):
                if not backedUp:
                    continue
                try:
                    backup.replace(destination)
                except Exception as rollbackException:
                    rollbackErrors.append(
                        f"failed to restore '{destination}': {rollbackException}"
                    )
            if rollbackErrors:
                preserveTemporary = True
                details = "; ".join(rollbackErrors)
                raise RuntimeError(
                    f"Failed to roll back official plug-ins; backups remain in "
                    f"'{temporaryRoot}': {details}"
                ) from exception
            raise
    finally:
        if not preserveTemporary:
            shutil.rmtree(temporaryRoot)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools editor-official-plugins")
    subparsers = parser.add_subparsers(dest="command", required=True)
    prepareParser = subparsers.add_parser("prepare")
    prepareParser.add_argument("source", type=pathlib.Path)
    prepareParser.add_argument("output_root", type=pathlib.Path)
    validateParser = subparsers.add_parser("validate")
    validateParser.add_argument("output_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    if parsed.command == "prepare":
        prepare(parsed.source.absolute(), parsed.output_root.absolute())
    else:
        validate(parsed.output_root.absolute())
    return 0
