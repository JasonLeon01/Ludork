from __future__ import annotations

import os
import pathlib
import plistlib
import re
import shutil
import subprocess
from dataclasses import dataclass
from .pack_error import PackError
from .packaging_constants import (
    EXIT_DEVICE,
    EXIT_PROJECT,
    EXIT_SIGNING,
    EXIT_TOOLCHAIN,
)


def run_capture(
    command: list[str],
    *,
    environment: dict[str, str] | None = None,
    timeout: int | None = None,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            check=False,
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


def run_streaming(
    command: list[str],
    *,
    environment: dict[str, str],
    cwd: pathlib.Path | None = None,
) -> None:
    print("> " + " ".join(command), flush=True)
    try:
        process = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
            cwd=cwd,
            bufsize=1,
        )
    except OSError as exception:
        raise PackError(str(exception)) from exception
    if process.stdout is not None:
        for line in process.stdout:
            print(line, end="", flush=True)
    return_code = process.wait()
    if return_code != 0:
        raise PackError(
            f"Command failed with exit code {return_code}: {command[0]}"
        )


def valid_developer_dir(path: pathlib.Path) -> bool:
    return (
        path.is_dir()
        and (path / "usr" / "bin" / "xcodebuild").is_file()
        and (
            path
            / "Platforms"
            / "iPhoneOS.platform"
            / "Developer"
            / "SDKs"
        ).is_dir()
    )


def resolve_developer_dir() -> pathlib.Path:
    candidates: list[pathlib.Path] = []
    configured = os.environ.get("DEVELOPER_DIR", "").strip()
    if configured:
        candidates.append(pathlib.Path(configured).expanduser())
    selected = run_capture(["xcode-select", "-p"], timeout=10)
    if selected.returncode == 0 and selected.stdout.strip():
        candidates.append(pathlib.Path(selected.stdout.strip()))
    candidates.append(pathlib.Path("/Applications/Xcode.app/Contents/Developer"))
    seen: set[pathlib.Path] = set()
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if valid_developer_dir(resolved):
            return resolved
    raise PackError(
        "A complete Xcode installation with the iPhoneOS SDK was not found.",
        EXIT_TOOLCHAIN,
    )


def resolve_cmake() -> pathlib.Path:
    configured = shutil.which("cmake")
    if configured:
        return pathlib.Path(configured).resolve()
    application_cmake = pathlib.Path("/Applications/CMake.app/Contents/bin/cmake")
    if application_cmake.is_file():
        return application_cmake
    raise PackError("CMake was not found.", EXIT_TOOLCHAIN)


def require_cmake(cmake: pathlib.Path) -> str:
    result = run_capture([str(cmake), "--version"], timeout=20)
    if result.returncode != 0 or not result.stdout.strip():
        detail = result.stdout.strip()
        message = "CMake is unavailable."
        if detail:
            message += f"\n{detail}"
        raise PackError(message, EXIT_TOOLCHAIN)
    return result.stdout.strip().splitlines()[0]


