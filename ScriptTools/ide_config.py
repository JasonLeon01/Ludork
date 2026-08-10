from __future__ import annotations

import argparse
import json
import os
import pathlib
import tempfile
import xml.etree.ElementTree as elementTree


MANAGED_PRESET_PREFIX = "ludork-clion-"
PROFILE_NAME = "ludork-clion-debug"
PROFILE_DISPLAY_NAME = "Ludork Debug"
CLION_PROFILE_NAME = PROFILE_DISPLAY_NAME
RUN_CONFIGURATION_NAME = "Ludork Play"


def requireProject(projectPath: pathlib.Path) -> None:
    if not projectPath.is_dir():
        raise RuntimeError(f"Project directory was not found: {projectPath}")
    for fileName in ("CMakeLists.txt", "Main.proj"):
        path = projectPath / fileName
        if not path.is_file():
            raise RuntimeError(f"{fileName} was not found: {projectPath}")


def requireFile(path: pathlib.Path, description: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"{description} was not found: {path}")


def cmakePath(path: pathlib.Path) -> str:
    return path.as_posix()


def loadJsonObject(path: pathlib.Path) -> dict[str, object]:
    if not path.is_file():
        return {}
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict):
        raise RuntimeError(f"JSON root must be an object: {path}")
    return data


def replaceManagedPresets(
    data: dict[str, object],
    key: str,
    managedPreset: dict[str, object] | None,
) -> None:
    value = data.get(key, [])
    if not isinstance(value, list):
        raise RuntimeError(f"{key} must be an array in CMakeUserPresets.json")
    preserved: list[object] = []
    for item in value:
        if (
            isinstance(item, dict)
            and isinstance(item.get("name"), str)
            and item["name"].startswith(MANAGED_PRESET_PREFIX)
        ):
            continue
        preserved.append(item)
    if managedPreset is not None:
        preserved.append(managedPreset)
    if preserved:
        data[key] = preserved
    else:
        data.pop(key, None)


def createPresetData(
    existing: dict[str, object],
    platform: str,
    scriptToolsPath: pathlib.Path,
    gnuMakePath: pathlib.Path | None,
) -> dict[str, object]:
    data = dict(existing)
    version = data.get("version", 3)
    if not isinstance(version, int):
        raise RuntimeError("version must be an integer in CMakeUserPresets.json")
    data["version"] = max(version, 3)
    data.setdefault(
        "cmakeMinimumRequired",
        {
            "major": 3,
            "minor": 21,
            "patch": 0,
        },
    )

    cacheVariables: dict[str, object] = {
        "LUDORK_SCRIPT_TOOLS_EXECUTABLE": {
            "type": "FILEPATH",
            "value": cmakePath(scriptToolsPath),
        },
    }
    configurePreset: dict[str, object] = {
        "name": PROFILE_NAME,
        "displayName": PROFILE_DISPLAY_NAME,
        "binaryDir": "${sourceDir}/cmake-build-ludork-debug",
        "cacheVariables": cacheVariables,
    }
    if platform == "windows":
        configurePreset["generator"] = "Visual Studio 17 2022"
        configurePreset["architecture"] = "x64"
        configurePreset["vendor"] = {
            "jetbrains.com/clion": {
                "toolchain": "Visual Studio",
            },
        }
        if gnuMakePath is not None:
            cacheVariables["LUDORK_GNU_MAKE_EXECUTABLE"] = {
                "type": "FILEPATH",
                "value": cmakePath(gnuMakePath),
            }
    else:
        configurePreset["generator"] = "Ninja"
        cacheVariables.update(
            {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_OSX_ARCHITECTURES": "arm64",
                "CMAKE_OSX_DEPLOYMENT_TARGET": "13.3",
            }
        )

    replaceManagedPresets(data, "configurePresets", configurePreset)
    replaceManagedPresets(data, "buildPresets", None)
    return data


def loadXmlProject(path: pathlib.Path) -> elementTree.Element:
    if not path.is_file():
        return elementTree.Element("project", {"version": "4"})
    root = elementTree.parse(path).getroot()
    if root.tag != "project":
        raise RuntimeError(f"XML root must be project: {path}")
    return root


def createWorkspaceRoot(
    existingRoot: elementTree.Element,
    platform: str,
) -> elementTree.Element:
    root = elementTree.fromstring(elementTree.tostring(existingRoot))
    component = next(
        (
            item
            for item in root.findall("component")
            if item.get("name") == "CMakeSettings"
        ),
        None,
    )
    if component is None:
        component = elementTree.SubElement(root, "component", {"name": "CMakeSettings"})
    configurations = component.find("configurations")
    if configurations is None:
        configurations = elementTree.SubElement(component, "configurations")
    for configuration in list(configurations.findall("configuration")):
        profileName = configuration.get("PROFILE_NAME", "")
        if (
            profileName.startswith(MANAGED_PRESET_PREFIX)
            or profileName == CLION_PROFILE_NAME
        ):
            configurations.remove(configuration)
    attributes = {
        "PROFILE_NAME": CLION_PROFILE_NAME,
        "ENABLED": "true",
        "CONFIG_NAME": "Debug",
        "GENERATION_DIR": "$PROJECT_DIR$/cmake-build-ludork-debug",
    }
    if platform == "windows":
        attributes["TOOLCHAIN_NAME"] = "Visual Studio"
        attributes["GENERATION_OPTIONS"] = (
            f'--preset {PROFILE_NAME} -G "Visual Studio 17 2022" -A x64'
        )
    else:
        attributes["GENERATION_OPTIONS"] = f'--preset {PROFILE_NAME} -G Ninja'
    elementTree.SubElement(configurations, "configuration", attributes)
    return root


