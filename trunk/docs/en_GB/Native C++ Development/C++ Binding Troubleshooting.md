# C++ Binding Troubleshooting

Diagnose and resolve common Core bindgen, compilation and Lua runtime-path errors. Start with the first Bindgen or compiler diagnostic, fix declaration, generation and runtime-path issues in that order, and rebuild before chasing downstream symptoms.

## The declaration is not generated

Confirm that the `BIND_*` marker is on a `public` declaration parsed by the module, that the header belongs to the module's CMake binding input set, and that the annotation syntax matches [Macro Reference](<Macro Reference.md>).

## C++ compiles but Lua cannot find the member

Verify the generated registration unit, [module load order](<Build and Module Layout.md#load-the-runtime-modules>) and canonical Lua path: classes and ungrouped free functions register at the module root, while grouped functions register inside their [function group](<Functions Events and Execution.md#assign-function-groups>).

## LuaLS sees a member but Blueprint does not

Check whether the method is intentionally marked `metadata = false` (common for internal helpers and secondary adapter overloads). Otherwise, the generated root `*_meta.lua` includes every logical public `BIND_METHOD`. Inspect the Bindgen diagnostics rather than adding a handwritten duplicate.

## Bindgen reports a duplicate metadata method

Keep the primary Blueprint-facing overload in metadata and mark secondary conversion or adapter overloads `metadata = false`. Do not add overload-specific generator branches or allow one declaration to overwrite another silently.

## Type conversion fails

Use supported primitive, STL, SFML and declared Core types, and include complete cross-module declarations. Add reusable conversion support through generic adapters or traits — never branch inside Bindgen on a specific gameplay class or member name.

## Metadata default is wrong

Place `meta(...)` inside the property or method's own binding annotation and supply an annotation-native `default` (see [Macro Reference](<Macro Reference.md>) and [Generated Metadata and Stubs](<Generated Metadata and Stubs.md>)), then rebuild to regenerate outputs.

A top-level `default`, `success` or `fail` option on a method or free function fails validation and requires `outpins(...)`. See [Functions, Events and Execution](<Functions Events and Execution.md>) and [Macro Reference](<Macro Reference.md>) for the distinction between `outpins(...)` and `defaults`, and for lowercase token rules.
