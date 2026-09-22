from __future__ import annotations

import json
import os
import pathlib
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import TextIO

from .pack_error import PackError
from .packaging_constants import EXIT_SIGNING, EXIT_TOOLCHAIN


MACOS_SIGNING_IDENTITY_ENVIRONMENT = "LUDORK_MACOS_SIGNING_IDENTITY"
MACOS_SIGNING_CERTIFICATE_ENVIRONMENT = "LUDORK_MACOS_SIGNING_CERTIFICATE"
MACOS_SIGNING_ENTITLEMENTS_ENVIRONMENT = "LUDORK_MACOS_SIGNING_ENTITLEMENTS"
MACOS_NOTARY_APPLE_ID_ENVIRONMENT = "LUDORK_MACOS_NOTARY_APPLE_ID"
MACOS_NOTARY_TEAM_ID_ENVIRONMENT = "LUDORK_MACOS_NOTARY_TEAM_ID"
MACOS_NOTARY_KEY_ENVIRONMENT = "LUDORK_MACOS_NOTARY_KEY"
MACOS_NOTARY_KEY_ID_ENVIRONMENT = "LUDORK_MACOS_NOTARY_KEY_ID"
MACOS_NOTARY_KEY_ISSUER_ENVIRONMENT = "LUDORK_MACOS_NOTARY_KEY_ISSUER"
IOS_DEVELOPMENT_TEAM_ENVIRONMENT = "LUDORK_IOS_DEVELOPMENT_TEAM"
IOS_SIGNING_IDENTITY_ENVIRONMENT = "LUDORK_IOS_SIGNING_IDENTITY"
IOS_SIGNING_CERTIFICATE_ENVIRONMENT = "LUDORK_IOS_SIGNING_CERTIFICATE"
IOS_PROVISIONING_PROFILE_ENVIRONMENT = "LUDORK_IOS_PROVISIONING_PROFILE"

AD_HOC_IDENTITY = "-"
NOTARY_KEYCHAIN_PROFILE = "ludork-notary"
KEYCHAIN_TIMEOUT_SECONDS = "21600"

_BUNDLE_SUFFIXES = frozenset({".app", ".appex", ".bundle", ".framework", ".plugin", ".xpc"})
_CODE_LOCATION_DIRECTORIES = frozenset(
    {"MacOS", "Frameworks", "SharedFrameworks", "PlugIns", "Helpers", "XPCServices", "Library"}
)
_MACH_O_MAGICS = frozenset(
    (
        b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
        b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
        b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
    )
)
_IDENTITY_PATTERN = re.compile(
    r"^\s*\d+\)\s+(?P<identifier>[0-9A-Fa-f]{40})\s+\"(?P<name>.+?)\""
    r"(?:\s+\((?P<status>[^)]*)\))?\s*$"
)
TEAM_ID_PATTERN = re.compile(r"^[A-Z0-9]{10}$")


def require_macos(description: str) -> None:
    if sys.platform != "darwin":
        raise PackError(f"{description} is only supported on macOS.", EXIT_TOOLCHAIN)


@dataclass(frozen=True)
class OptionSource:
    ignore_environment: bool = False

    def value(self, environment: str, argument: str | None) -> str:
        if not self.ignore_environment:
            configured = os.environ.get(environment, "").strip()
            if configured:
                return configured
        return argument.strip() if argument else ""


def absolute_path(value: str, description: str, environment: str) -> pathlib.Path:
    if not value:
        raise PackError(
            f"{description} was not provided. Set {environment} or pass it explicitly.",
            EXIT_SIGNING,
        )
    if not value.startswith("/"):
        raise PackError(f"{description} must be an absolute path.", EXIT_SIGNING)
    path = pathlib.Path(value).expanduser()
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as exception:
        raise PackError(f"{description} is unavailable: {path}", EXIT_SIGNING) from exception
    if not resolved.is_file() or not os.access(resolved, os.R_OK):
        raise PackError(f"{description} is unavailable: {resolved}", EXIT_SIGNING)
    return resolved


