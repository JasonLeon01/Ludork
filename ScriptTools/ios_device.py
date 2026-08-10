from __future__ import annotations

import json
import pathlib

from .ios_toolchain import EXIT_DEVICE
from .ios_toolchain import EXIT_SIGNING
from .ios_toolchain import EXIT_TOOLCHAIN
from .ios_toolchain import PackError
from .ios_toolchain import run_capture
from .ios_toolchain import run_streaming


IPHONE_PLATFORM = "com.apple.platform.iphoneos"


def require_device_tools(environment: dict[str, str]) -> dict[str, str]:
    commands = (
        (["xcrun", "--find", "xcdevice"], "xcdevice"),
        (["xcrun", "--find", "devicectl"], "devicectl"),
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


def select_iphone(environment: dict[str, str]) -> dict[str, object]:
    result = run_capture(
        ["xcrun", "xcdevice", "list", "--timeout", "12"],
        environment=environment,
        timeout=20,
    )
    if result.returncode != 0:
        raise PackError(
            "Unable to query connected Apple devices.\n" + result.stdout.strip(),
            EXIT_DEVICE,
        )
    try:
        devices = json.loads(result.stdout)
    except json.JSONDecodeError as exception:
        raise PackError(
            f"xcdevice returned invalid JSON: {exception}",
            EXIT_DEVICE,
        ) from exception
    if not isinstance(devices, list):
        raise PackError("xcdevice returned an invalid device list.", EXIT_DEVICE)
    physical_iphones = [
        device
        for device in devices
        if isinstance(device, dict)
        and device.get("simulator") is False
        and device.get("platform") == IPHONE_PLATFORM
    ]
    available = [device for device in physical_iphones if device.get("available") is True]
    if len(available) == 1:
        return available[0]
    if len(available) > 1:
        names = ", ".join(
            f"{device.get('name', 'iPhone')} ({device.get('identifier', 'unknown')})"
            for device in available
        )
        raise PackError(
            f"More than one available physical iPhone was found: {names}",
            EXIT_DEVICE,
        )
    if physical_iphones:
        details: list[str] = []
        for device in physical_iphones:
            name = str(device.get("name", "iPhone")).strip()
            error = device.get("error")
            description = ""
            recovery = ""
            if isinstance(error, dict):
                description = str(error.get("description", "")).strip()
                recovery = str(error.get("recoverySuggestion", "")).strip()
            line = name
            if description:
                line += f": {description}"
            if recovery:
                line += f"\n{recovery}"
            details.append(line)
        raise PackError(
            "A physical iPhone was detected but is unavailable. Unlock it, trust this Mac, and enable Developer Mode.\n"
            + "\n".join(details),
            EXIT_DEVICE,
        )
    raise PackError(
        "No available physical iPhone was found. Connect and unlock one iPhone.",
        EXIT_DEVICE,
    )


def device_identifier(device: dict[str, object]) -> str:
    identifier = str(device.get("identifier", "")).strip()
    if not identifier:
        raise PackError("The selected iPhone has no device identifier.", EXIT_DEVICE)
    return identifier


def requires_developer_trust(output: str) -> bool:
    return "not been explicitly trusted by the user" in output.casefold()


def install_and_launch(
    device: dict[str, object],
    app_path: pathlib.Path,
    bundle_identifier: str,
    game_name: str,
    environment: dict[str, str],
) -> None:
    identifier = device_identifier(device)
    run_streaming(
        [
            "xcrun",
            "devicectl",
            "device",
            "install",
            "app",
            "--device",
            identifier,
            "--timeout",
            "180",
            str(app_path),
        ],
        environment=environment,
    )
    print(f"Installed on {str(device.get('name', 'iPhone')).strip()}.", flush=True)
    launch_command = [
        "xcrun",
        "devicectl",
        "device",
        "process",
        "launch",
        "--device",
        identifier,
        "--timeout",
        "60",
        "--terminate-existing",
        bundle_identifier,
    ]
    print("> " + " ".join(launch_command), flush=True)
    launch = run_capture(
        launch_command,
        environment=environment,
        timeout=90,
    )
    print(launch.stdout, end="" if launch.stdout.endswith("\n") else "\n", flush=True)
    if launch.returncode != 0:
        if requires_developer_trust(launch.stdout):
            raise PackError(
                f"{game_name} was installed, but this iPhone has not trusted the developer profile. "
                "On the iPhone, open Settings > General > VPN & Device Management, trust the Developer App "
                "for the signed-in Apple account, then run packaging again.",
                EXIT_SIGNING,
            )
        raise PackError(
            f"Command failed with exit code {launch.returncode}: {launch_command[0]}"
        )
    print(f"Launched {game_name}.", flush=True)
