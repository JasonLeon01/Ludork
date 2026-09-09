from __future__ import annotations

import argparse
import pathlib

from .ui_preview import describe_host, load_preview
from .ui_property_values import UiAssetError


def verify_ui_adapters(project_root: pathlib.Path) -> str:
    root = project_root.expanduser().resolve()
    if not (root / "Main.proj").is_file() and (root / "Sample" / "Main.proj").is_file():
        root /= "Sample"
    snapshot = load_preview(root)
    compiled = describe_host(snapshot.host_path, root)
    if compiled.raw != snapshot.registry.raw:
        raise UiAssetError(
            "Project preview and registry descriptions differ; rebuild the project preview"
        )
    return compiled.fingerprint


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools ui-adapter-check")
    parser.add_argument(
        "project_root", nargs="?", type=pathlib.Path, default=pathlib.Path.cwd()
    )
    parsed = parser.parse_args(arguments)
    try:
        fingerprint = verify_ui_adapters(parsed.project_root)
    except (OSError, ValueError, UiAssetError) as error:
        parser.exit(1, f"{error}\n")
    print(f"UI adapter descriptors are consistent: {fingerprint}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
