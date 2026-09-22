from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import re
from dataclasses import dataclass

from .pack_error import PackError
from .packaging_constants import EXIT_PROJECT
from .packaging_names import artifact_name, read_app_name


BUILD_TIMEZONE = datetime.timezone(datetime.timedelta(hours=8))
_VERSION_PATTERN = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)")


def add_channel_arguments(parser: argparse.ArgumentParser) -> None:
    channel = parser.add_mutually_exclusive_group()
    channel.add_argument("--dev", dest="dev", action="store_const", const=True, default=None,
                         help="Build an internal test package with an hourly UTC+8 suffix")
    channel.add_argument("--release", dest="dev", action="store_const", const=False,
                         help="Build a release package without the date suffix")


def add_package_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--version", help="Override packaging.version from Main.proj")
    add_channel_arguments(parser)


@dataclass(frozen=True)
class ReleaseVersion:
    version: str
    dev: bool
    built_at: datetime.datetime

    def __post_init__(self) -> None:
        if not isinstance(self.version, str) or _VERSION_PATTERN.fullmatch(self.version) is None:
            raise PackError("Package version must contain three decimal integers without leading zeros, such as 1.0.0.", EXIT_PROJECT)
        if len(self.version.encode("utf-8")) > 127:
            raise PackError("Package version must not exceed 127 bytes.", EXIT_PROJECT)
        if not isinstance(self.dev, bool):
            raise PackError("packaging.dev must be a boolean.", EXIT_PROJECT)
        if self.built_at.tzinfo is None or self.built_at.utcoffset() is None:
            raise PackError("Package build time must include its timezone.", EXIT_PROJECT)
        object.__setattr__(self, "built_at", self.built_at.astimezone(BUILD_TIMEZONE).replace(microsecond=0))
        if not 2000 <= self.built_at.year <= 2099:
            raise PackError("Package build time must be in 2000–2099.", EXIT_PROJECT)
        if not 0 < self.version_code <= 2_100_000_000:
            raise PackError("Package build number exceeds the platform limit.", EXIT_PROJECT)

    @property
    def full_version(self) -> str:
        return f"{self.version}.{self.built_at:%Y%m%d%H}" if self.dev else self.version

    @property
    def version_code(self) -> int:
        return int(self.built_at.strftime("%Y%m%d%H"))

    @property
    def apple_build_version(self) -> str:
        return f"{int(self.built_at.strftime('%y%m'))}.{self.built_at.day}.{self.built_at.hour}"

    def build_info(self) -> dict[str, object]:
        return {
            "version": self.version,
            "fullVersion": self.full_version,
            "dev": self.dev,
            "builtAt": self.built_at.isoformat(),
        }

    def as_dict(self) -> dict[str, object]:
        return self.build_info() | {
            "versionCode": self.version_code,
            "appleBuildVersion": self.apple_build_version,
        }

    def write_build_info(self, directory: pathlib.Path) -> None:
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "BuildInfo.json").write_text(
            json.dumps(self.build_info(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )


@dataclass(frozen=True)
class PackageMetadata(ReleaseVersion):
    app_name: str
    display_name: str

    def __post_init__(self) -> None:
        super().__post_init__()
        for label, value in (("APP_NAME", self.app_name), ("System title", self.display_name)):
            if not isinstance(value, str) or not value.strip() or any(ord(char) < 32 or ord(char) == 127 for char in value):
                raise PackError(f"{label} must be a non-empty string without control characters.", EXIT_PROJECT)

    @property
    def package_name(self) -> str:
        return f"{artifact_name(self.app_name)}-{self.full_version}"

    def as_dict(self) -> dict[str, object]:
        return super().as_dict() | {
            "appName": self.app_name,
            "displayName": self.display_name,
            "packageName": self.package_name,
        }

    def write_build_info(self, runtime_root: pathlib.Path) -> None:
        data = runtime_root / "Data"
        super().write_build_info(data)
        (data / "BuildInfo.ldc").unlink(missing_ok=True)


def read_release_version(version: str, dev: bool = False,
                         built_at: datetime.datetime | None = None) -> ReleaseVersion:
    return ReleaseVersion(version, dev, built_at if built_at is not None else datetime.datetime.now(BUILD_TIMEZONE))


def _read_object(path: pathlib.Path) -> dict[str, object]:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, ValueError) as error:
        raise PackError(f"Unable to read package metadata from {path}: {error}", EXIT_PROJECT) from error
    if not isinstance(data, dict):
        raise PackError(f"Package metadata must be a JSON object: {path}", EXIT_PROJECT)
    return data


def read_package_metadata(project: pathlib.Path, version: str | None = None,
                          dev: bool | None = None,
                          built_at: datetime.datetime | None = None) -> PackageMetadata:
    started_at = built_at if built_at is not None else datetime.datetime.now(BUILD_TIMEZONE)
    project_data = _read_object(project / "Main.proj")
    packaging = project_data.get("packaging", {})
    if not isinstance(packaging, dict):
        raise PackError("Main.proj packaging must be an object.", EXIT_PROJECT)
    release = read_release_version(
        packaging.get("version", "1.0.0") if version is None else version,
        packaging.get("dev", False) if dev is None else dev,
        started_at,
    )
    app_name = read_app_name(project)
    system = _read_object(project / "Data" / "Configs" / "System.json")
    title = system.get("title")
    display_name = title.get("value") if isinstance(title, dict) else None
    return PackageMetadata(release.version, release.dev, release.built_at, app_name, display_name)


def load_release_version(path: pathlib.Path) -> ReleaseVersion:
    data = _read_object(path)
    try:
        release = ReleaseVersion(
            data["version"], data["dev"], datetime.datetime.fromisoformat(data["builtAt"]),
        )
    except (KeyError, TypeError, ValueError) as error:
        raise PackError(f"Invalid build information {path}: {error}", EXIT_PROJECT) from error
    if data != release.build_info():
        raise PackError(f"Build information fields do not match: {path}", EXIT_PROJECT)
    return release


def load_package_metadata(path: pathlib.Path) -> PackageMetadata:
    data = _read_object(path)
    try:
        metadata = PackageMetadata(
            data["version"], data["dev"], datetime.datetime.fromisoformat(data["builtAt"]),
            data["appName"], data["displayName"],
        )
    except (KeyError, TypeError, ValueError) as error:
        raise PackError(f"Invalid package metadata snapshot {path}: {error}", EXIT_PROJECT) from error
    if data != metadata.as_dict():
        raise PackError(f"Package metadata snapshot fields do not match: {path}", EXIT_PROJECT)
    return metadata
