from __future__ import annotations

import tempfile
from pathlib import Path
import unittest

from ScriptTools.core_bindgen.annotations import parse_header
from ScriptTools.core_bindgen.bindings import generate_bindings
from ScriptTools.core_bindgen.context import GeneratorContext
from ScriptTools.core_bindgen.metadata import generate_metadata, metadata_type_names
from ScriptTools.core_bindgen.stub import generate_stub


class CoreBindgenTests(unittest.TestCase):
    def parse_source(self, directory: Path, source: str) -> None:
        header = directory / "Legacy.hpp"
        header.write_text(source, encoding="utf-8")
        parse_header(GeneratorContext(), header)

    def test_runtime_path_options_are_retired_with_locations(self) -> None:
        cases = [
            (
                "CLASS",
                "alias",
                'BIND_CLASS(alias = "Legacy.Widget") class Widget {};',
            ),
            (
                "CLASS",
                "aliases",
                'BIND_CLASS(aliases = "Legacy.Widget") class Widget {};',
            ),
            (
                "CLASS",
                "scope",
                'BIND_CLASS(scope = "Legacy") class Widget {};',
            ),
            (
                "CLASS",
                "scopes",
                'BIND_CLASS(scopes = "Legacy") class Widget {};',
            ),
            (
                "FUNCTION",
                "alias",
                'BIND_FUNCTION(alias = "Legacy.Ping") int Ping();',
            ),
            (
                "FUNCTION",
                "aliases",
                'BIND_FUNCTION(aliases = "Legacy.Ping") int Ping();',
            ),
            (
                "FUNCTION",
                "scope",
                'BIND_FUNCTION(scope = "Legacy") int Ping();',
            ),
            (
                "FUNCTION",
                "scopes",
                'BIND_FUNCTION(scopes = "Legacy") int Ping();',
            ),
            (
                "FUNCTION",
                "global",
                'BIND_FUNCTION(global = "Ping") int Ping();',
            ),
            (
                "FUNCTION",
                "globals",
                'BIND_FUNCTION(globals = "Ping") int Ping();',
            ),
            (
                "MODULE_PROPERTY",
                "alias",
                'BIND_MODULE_PROPERTY(alias = "Legacy.Value") int Value;',
            ),
            (
                "MODULE_PROPERTY",
                "aliases",
                'BIND_MODULE_PROPERTY(aliases = "Legacy.Value") int Value;',
            ),
            (
                "MODULE_PROPERTY",
                "scope",
                'BIND_MODULE_PROPERTY(scope = "Legacy") int Value;',
            ),
            (
                "MODULE_PROPERTY",
                "scopes",
                'BIND_MODULE_PROPERTY(scopes = "Legacy") int Value;',
            ),
        ]
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            header = directory / "Legacy.hpp"
            for macro_name, option_name, declaration in cases:
                with self.subTest(macro=macro_name, option=option_name):
                    expected = (
                        f"{header}:2: BIND_{macro_name} options {option_name} "
                        "are no longer supported"
                    )
                    with self.assertRaisesRegex(ValueError, expected):
                        self.parse_source(directory, "#pragma once\n" + declaration)

    def test_bind_lua_alias_is_retired_with_location(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            header = directory / "Legacy.hpp"
            with self.assertRaisesRegex(
                ValueError,
                f"{header}:2: BIND_LUA_ALIAS is no longer supported",
            ):
                self.parse_source(
                    directory,
                    '#pragma once\nBIND_LUA_ALIAS(path = "Old", target = "New")',
                )

    def test_root_names_reject_nested_paths_with_locations(self) -> None:
        cases = [
            (
                "CLASS",
                'BIND_CLASS(name = "Legacy.Widget") class Widget {};',
            ),
            (
                "FUNCTION",
                'BIND_FUNCTION(name = "Legacy.Ping") int Ping();',
            ),
            (
                "MODULE_PROPERTY",
                'BIND_MODULE_PROPERTY(name = "Legacy.Value") int Value;',
            ),
        ]
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            header = directory / "Legacy.hpp"
            for macro_name, declaration in cases:
                with self.subTest(macro=macro_name):
                    expected = (
                        f"{header}:2: BIND_{macro_name} name must be one Lua "
                        "identifier"
                    )
                    with self.assertRaisesRegex(ValueError, expected):
                        self.parse_source(directory, "#pragma once\n" + declaration)

    def test_root_only_generation_preserves_non_alias_features(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            root_header = directory / "Root.hpp"
            root_header.write_text(
                """#pragma once
BIND_CLASS(
    name = "Widget",
    cast_bases = "ExternalWidget",
    module = "Input",
    singleton = "getInput")
class NativeWidget {
public:
    BIND_METHOD()
    int Read(int value) const;
};

BIND_FUNCTION(name = "Ping")
int NativePing(int value);

BIND_MODULE_PROPERTY(name = "Version")
inline int NativeVersion = 1;

BIND_INJECT(global = "RUNTIME_VALUE")
void InjectRuntimeValue(int value);

BIND_LUA_REVERSE(path = "Reverse.Values", source = "Values")
BIND_LUA_HELPER(path = "Helpers.cast", kind = "cast")
""",
                encoding="utf-8",
            )
            group_header = directory / "Group.hpp"
            group_header.write_text(
                """#pragma once
BIND_FUNCTION_GROUP(name = "Tools")
BIND_FUNCTION()
int Run(int value);
""",
                encoding="utf-8",
            )
            context = GeneratorContext()
            types = []
            functions = []
            for header in (root_header, group_header):
                parsed_types, parsed_functions = parse_header(context, header)
                types.extend(parsed_types)
                functions.extend(parsed_functions)
            context.exposed_type_names = {"NativeWidget": "Widget"}
            context.type_modules = {"NativeWidget": "Test"}

            stub = generate_stub(context, "Test", types, functions)
            metadata = generate_metadata(
                context,
                "Test",
                types,
                functions,
                context.type_modules,
                metadata_type_names(context, types),
            )
            bindings = generate_bindings(
                context,
                directory,
                directory,
                "Test",
                types,
                functions,
                stub,
                metadata,
                types,
                [],
            )

            self.assertIn("---@alias ExternalWidget Test.Widget", stub)
            self.assertIn("    Widget = {", metadata)
            self.assertNotIn("Legacy", metadata)
            self.assertIn("function Test.Ping(value) end", stub)
            self.assertIn("function Test.Tools.Run(value) end", stub)
            self.assertNotIn("function Test.Run(value) end", stub)
            self.assertIn("Test.Version = nil", stub)
            self.assertIn("function Test.Input.Read(value) end", stub)
            self.assertIn("function Test.Helpers.cast(targetType, value) end", stub)
            self.assertIn(
                'root.new_usertype<NativeWidget>("Widget"',
                bindings,
            )
            self.assertIn('root.set_function("Ping"', bindings)
            self.assertRegex(
                bindings,
                r'bindingFunctionGroup\d+_0\.set_function\("Run"',
            )
            self.assertIn('lua.globals().raw_get<sol::object>("RUNTIME_VALUE")', bindings)
            self.assertIn("bindingReversePathScope", bindings)
            self.assertIn("bindingLuaHelperPathScope", bindings)
            self.assertIn("NativeWidgetSingletonModule", bindings)
            self.assertNotIn("bindingAlias", bindings)
            self.assertNotIn("bindingGlobal", bindings)
            self.assertNotIn("bindingModulePropertyScope", bindings)


if __name__ == "__main__":
    unittest.main()
