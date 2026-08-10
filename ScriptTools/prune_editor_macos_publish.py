from __future__ import annotations

import argparse
import json
import pathlib


FOREIGN_ASSEMBLIES = {
    "Avalonia.FreeDesktop.AtSpi.dll",
    "Avalonia.FreeDesktop.dll",
    "Avalonia.Win32.Automation.dll",
    "Avalonia.Win32.dll",
    "Avalonia.X11.dll",
    "NAudio.Asio.dll",
    "NAudio.Midi.dll",
    "NAudio.Wasapi.dll",
    "NAudio.WinMM.dll",
    "NAudio.dll",
    "Tmds.DBus.Protocol.dll",
}

FOREIGN_PACKAGES = {
    "Avalonia.FreeDesktop",
    "Avalonia.FreeDesktop.AtSpi",
    "Avalonia.Win32",
    "Avalonia.X11",
    "NAudio",
    "NAudio.Asio",
    "NAudio.Midi",
    "NAudio.Wasapi",
    "NAudio.WinMM",
    "Tmds.DBus.Protocol",
}


def packageName(key: str) -> str:
    return key.split("/", 1)[0]


def pruneDependencies(node: object, foreignPackages: set[str]) -> None:
    if not isinstance(node, dict):
        return
    dependencies = node.get("dependencies")
    if not isinstance(dependencies, dict):
        return
    for dependency in list(dependencies):
        if dependency in foreignPackages:
            del dependencies[dependency]


def pruneAssets(node: object, foreignAssemblies: set[str]) -> None:
    if not isinstance(node, dict):
        return
    for groupName in ("runtime", "native", "resources", "runtimeTargets"):
        assets = node.get(groupName)
        if not isinstance(assets, dict):
            continue
        for asset in list(assets):
            if pathlib.PurePosixPath(asset).name in foreignAssemblies:
                del assets[asset]


def validatePrunedData(data: object, foreignPackages: set[str], foreignAssemblies: set[str]) -> None:
    if not isinstance(data, dict):
        raise RuntimeError("The dependency manifest root is not an object")
    targets = data.get("targets")
    if isinstance(targets, dict):
        for target in targets.values():
            if not isinstance(target, dict):
                continue
            for key, node in target.items():
                if packageName(key) in foreignPackages:
                    raise RuntimeError(f"Foreign package remains in dependency targets: {key}")
                if isinstance(node, dict):
                    dependencies = node.get("dependencies")
                    if isinstance(dependencies, dict):
                        for dependency in dependencies:
                            if dependency in foreignPackages:
                                raise RuntimeError(f"Foreign package dependency remains: {dependency}")
                    for groupName in ("runtime", "native", "resources", "runtimeTargets"):
                        assets = node.get(groupName)
                        if not isinstance(assets, dict):
                            continue
                        for asset in assets:
                            if pathlib.PurePosixPath(asset).name in foreignAssemblies:
                                raise RuntimeError(f"Foreign assembly remains in dependency targets: {asset}")
    libraries = data.get("libraries")
    if isinstance(libraries, dict):
        for key in libraries:
            if packageName(key) in foreignPackages:
                raise RuntimeError(f"Foreign package remains in dependency libraries: {key}")


def pruneDepsFile(path: pathlib.Path, foreignPackages: set[str], foreignAssemblies: set[str]) -> None:
    with path.open(encoding="utf-8") as stream:
        data = json.load(stream)
    targets = data.get("targets")
    if isinstance(targets, dict):
        for target in targets.values():
            if not isinstance(target, dict):
                continue
            for key in list(target):
                if packageName(key) in foreignPackages:
                    del target[key]
                    continue
                pruneDependencies(target[key], foreignPackages)
                pruneAssets(target[key], foreignAssemblies)
    libraries = data.get("libraries")
    if isinstance(libraries, dict):
        for key in list(libraries):
            if packageName(key) in foreignPackages:
                del libraries[key]
    validatePrunedData(data, foreignPackages, foreignAssemblies)
    temporaryPath = path.with_name(f"{path.name}.tmp")
    temporaryPath.unlink(missing_ok=True)
    with temporaryPath.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    temporaryPath.replace(path)


def prunePublish(publishDirectory: pathlib.Path, foreignPackages: set[str], foreignAssemblies: set[str]) -> None:
    depsPath = publishDirectory / "Ludork.deps.json"
    if not depsPath.is_file():
        raise RuntimeError(f"Ludork.deps.json was not found: {depsPath}")
    pruneDepsFile(depsPath, foreignPackages, foreignAssemblies)
    for assembly in foreignAssemblies:
        path = publishDirectory / assembly
        if path.exists():
            path.unlink()


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools prune-editor-macos-publish")
    parser.add_argument("publish_folder", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    publishDirectory = parsed.publish_folder.resolve()
    prunePublish(publishDirectory, FOREIGN_PACKAGES, FOREIGN_ASSEMBLIES)
    return 0
