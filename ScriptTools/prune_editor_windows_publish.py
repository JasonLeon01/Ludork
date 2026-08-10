from __future__ import annotations

import argparse
import pathlib

from ScriptTools.prune_editor_macos_publish import prunePublish


FOREIGN_ASSEMBLIES = {
    "Avalonia.FreeDesktop.AtSpi.dll",
    "Avalonia.FreeDesktop.dll",
    "Avalonia.Native.dll",
    "Avalonia.X11.dll",
    "Tmds.DBus.Protocol.dll",
}

FOREIGN_PACKAGES = {
    "Avalonia.FreeDesktop",
    "Avalonia.FreeDesktop.AtSpi",
    "Avalonia.Native",
    "Avalonia.X11",
    "Tmds.DBus.Protocol",
}


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools prune-editor-windows-publish")
    parser.add_argument("publish_folder", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    publishDirectory = parsed.publish_folder.resolve()
    prunePublish(publishDirectory, FOREIGN_PACKAGES, FOREIGN_ASSEMBLIES)
    return 0
