from __future__ import annotations

import os
import pathlib
import plistlib
import re
import shutil
import subprocess


EXIT_TOOLCHAIN = 20
EXIT_DEVICE = 21
EXIT_SIGNING = 22
EXIT_PROJECT = 23
EXIT_APP_NAME_UNCHANGED = 24


class PackError(RuntimeError):
    def __init__(self, message: str, exit_code: int = 1) -> None:
        super().__init__(message)
        self.exit_code = exit_code


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
    configured = configured.strip().upper()
    if configured:
        if not re.fullmatch(r"[A-Z0-9]{10}", configured):
            raise PackError(
                "LUDORK_IOS_DEVELOPMENT_TEAM must be a 10-character Apple Team ID.",
                EXIT_SIGNING,
            )
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
        "Multiple signed-in Apple Development teams were found. Set LUDORK_IOS_DEVELOPMENT_TEAM before starting Ludork.",
        EXIT_SIGNING,
    )


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


def select_team_id() -> str:
    return choose_team_id(
        signed_in_xcode_teams(),
        os.environ.get("LUDORK_IOS_DEVELOPMENT_TEAM", ""),
    )