def read_secret_lines(input_stream: TextIO, count: int, description: str) -> list[str]:
    values: list[str] = []
    for _ in range(count):
        line = input_stream.readline()
        if line == "":
            raise PackError(
                f"{description} requires {count} non-empty UTF-8 line(s) on standard input.",
                EXIT_SIGNING,
            )
        value = line[:-1] if line.endswith("\n") else line
        if not value or "\r" in value:
            raise PackError(
                f"{description} requires non-empty single-line UTF-8 values on standard input.",
                EXIT_SIGNING,
            )
        values.append(value)
    return values


def _run_capture(
    command: list[str],
    *,
    environment: dict[str, str] | None = None,
    input_text: str | None = None,
    timeout: int | None = 600,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            check=False,
            input=input_text,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        return subprocess.CompletedProcess(command, 1, str(exception))


def _run_streaming(
    command: list[str],
    *,
    environment: dict[str, str] | None = None,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    print("> " + " ".join(command), flush=True)
    result = _run_capture(command, environment=environment, input_text=input_text, timeout=None)
    if result.stdout:
        print(result.stdout.rstrip("\n"), flush=True)
    if result.returncode != 0:
        raise PackError(
            f"Command failed with exit code {result.returncode}: {command[0]}",
            EXIT_SIGNING,
        )
    return result


def _run_checked(command: list[str], description: str, *, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    result = _run_capture(command, timeout=timeout)
    if result.returncode != 0:
        detail = result.stdout.strip()
        message = f"{description} failed."
        if detail:
            message += f"\n{detail}"
        raise PackError(message, EXIT_SIGNING)
    return result


def _required_command(name: str) -> str:
    located = shutil.which(name)
    if located is None:
        raise PackError(f"{name} was not found.", EXIT_TOOLCHAIN)
    return located


@dataclass(frozen=True)
class SigningIdentity:
    identifier: str
    name: str


def _search_list() -> list[str]:
    result = _run_capture(["security", "list-keychains", "-d", "user"], timeout=30)
    if result.returncode != 0:
        return []
    return [
        line.strip().strip('"')
        for line in result.stdout.splitlines()
        if line.strip().strip('"')
    ]


def _parse_identities(output: str) -> tuple[tuple[SigningIdentity, str], ...]:
    identities: list[tuple[SigningIdentity, str]] = []
    for line in output.splitlines():
        match = _IDENTITY_PATTERN.match(line)
        if match is not None:
            identities.append(
                (
                    SigningIdentity(match.group("identifier").upper(), match.group("name")),
                    (match.group("status") or "").strip(),
                )
            )
    return tuple(identities)


class TemporaryKeychain:
    def __init__(self) -> None:
        self._directory: pathlib.Path | None = None
        self._path: pathlib.Path | None = None
        self._password = ""
        self._previous_search_list: list[str] = []

    @property
    def path(self) -> pathlib.Path:
        if self._path is None:
            raise PackError("The signing keychain was not created.", EXIT_SIGNING)
        return self._path

    def __enter__(self) -> TemporaryKeychain:
        self._directory = pathlib.Path(tempfile.mkdtemp(prefix="ludork-signing-"))
        self._path = self._directory / "ludork-signing.keychain-db"
        self._password = secrets.token_urlsafe(24)
        self._previous_search_list = _search_list()
        _run_checked(
            ["security", "create-keychain", "-p", self._password, str(self.path)],
            "Creating the signing keychain",
        )
        _run_checked(
            ["security", "set-keychain-settings", "-lut", KEYCHAIN_TIMEOUT_SECONDS, str(self.path)],
            "Configuring the signing keychain",
        )
        _run_checked(
            ["security", "unlock-keychain", "-p", self._password, str(self.path)],
            "Unlocking the signing keychain",
        )
        _run_checked(
            ["security", "list-keychains", "-d", "user", "-s", str(self.path), *self._previous_search_list],
            "Enabling the signing keychain",
        )
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> None:
        self.close()

    def close(self) -> None:
        if self._path is not None and self._path.exists():
            if self._previous_search_list:
                _run_capture(
                    ["security", "list-keychains", "-d", "user", "-s", *self._previous_search_list],
                    timeout=30,
                )
            _run_capture(["security", "delete-keychain", str(self.path)], timeout=30)
        if self._directory is not None:
            shutil.rmtree(self._directory, ignore_errors=True)
        self._path = None
        self._directory = None

    def add_certificate(self, certificate: pathlib.Path, password: str) -> None:
        _run_checked(
            [
                "security",
                "import",
                str(certificate),
                "-k",
                str(self.path),
                "-P",
                password,
                "-A",
                "-T",
                "/usr/bin/codesign",
                "-T",
                "/usr/bin/security",
            ],
            "Importing the signing certificate",
        )
        _run_checked(
            [
                "security",
                "set-key-partition-list",
                "-S",
                "apple-tool:,apple:,codesign:",
                "-s",
                "-k",
                self._password,
                str(self.path),
            ],
            "Preparing the signing certificate for codesign",
        )

    def identities(self) -> tuple[SigningIdentity, ...]:
        entries = _parse_identities(
            _run_checked(
                ["security", "find-identity", "-p", "codesigning", str(self.path)],
                "Searching the signing keychain for identities",
                timeout=60,
            ).stdout
        )
        valid = tuple(identity for identity, status in entries if not status)
        if valid:
            return valid
        if entries:
            names = "\n".join(
                f"    {identity.name} ({status})" for identity, status in entries
            )
            raise PackError(
                "The signing certificate is not trusted for code signing. "
                "Install the certificate authority that issued it:\n" + names,
                EXIT_SIGNING,
            )
        return ()


def select_identity(
    identities: tuple[SigningIdentity, ...],
    requested: str,
) -> SigningIdentity:
    if requested:
        for identity in identities:
            if (
                identity.name == requested
                or identity.identifier.lower() == requested.lower()
                or identity.name.startswith(requested)
            ):
                return identity
        raise PackError(
            f"The requested signing identity was not found in the signing keychain: {requested}",
            EXIT_SIGNING,
        )
    if len(identities) == 1:
        return identities[0]
    if not identities:
        raise PackError(
            "The signing certificate contains no code-signing identity.",
            EXIT_SIGNING,
        )
    names = "\n".join(f"    {identity.name}" for identity in identities)
    raise PackError(
        "The signing certificate contains multiple code-signing identities. "
        "Select one explicitly:\n" + names,
        EXIT_SIGNING,
    )


def is_mach_o(path: pathlib.Path) -> bool:
    try:
        with path.open("rb") as stream:
            return stream.read(4) in _MACH_O_MAGICS
    except OSError:
        return False


def _depth(path: pathlib.Path) -> int:
    return len(path.parts)


def nested_sign_targets(root: pathlib.Path) -> list[pathlib.Path]:
    targets: list[pathlib.Path] = []
    for directory, directory_names, file_names in os.walk(root):
        current = pathlib.Path(directory)
        directory_names.sort()
        for name in sorted(directory_names):
            path = current / name
            if not path.is_symlink() and path.suffix in _BUNDLE_SUFFIXES:
                targets.append(path)
        for name in sorted(file_names):
            path = current / name
            if not path.is_symlink() and is_mach_o(path):
                targets.append(path)
    return sorted(targets, key=_depth, reverse=True)


def _uses_hardened_runtime(root: pathlib.Path, target: pathlib.Path, hardened: bool) -> bool:
    if not hardened:
        return False
    relative = target.relative_to(root)
    return len(relative.parts) > 1 and relative.parts[0] == "Contents" and (
        relative.parts[1] in _CODE_LOCATION_DIRECTORIES
    )


def _codesign(
    target: pathlib.Path,
    *,
    identity: str,
    environment: dict[str, str],
    hardened_runtime: bool,
    entitlements: pathlib.Path | None = None,
    keychain: pathlib.Path | None = None,
) -> None:
    command = ["codesign", "--force", "--sign", identity]
    if hardened_runtime:
        command.extend(["--options", "runtime"])
    if identity != AD_HOC_IDENTITY:
        command.append("--timestamp")
    if entitlements is not None:
        command.extend(["--entitlements", str(entitlements)])
    if keychain is not None:
        command.extend(["--keychain", str(keychain)])
    command.append(str(target))
    result = _run_capture(command, environment=environment)
    if result.returncode != 0:
        raise PackError(
            f"Code signing failed for {target}.\n" + result.stdout.strip(),
            EXIT_SIGNING,
        )


def verify_signature(target: pathlib.Path, environment: dict[str, str]) -> None:
    command = ["codesign", "--verify", "--strict"]
    if target.is_dir():
        command.append("--deep")
    command.append(str(target))
    result = _run_capture(command, environment=environment)
    if result.returncode != 0:
        raise PackError(
            f"Code signature verification failed for {target}.\n" + result.stdout.strip(),
            EXIT_SIGNING,
        )


def sign_artifact(
    target: pathlib.Path,
    *,
    identity: str,
    environment: dict[str, str],
    entitlements: pathlib.Path | None = None,
    keychain: pathlib.Path | None = None,
    notarized: bool = False,
) -> None:
    if not target.exists():
        raise PackError(f"The signing target was not found: {target}", EXIT_SIGNING)
    hardened = identity != AD_HOC_IDENTITY
    if target.is_dir():
        for nested in nested_sign_targets(target):
            _codesign(
                nested,
                identity=identity,
                environment=environment,
                hardened_runtime=_uses_hardened_runtime(target, nested, hardened),
                keychain=keychain,
            )
        _codesign(
            target,
            identity=identity,
            environment=environment,
            hardened_runtime=hardened,
            entitlements=entitlements,
            keychain=keychain,
        )
    else:
        _codesign(
            target,
            identity=identity,
            environment=environment,
            hardened_runtime=False,
            keychain=keychain,
        )
    verify_signature(target, environment)
    print(f"Signed: {target} [{identity}]", flush=True)


@dataclass(frozen=True)
class NotaryCredentials:
    apple_id: str = ""
    team_id: str = ""
    key: pathlib.Path | None = None
    key_id: str = ""
    key_issuer: str = ""

    @property
    def enabled(self) -> bool:
        return bool(self.apple_id or self.key is not None)

    @property
    def password_required(self) -> bool:
        return bool(self.apple_id)

    def arguments(self, keychain: pathlib.Path | None) -> list[str]:
        if self.apple_id:
            if keychain is None:
                raise PackError("Notarisation requires a signing keychain.", EXIT_SIGNING)
            return [
                "--keychain-profile",
                NOTARY_KEYCHAIN_PROFILE,
                "--keychain",
                str(keychain),
            ]
        if self.key is not None:
            return [
                "--key",
                str(self.key),
                "--key-id",
                self.key_id,
                "--issuer",
                self.key_issuer,
            ]
        raise PackError("Notarisation credentials are incomplete.", EXIT_SIGNING)


def resolve_notary_credentials(
    source: OptionSource,
    *,
    apple_id: str | None,
    team_id: str | None,
    key: pathlib.Path | None,
    key_id: str | None,
    key_issuer: str | None,
) -> NotaryCredentials:
    resolved_apple_id = source.value(MACOS_NOTARY_APPLE_ID_ENVIRONMENT, apple_id)
    resolved_team_id = source.value(MACOS_NOTARY_TEAM_ID_ENVIRONMENT, team_id)
    resolved_key = source.value(
        MACOS_NOTARY_KEY_ENVIRONMENT,
        str(key) if key is not None else None,
    )
    resolved_key_id = source.value(MACOS_NOTARY_KEY_ID_ENVIRONMENT, key_id)
    resolved_key_issuer = source.value(MACOS_NOTARY_KEY_ISSUER_ENVIRONMENT, key_issuer)
    if resolved_apple_id or resolved_team_id:
        if not resolved_apple_id or not resolved_team_id:
            raise PackError(
                "Notarisation with an Apple ID requires both an Apple ID and a team ID.",
                EXIT_SIGNING,
            )
        if not TEAM_ID_PATTERN.fullmatch(resolved_team_id):
            raise PackError(
                "The notarisation team ID must be a 10-character Apple Team ID.",
                EXIT_SIGNING,
            )
        return NotaryCredentials(apple_id=resolved_apple_id, team_id=resolved_team_id)
    if resolved_key or resolved_key_id or resolved_key_issuer:
        if not (resolved_key and resolved_key_id and resolved_key_issuer):
            raise PackError(
                "Notarisation with an App Store Connect key requires a key file, key ID and issuer ID.",
                EXIT_SIGNING,
            )
        return NotaryCredentials(
            key=absolute_path(resolved_key, "The notarisation key", MACOS_NOTARY_KEY_ENVIRONMENT),
            key_id=resolved_key_id,
            key_issuer=resolved_key_issuer,
        )
    raise PackError(
        "Notarisation requires Apple ID credentials or an App Store Connect API key.",
        EXIT_SIGNING,
    )


def store_notary_credentials(
    keychain: TemporaryKeychain,
    credentials: NotaryCredentials,
    password: str,
) -> None:
    result = _run_capture(
        [
            "xcrun",
            "notarytool",
            "store-credentials",
            NOTARY_KEYCHAIN_PROFILE,
            "--apple-id",
            credentials.apple_id,
            "--team-id",
            credentials.team_id,
            "--keychain",
            str(keychain.path),
        ],
        input_text=password + "\n",
        timeout=300,
    )
    if result.returncode != 0:
        raise PackError(
            "The notarisation credentials could not be stored.\n" + result.stdout.strip(),
            EXIT_SIGNING,
        )


def notarize_and_staple(
    artifact: pathlib.Path,
    credentials: NotaryCredentials,
    *,
    keychain: TemporaryKeychain,
    environment: dict[str, str],
) -> None:
    if not artifact.exists():
        raise PackError(f"The notarisation target was not found: {artifact}", EXIT_SIGNING)
    print(f"Notarising: {artifact}", flush=True)
    with tempfile.TemporaryDirectory(prefix="ludork-notary-") as temporary:
        submission = artifact
        if artifact.is_dir():
            submission = pathlib.Path(temporary) / f"{artifact.name}.zip"
            _run_streaming(
                ["ditto", "-c", "-k", "--norsrc", "--keepParent", str(artifact), str(submission)],
                environment=environment,
            )
        result = _run_streaming(
            [
                "xcrun",
                "notarytool",
                "submit",
                str(submission),
                *credentials.arguments(keychain.path),
                "--wait",
                "--output-format",
                "json",
            ],
            environment=environment,
        )
    report = _notary_report(result.stdout)
    status = str(report.get("status", "")).strip()
    if status != "Accepted":
        message = str(report.get("message", "")).strip()
        detail = f"\n{message}" if message else ""
        raise PackError(
            f"Notarisation was not accepted for {artifact}: {status or 'unknown status'}{detail}",
            EXIT_SIGNING,
        )
    _run_streaming(
        ["xcrun", "stapler", "staple", str(artifact)],
        environment=environment,
    )
    _run_streaming(
        ["xcrun", "stapler", "validate", str(artifact)],
        environment=environment,
    )
    print(f"Notarised and stapled: {artifact}", flush=True)


def _notary_report(output: str) -> dict[str, object]:
    start = output.find("{")
    end = output.rfind("}")
    if start < 0 or end <= start:
        return {}
    try:
        value = json.loads(output[start:end + 1])
    except json.JSONDecodeError:
        return {}
    return value if isinstance(value, dict) else {}
