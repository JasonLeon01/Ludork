from __future__ import annotations

import argparse
import pathlib
import plistlib

from .packaging_metadata import ReleaseVersion, load_release_version


def expectedMetadata(release: ReleaseVersion) -> dict[str, object]:
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
        "CFBundleShortVersionString": release.version,
        "CFBundleVersion": release.apple_build_version,
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


def generate(buildInfoPath: pathlib.Path, outputPath: pathlib.Path) -> None:
    metadata = expectedMetadata(load_release_version(buildInfoPath))
    outputPath.parent.mkdir(parents=True, exist_ok=True)
    with outputPath.open("wb") as stream:
        plistlib.dump(metadata, stream, sort_keys=True)


def validate(buildInfoPath: pathlib.Path, plistPath: pathlib.Path) -> None:
    with plistPath.open("rb") as stream:
        metadata = plistlib.load(stream)
    expected = expectedMetadata(load_release_version(buildInfoPath))
    if metadata != expected:
        raise RuntimeError(f"Editor bundle metadata is invalid: {plistPath}")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools editor-macos-metadata")
    subparsers = parser.add_subparsers(dest="command", required=True)
    generateParser = subparsers.add_parser("generate")
    generateParser.add_argument("output_file", type=pathlib.Path)
    generateParser.add_argument("--build-info", type=pathlib.Path, required=True)
    validateParser = subparsers.add_parser("validate")
    validateParser.add_argument("plist_file", type=pathlib.Path)
    validateParser.add_argument("--build-info", type=pathlib.Path, required=True)
    parsed = parser.parse_args(arguments)
    if parsed.command == "generate":
        generate(parsed.build_info.resolve(), parsed.output_file.resolve())
    else:
        validate(parsed.build_info.resolve(), parsed.plist_file.resolve())
    return 0
