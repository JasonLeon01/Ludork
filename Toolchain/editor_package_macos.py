from __future__ import annotations

import argparse
import os
import pathlib
import plistlib
import shutil
import subprocess
import sys
import time
from collections.abc import Callable

from . import editor_macos_metadata, editor_official_plugins
from .editor_package_contents import require_directory, require_file
from .pack_error import PackError
from .package_process import run
from .packaging_constants import STANDALONE_TEMPLATE_NAMES
from .tool_paths import resolve_tool


SIGNING_ENVIRONMENT = {
    "signing_identity": "LUDORK_MACOS_SIGNING_IDENTITY",
    "certificate": "LUDORK_MACOS_SIGNING_CERTIFICATE",
    "entitlements": "LUDORK_MACOS_SIGNING_ENTITLEMENTS",
    "notary_apple_id": "LUDORK_MACOS_NOTARY_APPLE_ID",
    "notary_team_id": "LUDORK_MACOS_NOTARY_TEAM_ID",
    "notary_key": "LUDORK_MACOS_NOTARY_KEY",
    "notary_key_id": "LUDORK_MACOS_NOTARY_KEY_ID",
    "notary_key_issuer": "LUDORK_MACOS_NOTARY_KEY_ISSUER",
}


def _text(command: list[str | pathlib.Path]) -> str:
    return run(command, capture=True).stdout.decode("utf-8", errors="replace").strip()


def _require_format(path: pathlib.Path, expected: str) -> None:
    if expected not in _text(["file", "-b", path]):
        raise PackError(f"Invalid {expected} file: {path}")


def validate_native_package(package: pathlib.Path, build_info: pathlib.Path) -> None:
    binaries = package / "Contents/MacOS"
    resources = package / "Contents/Resources"
    run(["codesign", "--verify", "--deep", "--strict", package])
    run(["plutil", "-lint", package / "Contents/Info.plist"], quiet=True)
    editor_macos_metadata.validate(build_info, package / "Contents/Info.plist")
    if _text(["/usr/bin/lipo", "-archs", binaries / "Ludork"]) != "arm64":
        raise PackError("The editor executable is not Apple Silicon-only.")
    for name in ("AppIcon.icns", "ProjectIcon.icns"):
        _require_format(resources / name, "Mac OS X icon")
    for path in (resources / "BuildTools/Toolchain").rglob("*"):
        if path.is_file() and not path.is_symlink() and "Mach-O" in _text(["file", "-b", path]):
            run(["codesign", "--verify", "--strict", path])


