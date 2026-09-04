from __future__ import annotations

import sys
from collections.abc import Callable

from ScriptTools import android_pack
from ScriptTools import compile_lua
from ScriptTools import configure_project_template
from ScriptTools import editor_macos_metadata
from ScriptTools import editor_official_plugins
from ScriptTools import finalize_package
from ScriptTools import harmony_pack
from ScriptTools import ide_config
from ScriptTools import impl_boundary_check
from ScriptTools import ios_pack
from ScriptTools import ldpak
from ScriptTools import macos_bundle
from ScriptTools import project_runtime_mode
from ScriptTools import prune_editor_macos_publish
from ScriptTools import prune_editor_windows_publish
from ScriptTools import ui_adapter_check
from ScriptTools import ui_assets
from ScriptTools.core_bindgen import generate
from ScriptTools.core_bindgen import layout


Command = Callable[[list[str] | None], int]

COMMANDS: dict[str, Command] = {
    "android-pack": android_pack.main,
    "core-bindgen": generate.main,
    "core-bindgen-layout": layout.main,
    "configure-project-template": configure_project_template.main,
    "editor-macos-metadata": editor_macos_metadata.main,
    "editor-official-plugins": editor_official_plugins.main,
    "finalize-package": finalize_package.main,
    "harmony-pack": harmony_pack.main,
    "ide-config": ide_config.main,
    "impl-boundary-check": impl_boundary_check.main,
    "project-runtime-mode": project_runtime_mode.main,
    "macos-bundle": macos_bundle.main,
    "ios-pack": ios_pack.main,
    "compile-lua": compile_lua.main,
    "prune-editor-macos-publish": prune_editor_macos_publish.main,
    "prune-editor-windows-publish": prune_editor_windows_publish.main,
    "ui-adapter-check": ui_adapter_check.main,
    "ui-assets": ui_assets.main,
    "validate-ldpak-source": ldpak.main,
}


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        commands = ", ".join(sorted(COMMANDS))
        print(f"Usage: ScriptTools <command> [arguments]\nCommands: {commands}", file=sys.stderr)
        return 2
    return COMMANDS[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    raise SystemExit(main())