def createMiscRoot(existingRoot: elementTree.Element) -> elementTree.Element:
    root = elementTree.fromstring(elementTree.tostring(existingRoot))
    component = next(
        (
            item
            for item in root.findall("component")
            if item.get("name") == "CMakeWorkspace"
        ),
        None,
    )
    if component is None:
        component = elementTree.SubElement(
            root,
            "component",
            {"name": "CMakeWorkspace"},
        )
    component.set("PROJECT_DIR", "$PROJECT_DIR$")
    return root


def createRunConfiguration(
    existingRoot: elementTree.Element | None,
) -> elementTree.Element:
    if existingRoot is None:
        root = elementTree.Element(
            "component",
            {"name": "ProjectRunConfigurationManager"},
        )
    else:
        root = elementTree.fromstring(elementTree.tostring(existingRoot))
        if (
            root.tag != "component"
            or root.get("name") != "ProjectRunConfigurationManager"
        ):
            raise RuntimeError("CLion run configuration root is invalid")
    for configuration in list(root.findall("configuration")):
        if configuration.get("name") == RUN_CONFIGURATION_NAME:
            root.remove(configuration)
    configuration = elementTree.SubElement(
        root,
        "configuration",
        {
            "default": "false",
            "name": RUN_CONFIGURATION_NAME,
            "type": "CMakeRunConfiguration",
            "factoryName": "Application",
            "REDIRECT_INPUT": "false",
            "ELEVATE": "false",
            "USE_EXTERNAL_CONSOLE": "false",
            "EMULATE_TERMINAL": "false",
            "PASS_PARENT_ENVS_2": "true",
            "PROJECT_NAME": "Main",
            "TARGET_NAME": "Main",
            "CONFIG_NAME": CLION_PROFILE_NAME,
            "RUN_TARGET_PROJECT_NAME": "Main",
            "RUN_TARGET_NAME": "Main",
            "WORKING_DIR": "$PROJECT_DIR$",
        },
    )
    environments = elementTree.SubElement(configuration, "envs")
    elementTree.SubElement(
        environments,
        "env",
        {"name": "LUDORK_EDITOR", "value": "1"},
    )
    elementTree.SubElement(
        environments,
        "env",
        {"name": "LUDORK_WINDOW_MODE", "value": "individual"},
    )
    method = elementTree.SubElement(configuration, "method", {"v": "2"})
    elementTree.SubElement(
        method,
        "option",
        {
            "name": "com.jetbrains.cidr.execution.CidrBuildBeforeRunTaskProvider$BuildBeforeRunTask",
            "enabled": "true",
        },
    )
    return root


def loadRunConfiguration(path: pathlib.Path) -> elementTree.Element | None:
    if not path.is_file():
        return None
    return elementTree.parse(path).getroot()


def xmlText(root: elementTree.Element) -> str:
    elementTree.indent(root, space="  ")
    content = elementTree.tostring(root, encoding="unicode", short_empty_elements=True)
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + content + "\n"


def writeTextAtomic(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporaryName = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
    )
    temporaryPath = pathlib.Path(temporaryName)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        os.replace(temporaryPath, path)
    finally:
        temporaryPath.unlink(missing_ok=True)


def generateClionConfiguration(
    projectPath: pathlib.Path,
    platform: str,
    scriptToolsPath: pathlib.Path,
    gnuMakePath: pathlib.Path | None = None,
) -> None:
    requireProject(projectPath)
    requireFile(scriptToolsPath, "ScriptTools")
    if platform == "windows":
        if gnuMakePath is None:
            raise RuntimeError("GNU Make is required for Windows CLion configuration")
        requireFile(gnuMakePath, "GNU Make")

    presetsPath = projectPath / "CMakeUserPresets.json"
    miscPath = projectPath / ".idea" / "misc.xml"
    workspacePath = projectPath / ".idea" / "workspace.xml"
    runConfigurationPath = (
        projectPath
        / ".idea"
        / "runConfigurations"
        / "Ludork_Play.xml"
    )
    presetData = createPresetData(
        loadJsonObject(presetsPath),
        platform,
        scriptToolsPath,
        gnuMakePath,
    )
    miscRoot = createMiscRoot(loadXmlProject(miscPath))
    workspaceRoot = createWorkspaceRoot(loadXmlProject(workspacePath), platform)
    runConfigurationRoot = createRunConfiguration(
        loadRunConfiguration(runConfigurationPath)
    )

    writeTextAtomic(
        presetsPath,
        json.dumps(presetData, ensure_ascii=False, indent=2) + "\n",
    )
    writeTextAtomic(miscPath, xmlText(miscRoot))
    writeTextAtomic(workspacePath, xmlText(workspaceRoot))
    writeTextAtomic(runConfigurationPath, xmlText(runConfigurationRoot))


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools ide-config")
    subparsers = parser.add_subparsers(dest="ide", required=True)
    clionParser = subparsers.add_parser("clion")
    clionParser.add_argument("project", type=pathlib.Path)
    clionParser.add_argument(
        "--platform",
        required=True,
        choices=("windows", "macos"),
    )
    clionParser.add_argument("--script-tools", required=True, type=pathlib.Path)
    clionParser.add_argument("--gnu-make", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    generateClionConfiguration(
        parsed.project.resolve(),
        parsed.platform,
        parsed.script_tools.resolve(),
        parsed.gnu_make.resolve() if parsed.gnu_make is not None else None,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
