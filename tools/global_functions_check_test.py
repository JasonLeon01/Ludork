from __future__ import annotations

from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from ScriptTools.global_functions_check import (
    LUA_MODULES,
    NATIVE_HEADERS,
    _native_exports,
    _staged_snapshot,
    check,
)


class GlobalFunctionsCheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.header = self.root / NATIVE_HEADERS / "UI.hpp"
        self.header.parent.mkdir(parents=True)
        self.header.write_text(
            '#pragma once\nBIND_FUNCTION_GROUP(name = "UI")\n'
            'BIND_FUNCTION(name = "DisplayName")\nint nativeName();\n',
            encoding="utf-8",
        )

    def module(self, name: str, content: str) -> None:
        path = self.root / LUA_MODULES / f"{name}.lua"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def test_native_explicit_name_is_used(self) -> None:
        exports = _native_exports(self.root)
        self.assertIn(("UI",), exports)
        self.assertIn(("UI", "DisplayName"), exports)
        self.assertNotIn(("UI", "nativeName"), exports)

    def test_group_collision_is_rejected(self) -> None:
        self.module("UI", "local UI = {}\nfunction UI.Other() end\nreturn UI\n")
        messages = check(self.root)
        self.assertTrue(any("GlobalFunctions.UI conflicts" in message for message in messages))

    def test_native_member_collision_is_rejected(self) -> None:
        self.module("UI", "local UI = {}\nfunction UI.DisplayName() end\nreturn UI\n")
        messages = check(self.root)
        self.assertTrue(any("GlobalFunctions.UI.DisplayName conflicts" in message for message in messages))

    def test_same_member_name_in_another_group_is_allowed(self) -> None:
        self.module("Math", "local Math = {}\nfunction Math.DisplayName() end\nreturn Math\n")
        self.assertEqual(check(self.root), [])

    def test_module_scope_require_is_rejected_but_function_require_is_allowed(self) -> None:
        self.module(
            "Math",
            'local value = require("Engine")\n'
            'if true then local other = require("GlobalCore") end\n'
            'local Math = {}\n'
            'function Math.Use()\n    if true then require("Source.Data") end\nend\n'
            'return Math\n',
        )
        messages = check(self.root)
        self.assertEqual(sum("require at module scope" in message for message in messages), 2)
        self.module(
            "Math",
            'local Math = {}\nfunction Math.Use()\n'
            '    local value = require("Engine")\n    return value\nend\nreturn Math\n',
        )
        self.assertEqual(check(self.root), [])

    def test_staged_snapshot_uses_index_contents(self) -> None:
        staged_header = NATIVE_HEADERS / "UI.hpp"
        staged_module = LUA_MODULES / "Math.lua"
        self.module("Math", "local Math = {}\nreturn Math\n")
        staged_content = b'local Engine = require("Engine")\nlocal Math = {}\nreturn Math\n'

        def git_output(command: list[str], **_kwargs: object) -> bytes:
            if command[1] == "ls-files":
                return (staged_header.as_posix() + "\0" + staged_module.as_posix() + "\0").encode()
            if command[-1] == ":" + staged_header.as_posix():
                return self.header.read_bytes()
            if command[-1] == ":" + staged_module.as_posix():
                return staged_content
            raise AssertionError(command)

        with patch("ScriptTools.global_functions_check.subprocess.check_output", side_effect=git_output):
            _staged_snapshot(self.root, self.root / "snapshot")
        self.assertEqual(check(self.root), [])
        self.assertEqual((self.root / "snapshot" / staged_module).read_bytes(), staged_content)
        self.assertTrue(any("require at module scope" in message for message in check(self.root / "snapshot")))


if __name__ == "__main__":
    unittest.main()
