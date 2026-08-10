from __future__ import annotations

import argparse
import pathlib
import plistlib
import re
import xml.etree.ElementTree as elementTree


def readProjectVersion(projectPath: pathlib.Path) -> str:
    root = elementTree.parse(projectPath).getroot()
    versionNode = root.find(".//Version")
    if versionNode is None or versionNode.text is None:
        raise RuntimeError(f"Version was not found in {projectPath}")
    version = versionNode.text.strip()
    if re.fullmatch(r"[0-9]+(?:\.[0-9]+){0,2}", version) is None:
        raise RuntimeError(f"Unsupported macOS bundle version: {version}")
    return version


def expectedMetadata(version: str) -> dict[str, object]:
    return {
        "CFBundleDocumentTypes": [
            {
                "CFBundleTypeExtensions": ["proj"],
                "CFBundleTypeIconFile": "ProjectIcon.icns",
                "CFBundleTypeName": "Ludork Project",
                "CFBundleTypeRole": "Editor",
                "LSHandlerRank": "Owner",
                "LSItemContentTypes": ["com.ludork.project"],
            },
        ],
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": "Ludork",
        "CFBundleExecutable": "Ludork",
        "CFBundleIdentifier": "com.ludork.editor",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleIconFile": "AppIcon",
        "CFBundleName": "Ludork",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": version,
        "CFBundleVersion": version,
        "LSApplicationCategoryType": "public.app-category.developer-tools",
        "LSMinimumSystemVersion": "13.3",
        "NSHighResolutionCapable": True,
        "UTExportedTypeDeclarations": [
            {
                "UTTypeConformsTo": ["public.json"],
                "UTTypeDescription": "Ludork Project",
                "UTTypeIdentifier": "com.ludork.project",
                "UTTypeTagSpecification": {
                    "public.filename-extension": ["proj"],
                    "public.mime-type": "application/x-ludork-project",
                },
            },
        ],
    }


def generate(projectPath: pathlib.Path, outputPath: pathlib.Path) -> None:
    metadata = expectedMetadata(readProjectVersion(projectPath))
    outputPath.parent.mkdir(parents=True, exist_ok=True)
    with outputPath.open("wb") as stream:
        plistlib.dump(metadata, stream, sort_keys=True)


def validate(projectPath: pathlib.Path, plistPath: pathlib.Path) -> None:
    with plistPath.open("rb") as stream:
        metadata = plistlib.load(stream)
    expected = expectedMetadata(readProjectVersion(projectPath))
    if metadata != expected:
        raise RuntimeError(f"Editor bundle metadata is invalid: {plistPath}")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools editor-macos-metadata")
    subparsers = parser.add_subparsers(dest="command", required=True)
    generateParser = subparsers.add_parser("generate")
    generateParser.add_argument("project_file", type=pathlib.Path)
    generateParser.add_argument("output_file", type=pathlib.Path)
    validateParser = subparsers.add_parser("validate")
    validateParser.add_argument("project_file", type=pathlib.Path)
    validateParser.add_argument("plist_file", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    if parsed.command == "generate":
        generate(parsed.project_file.resolve(), parsed.output_file.resolve())
    else:
        validate(parsed.project_file.resolve(), parsed.plist_file.resolve())
    return 0