class EditorDiskImage:
    def __init__(self, repository: pathlib.Path, work: pathlib.Path, stage: pathlib.Path,
                 toolchain: pathlib.Path, arguments: argparse.Namespace) -> None:
        self.repository = repository
        self.work = work
        self.stage = stage
        self.toolchain = toolchain
        self.arguments = arguments
        self.root = work / "dmg-root"
        self.app = self.root / "Ludork.app"
        self.binaries = self.app / "Contents/MacOS"
        self.resources = self.app / "Contents/Resources"
        self.payload = self.root / ".official-plugin-payload"
        self.background = self.root / ".background"
        self.installer = self.root / "Install Official Plugins.command"
        self.assets = repository / "tools/editor_dmg"
        self.mount = pathlib.Path(f"/tmp/ludork-editor-dmg-mount.{os.getpid()}")
        self.layout_image = pathlib.Path(f"/tmp/ludork-editor-layout-rw.{os.getpid()}.dmg")
        self.publish_image = pathlib.Path(f"/tmp/ludork-editor-publish-rw.{os.getpid()}.dmg")
        self.mounted = False
        self.image: pathlib.Path | None = None
        self.mount_owned = False
        self.owned_images: set[pathlib.Path] = set()
        self.signing = {
            name: os.environ.get(environment) or str(getattr(arguments, name, None) or "")
            for name, environment in SIGNING_ENVIRONMENT.items()
        }
        self.certificate_password = ""
        self.notary_password = ""

    def preflight(self) -> None:
        for name in ("rsync", "sips", "iconutil", "plutil", "file", "hdiutil", "diskutil", "codesign",
                     "DeRez", "GetFileInfo", "Rez", "SetFile", "osascript", "strings", "sync", "xattr", "tar"):
            if shutil.which(name) is None:
                raise PackError(f"Required command was not found: {name}")
        require_file(pathlib.Path("/usr/bin/lipo"))
        require_file(pathlib.Path("/sbin/umount"), executable=True)
        for name in ("background.svg", "installer_icon.svg", "configure_layout.applescript",
                     "install_official_plugins.command"):
            require_file(self.assets / name, executable=name.endswith(".command"))
        run(["sh", "-n", self.assets / "install_official_plugins.command"])
        for path in (self.mount, self.layout_image, self.publish_image):
            if path.exists() or path.is_symlink():
                raise PackError(f"An editor DMG temporary path already exists: {path}")
        current_mounts = _text(["hdiutil", "info"])
        if str(self.work) in current_mounts or "/tmp/ludork-editor-dmg-mount." in current_mounts:
            raise PackError("An earlier editor DMG is still mounted; detach it before packaging again.")
        if self.signing["certificate"]:
            self.certificate_password = self._read_secret()
        if self.arguments.notarize and self.signing["notary_apple_id"]:
            self.notary_password = self._read_secret()

    @staticmethod
    def _read_secret() -> str:
        value = sys.stdin.readline().rstrip("\r\n")
        if not value:
            raise PackError("A non-empty signing password is required on standard input.")
        return value

    def sign(self, mode: str, target: pathlib.Path, *, templates: bool = False) -> None:
        command: list[str | pathlib.Path] = [resolve_tool(self.toolchain, "pack"), "macos-sign", "--ignore-environment"]
        options = {name: value for name, value in self.signing.items() if name != "entitlements"}
        for name, value in options.items():
            if value:
                command.extend(("--" + name.replace("_", "-"), value))
        if mode == "app":
            entitlements = self.signing["entitlements"]
            if not entitlements and (self.signing["signing_identity"] or self.signing["certificate"]):
                entitlements = str(self.assets / "Editor.entitlements")
            if entitlements:
                command.extend(("--entitlements", entitlements))
            command.extend(("--runtime-bundle", self.resources / "BuildTools/Toolchain"))
            if templates:
                for name in STANDALONE_TEMPLATE_NAMES:
                    command.extend(("--preview-template", self.resources / "Templates" / name))
        if mode == "dmg-notarize":
            command.append("--notarize")
        secrets = []
        if self.signing["certificate"]:
            secrets.append(self.certificate_password)
        if mode == "dmg-notarize" and self.signing["notary_apple_id"]:
            secrets.append(self.notary_password)
        command.append(target)
        run(command, input="".join(value + "\n" for value in secrets).encode("utf-8"))

    def create_icons(self) -> None:
        for source, name in ((self.repository / "Editor/Assets/icon.ico", "AppIcon"),
                             (self.repository / "Editor/Assets/project-icon.png", "ProjectIcon")):
            work = self.work / "editor-icons" / name
            iconset = work / f"{name}.iconset"
            iconset.mkdir(parents=True, exist_ok=True)
            png = work / "source.png"
            run(["sips", "-s", "format", "png", source, "--out", png], quiet=True)
            for size in (16, 32, 128, 256, 512):
                run(["sips", "-z", str(size), str(size), png, "--out", iconset / f"icon_{size}x{size}.png"], quiet=True)
                run(["sips", "-z", str(size * 2), str(size * 2), png,
                     "--out", iconset / f"icon_{size}x{size}@2x.png"], quiet=True)
            output = self.resources / f"{name}.icns"
            run(["iconutil", "-c", "icns", iconset, "-o", output])
            require_file(output)

    def create_assets(self) -> None:
        work = self.work / "editor-dmg-installer-icon"
        work.mkdir(parents=True, exist_ok=True)
        self.background.mkdir(parents=True, exist_ok=True)
        icon = work / "installer-icon.png"
        resource = work / "installer-icon.rsrc"
        run(["sips", "-s", "format", "png", self.assets / "background.svg",
             "--out", self.background / "background.png"], quiet=True)
        shutil.copy2(self.assets / "install_official_plugins.command", self.installer)
        shutil.copy2(self.resources / "AppIcon.icns", self.root / ".VolumeIcon.icns")
        (self.root / ".metadata_never_index").touch()
        run(["sips", "-s", "format", "png", self.assets / "installer_icon.svg", "--out", icon], quiet=True)
        run(["sips", "-z", "512", "512", icon], quiet=True)
        run(["sips", "-i", icon], quiet=True)
        for path in (self.background / "background.png", self.installer,
                     self.root / ".VolumeIcon.icns", self.root / ".metadata_never_index"):
            run(["xattr", "-c", path])
        resource.write_bytes(run(["DeRez", "-only", "icns", icon], capture=True).stdout)
        run(["Rez", "-append", resource, "-o", self.installer])
        self.installer.chmod(self.installer.stat().st_mode | 0o111)
        run(["SetFile", "-a", "CE", self.installer])
        run(["SetFile", "-c", "icnC", self.root / ".VolumeIcon.icns"])
        for path in (self.background, self.payload, self.root / ".VolumeIcon.icns", self.root / ".metadata_never_index"):
            run(["SetFile", "-a", "V", path])

    def validate_root(self, root: pathlib.Path, layout: bool, validate_app: Callable[[pathlib.Path], None]) -> None:
        applications = root / "Applications"
        installer = root / self.installer.name
        payload = root / self.payload.name
        background = root / ".background/background.png"
        volume_icon = root / ".VolumeIcon.icns"
        marker = root / ".metadata_never_index"
        if not applications.is_symlink() or os.readlink(applications) != "/Applications":
            raise PackError(f"The Applications link target is invalid: {applications}")
        require_file(installer, executable=True)
        if installer.is_symlink() or installer.read_bytes() != (self.assets / "install_official_plugins.command").read_bytes():
            raise PackError(f"The DMG official plug-in installer is invalid: {installer}")
        run(["DeRez", "-only", "icns", installer], quiet=True)
        attributes = _text(["GetFileInfo", "-a", installer])
        if "C" not in attributes or "E" not in attributes:
            raise PackError("The DMG official plug-in installer icon or hidden-extension flag is missing.")
        for path in (payload, background.parent):
            require_directory(path)
        for path in (background, volume_icon, marker):
            require_file(path)
        for path in (payload, background.parent, background, volume_icon, marker):
            if path.is_symlink():
                raise PackError(f"A symbolic link was found in the DMG support files: {path}")
        editor_official_plugins.validate(payload)
        validate_app(root / "Ludork.app")
        if {path.name for path in background.parent.iterdir()} != {"background.png"}:
            raise PackError("Unexpected DMG background entry was found.")
        dimensions = _text(["sips", "-g", "pixelWidth", "-g", "pixelHeight", background])
        properties = dict(line.strip().split(": ", 1) for line in dimensions.splitlines() if ": " in line)
        if properties.get("pixelWidth") != "760" or properties.get("pixelHeight") != "500":
            raise PackError("The DMG background dimensions are invalid.")
        _require_format(background, "PNG image data")
        _require_format(volume_icon, "Mac OS X icon")
        if marker.stat().st_size:
            raise PackError("The DMG Spotlight marker is invalid.")
        for path in (installer, background, volume_icon, marker):
            if run(["xattr", "-p", "com.apple.quarantine", path], capture=True, check=False).returncode == 0:
                raise PackError(f"Unexpected quarantine metadata was found in the DMG: {path}")
        finder = root / ".DS_Store"
        if layout:
            require_file(finder)
            if finder.is_symlink():
                raise PackError("The DMG Finder layout is a symbolic link.")
            _require_format(finder, "Apple Desktop Services Store")
            strings = run(["strings", finder], capture=True, env={**os.environ, "LC_ALL": "C"}).stdout
            if b"background.png" not in strings or str(self.repository).encode() in strings:
                raise PackError("The DMG Finder layout has an invalid background or contains the workspace path.")
            if "C" not in _text(["GetFileInfo", "-a", root]):
                raise PackError("The DMG custom volume icon flag is missing.")
        elif finder.exists() or finder.is_symlink():
            raise PackError("The staged DMG contains a Finder layout before image creation.")
        expected = {"Ludork.app", "Applications", self.installer.name, self.payload.name, ".background",
                    ".DS_Store", ".VolumeIcon.icns", ".metadata_never_index"}
        for path in root.iterdir():
            if path.name not in expected:
                raise PackError(f"Unexpected DMG root entry was found: {path}")

    def attach(self, image: pathlib.Path, *, readonly: bool) -> None:
        if not self.mount_owned:
            self.mount.mkdir()
            self.mount_owned = True
        self.image = image
        self.mounted = True
        run(["hdiutil", "attach", "-readonly" if readonly else "-readwrite", "-nobrowse", "-noautoopen",
             "-mountpoint", self.mount, image], quiet=True)

    def attached_device(self) -> str | None:
        information = plistlib.loads(run(["hdiutil", "info", "-plist"], capture=True).stdout)
        for image in information.get("images", []):
            name = image.get("image-path")
            if name and self.image is not None and pathlib.Path(name).resolve() == self.image.resolve():
                entities = image.get("system-entities", [])
                if entities and entities[0].get("dev-entry"):
                    return entities[0]["dev-entry"]
                raise PackError(f"Mounted editor disk image has no device: {self.image}")
        return None

    def detach(self, *, discard: bool = False) -> None:
        for attempt in range(5):
            device = self.attached_device()
            if device is None:
                self.mounted = False
                return
            if attempt == 4:
                raise PackError(f"Editor disk image is still attached: {self.image} ({device})")
            print(f"Detaching editor disk image ({attempt + 1}/4): {self.image} ({device})", flush=True)
            command = ["hdiutil", "detach"]
            if attempt and discard:
                command.append("-force")
            command.append(device)
            try:
                if attempt and self.mount.is_mount():
                    unmount = ["/sbin/umount"]
                    if discard:
                        unmount.append("-f")
                    run([*unmount, self.mount], quiet=True, check=False, timeout=30)
                run(command, quiet=True, check=False, timeout=30)
            except subprocess.TimeoutExpired:
                print(f"Editor disk image detach timed out: {self.image} ({device})", file=sys.stderr)
            time.sleep(2)

    def create(self, full_version: str, *, templates: bool,
               validate_app: Callable[[pathlib.Path], None]) -> pathlib.Path:
        print("Signing the editor application...", flush=True)
        self.sign("app", self.app, templates=templates)
        if self.arguments.with_build_tools:
            run(["tar", "-czf", self.stage / "Ludork-BuildTools-macos-arm64.tar.gz",
                 "-C", self.resources, "BuildTools"])
        editor_official_plugins.prepare(self.repository / "Plugins", self.payload)
        (self.root / "Applications").symlink_to("/Applications")
        self.create_assets()
        self.validate_root(self.root, False, validate_app)
        volume = f"Ludork {full_version}"
        output = self.stage / f"Ludork-{full_version}-macos-arm64.dmg"
        print("Creating macOS editor DMG...", flush=True)
        self.owned_images.add(self.layout_image)
        run(["hdiutil", "create", "-srcfolder", self.root, "-volname", volume, "-fs", "HFS+",
             "-format", "UDRW", "-nospotlight", "-ov", self.layout_image], quiet=True)
        self.attach(self.layout_image, readonly=False)
        layout_command = ["osascript", self.assets / "configure_layout.applescript", self.mount]
        if run(layout_command, check=False).returncode:
            time.sleep(2)
            run(layout_command)
        shutil.copy2(self.root / ".VolumeIcon.icns", self.mount / ".VolumeIcon.icns")
        run(["xattr", "-c", self.mount / ".VolumeIcon.icns"])
        run(["SetFile", "-c", "icnC", self.mount / ".VolumeIcon.icns"])
        run(["SetFile", "-a", "V", self.mount / ".VolumeIcon.icns"])
        run(["SetFile", "-a", "C", self.mount])
        run(["sync"])
        self.validate_root(self.mount, True, validate_app)
        self.owned_images.add(self.publish_image)
        run(["hdiutil", "create", "-srcfolder", self.mount, "-volname", volume, "-fs", "HFS+",
             "-format", "UDRW", "-noscrub", "-nospotlight", "-ov", self.publish_image], quiet=True)
        self.detach(discard=True)
        self.layout_image.unlink()
        self.owned_images.discard(self.layout_image)
        self.attach(self.publish_image, readonly=False)
        run(["SetFile", "-a", "C", self.mount])
        run(["sync"])
        self.detach()
        self.mount.rmdir()
        self.mount_owned = False
        run(["hdiutil", "convert", self.publish_image, "-format", "UDZO", "-ov", "-o", output], quiet=True)
        self.publish_image.unlink()
        self.owned_images.discard(self.publish_image)
        run(["hdiutil", "verify", output], quiet=True)
        self.attach(output, readonly=True)
        run(["diskutil", "verifyVolume", self.mount])
        self.validate_root(self.mount, True, validate_app)
        self.detach()
        self.mount.rmdir()
        self.mount_owned = False
        if self.arguments.notarize:
            self.sign("dmg-notarize", output)
        elif self.signing["signing_identity"] or self.signing["certificate"]:
            self.sign("dmg", output)
        return output

    def cleanup(self) -> bool:
        if self.mounted:
            try:
                self.detach(discard=True)
            except (OSError, RuntimeError, ValueError) as error:
                print(f"Failed to detach the editor DMG; preserving {self.work}: {error}", file=sys.stderr)
                return False
        if self.mount_owned:
            try:
                self.mount.rmdir()
                self.mount_owned = False
            except OSError as error:
                print(f"Failed to remove editor DMG mount directory: {error}", file=sys.stderr)
        for path in tuple(self.owned_images):
            try:
                path.unlink(missing_ok=True)
                self.owned_images.discard(path)
            except OSError as error:
                print(f"Failed to remove temporary editor image {path}: {error}", file=sys.stderr)
        return True
