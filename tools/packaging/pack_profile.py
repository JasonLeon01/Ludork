# -*- encoding: utf-8 -*-

from __future__ import annotations

import argparse
import configparser
import os
import sys
from pathlib import Path

_ROOT_DIR = Path(__file__).resolve().parents[2]
_PACK_CONF = _ROOT_DIR / "pack.conf"
_VALID_PROFILES = frozenset({"low", "turbo"})


def _cpuCount() -> int:
    return os.cpu_count() or 1


def resolvePackProfile(override: str | None = None) -> str:
    r"""\brief Resolve the editor packaging CPU profile.

    - \param override - Optional CLI or environment override.
    - \return Either ``low`` or ``turbo``.
    """
    if override:
        normalised = override.strip().lower().lstrip("-")
        if normalised in ("full", "fast", "max"):
            normalised = "turbo"
        if normalised in _VALID_PROFILES:
            return normalised

    envProfile = os.environ.get("LUDORK_PACK_PROFILE", "").strip().lower()
    if envProfile in _VALID_PROFILES:
        return envProfile

    if _PACK_CONF.is_file():
        parser = configparser.ConfigParser()
        parser.read(_PACK_CONF, encoding="utf-8")
        configured = parser.get("packaging", "Profile", fallback="low").strip().lower()
        if configured in _VALID_PROFILES:
            return configured

    return "low"


def packProfileSettings(profile: str) -> dict[str, str]:
    r"""\brief Map a packaging profile to Nuitka option values.

    - \param profile - Resolved packaging profile name.
    - \return Settings used by the pack scripts.
    """
    if profile == "turbo":
        return {
            "PROFILE": "turbo",
            "JOBS": "",
            "LTO": "yes",
            "JOBS_LABEL": "all",
        }

    cpus = _cpuCount()
    jobs = max(1, cpus // 4)
    return {
        "PROFILE": "low",
        "JOBS": str(jobs),
        "LTO": "no",
        "JOBS_LABEL": str(jobs),
    }


def main(argv: list[str]) -> int:
    argumentParser = argparse.ArgumentParser(add_help=False)
    argumentParser.add_argument("override", nargs="?", default="")
    args = argumentParser.parse_args(argv)
    settings = packProfileSettings(resolvePackProfile(args.override or None))
    for key, value in settings.items():
        print(f"{key}={value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