def require_xcode_tools(developer_dir: pathlib.Path) -> dict[str, str]:
    environment = os.environ.copy()
    environment["DEVELOPER_DIR"] = str(developer_dir)
    first_launch = run_capture(
        ["xcodebuild", "-checkFirstLaunchStatus"],
        environment=environment,
        timeout=30,
    )
    if first_launch.returncode != 0:
        detail = first_launch.stdout.strip()
        message = "Xcode first-launch setup is incomplete."
        if detail:
            message += f"\n{detail}"
        raise PackError(message, EXIT_TOOLCHAIN)
    commands = (
        (["xcodebuild", "-version"], "xcodebuild"),
        (["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], "iPhoneOS SDK"),
    )
    values: dict[str, str] = {}
    for command, label in commands:
        result = run_capture(command, environment=environment, timeout=20)
        if result.returncode != 0 or not result.stdout.strip():
            detail = result.stdout.strip()
            message = f"{label} is unavailable."
            if detail:
                message += f"\n{detail}"
            raise PackError(message, EXIT_TOOLCHAIN)
        values[label] = result.stdout.strip()
    return values


def choose_team_id(teams: set[str], configured: str) -> str:
    configured = validate_team_id(configured)
    if configured:
        if configured not in teams:
            raise PackError(
                f"No signed-in Xcode account is associated with team {configured}.",
                EXIT_SIGNING,
            )
        return configured
    if len(teams) == 1:
        return next(iter(teams))
    if not teams:
        raise PackError(
            "No Apple Development team is available from the accounts signed in to Xcode.",
            EXIT_SIGNING,
        )
    raise PackError(
        "Multiple signed-in Apple Development teams were found. "
        "Set LUDORK_IOS_DEVELOPMENT_TEAM or pass --team-id before packaging.",
        EXIT_SIGNING,
    )


def validate_team_id(value: str) -> str:
    configured = value.strip().upper()
    if not configured:
        return ""
    if not re.fullmatch(r"[A-Z0-9]{10}", configured):
        raise PackError(
            "The Apple Team ID must be 10 characters of A-Z and 0-9.",
            EXIT_SIGNING,
        )
    return configured


def xcode_account_identifiers(value: object) -> set[str]:
    if isinstance(value, str):
        identifier = value.strip()
        return {identifier} if identifier else set()
    if isinstance(value, list):
        return set().union(*(xcode_account_identifiers(item) for item in value))
    if isinstance(value, dict):
        return set().union(
            *(xcode_account_identifiers(item) for item in value.values())
        )
    return set()


def xcode_team_ids(value: object) -> set[str]:
    if isinstance(value, list):
        return set().union(*(xcode_team_ids(item) for item in value))
    if not isinstance(value, dict):
        return set()
    result = set().union(*(xcode_team_ids(item) for item in value.values()))
    team_id = value.get("teamID")
    if isinstance(team_id, str) and re.fullmatch(r"[A-Z0-9]{10}", team_id):
        result.add(team_id)
    return result


def xcode_account_team_ids(
    account_preferences: dict[str, object],
    provisioning_preferences: dict[str, object],
) -> set[str]:
    accounts = account_preferences.get("DVTDeveloperAccountManagerAppleIDLists")
    teams = provisioning_preferences.get("IDEProvisioningTeamByIdentifier")
    if not isinstance(teams, dict):
        return set()
    return set().union(
        *(
            xcode_team_ids(teams.get(identifier))
            for identifier in xcode_account_identifiers(accounts)
        )
    )


def read_xcode_preferences(domain: str) -> dict[str, object]:
    result = run_capture(
        ["defaults", "export", domain, "-"],
        timeout=20,
    )
    if result.returncode != 0:
        return {}
    try:
        preferences = plistlib.loads(result.stdout.encode("utf-8"))
    except (plistlib.InvalidFileException, ValueError):
        return {}
    return preferences if isinstance(preferences, dict) else {}


def signed_in_xcode_teams() -> set[str]:
    account_preferences = read_xcode_preferences("com.apple.dt.Xcode")
    provisioning_preferences = read_xcode_preferences("com.apple.dt.xcodebuild")
    teams = xcode_account_team_ids(
        account_preferences,
        provisioning_preferences,
    )
    if teams:
        return teams
    return xcode_account_team_ids(account_preferences, account_preferences)


def select_team_id(configured: str, manual_signing: bool) -> str:
    if manual_signing:
        team_id = validate_team_id(configured)
        if not team_id:
            raise PackError(
                "Manual iOS signing requires a team ID. "
                "Set LUDORK_IOS_DEVELOPMENT_TEAM or pass --team-id.",
                EXIT_SIGNING,
            )
        return team_id
    return choose_team_id(
        signed_in_xcode_teams(),
        configured,
    )


@dataclass(frozen=True)
class ProvisioningProfile:
    name: str
    uuid: str
    team_identifiers: tuple[str, ...]
    application_identifier: str


def read_provisioning_profile(path: pathlib.Path) -> ProvisioningProfile:
    result = run_capture(["security", "cms", "-D", "-i", str(path)], timeout=60)
    if result.returncode != 0 or not result.stdout.strip():
        raise PackError(
            "The iOS provisioning profile could not be read.\n" + result.stdout.strip(),
            EXIT_SIGNING,
        )
    try:
        value = plistlib.loads(result.stdout.encode("utf-8"))
    except (plistlib.InvalidFileException, ValueError) as exception:
        raise PackError(
            f"The iOS provisioning profile is invalid: {path}",
            EXIT_SIGNING,
        ) from exception
    if not isinstance(value, dict):
        raise PackError(f"The iOS provisioning profile is invalid: {path}", EXIT_SIGNING)
    entitlements = value.get("Entitlements")
    application_identifier = (
        entitlements.get("application-identifier")
        if isinstance(entitlements, dict)
        else None
    )
    teams = value.get("TeamIdentifier")
    name = value.get("Name")
    uuid = value.get("UUID")
    if (
        not isinstance(name, str)
        or not name.strip()
        or not isinstance(uuid, str)
        or not uuid.strip()
        or not isinstance(teams, list)
        or not isinstance(application_identifier, str)
        or not application_identifier.strip()
    ):
        raise PackError(
            f"The iOS provisioning profile is incomplete: {path}",
            EXIT_SIGNING,
        )
    return ProvisioningProfile(
        name.strip(),
        uuid.strip(),
        tuple(str(team) for team in teams),
        application_identifier.strip(),
    )


def validate_provisioning_profile(
    profile: ProvisioningProfile,
    team_id: str,
    bundle_identifier: str,
) -> None:
    if team_id not in profile.team_identifiers:
        raise PackError(
            f"The iOS provisioning profile is not issued for team {team_id}.",
            EXIT_SIGNING,
        )
    prefix = team_id + "."
    if not profile.application_identifier.startswith(prefix):
        raise PackError(
            "The iOS provisioning profile does not belong to the selected team.",
            EXIT_SIGNING,
        )
    application_identifier = profile.application_identifier[len(prefix):]
    if application_identifier not in {"*", bundle_identifier}:
        raise PackError(
            "The iOS provisioning profile covers "
            f"{application_identifier}, which does not match {bundle_identifier}.",
            EXIT_SIGNING,
        )


def install_provisioning_profile(
    source: pathlib.Path,
    profile: ProvisioningProfile,
) -> tuple[pathlib.Path, bool]:
    directory = pathlib.Path.home() / "Library" / "MobileDevice" / "Provisioning Profiles"
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / f"{profile.uuid}.mobileprovision"
    existed = destination.is_file()
    shutil.copy2(source, destination)
    return destination, existed
