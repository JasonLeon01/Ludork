# Generated Metadata and Stubs

The native binding pipeline reads C++ declaration macros once to generate Lua glue, LuaLS declarations, Blueprint metadata, private conversion traits and enum modules.

> **Important:** Generated files are build artefacts. Edit the C++ declarations and rebuild rather than editing generated files by hand.

## Pipeline outputs

Each module produces five output categories that agree on canonical module paths, class names, function groups and parameter order (with metadata adding Blueprint execution and editor details):

1. Stable `<Module>.<NativeClass>.auto.cpp` registration units and one `<Module>.stub.auto.cpp` for the native Lua module.
2. `Scripts/stub/<Module>.d.lua` for Lua language tooling (covering the full scripting API, including `metadata = false` members).
3. `Scripts/<Module>_meta.lua` for the editor's Blueprint catalogue.
4. `<Module>.traits.auto.hpp` for the module's private conversion traits.
5. Pure `Scripts/Enums/<Module>/<Name>.lua` modules with inline annotations for bound enums and marked constant maps.

Class unit filenames derive from the qualified C++ type name independently of the Lua `name` option. `<Module>.stub.auto.cpp` registers the module, invokes class units in binding order and supplies the LuaLS stub writer. Generated names and metadata inclusion follow [Binding a Class](<Binding a Class.md>) and [Functions, Events and Execution](<Functions Events and Execution.md>).

Core bindgen and LuaSF preserve multiline `///` comments in `.d.lua` files and convert recognised Doxygen commands (such as `\param` and `\return`) to `@` tags.

Desktop builds finish native stubs by calling the exported `<Module>_write_stub` symbol through `Engine/Tools/NativeStubDump` without creating a Lua VM. The writer appends `copy` and `deepcopy` only when the compiled `StructTraits` confirms independent value semantics.

Module runtime dependencies are copied before loading, and the aggregate native build refreshes stubs even when a module did not relink (cross-builds keep their source stubs). Generated conversion traits are supplied only to generated binding units via their binding PCH, or via forced inclusion when PCH is disabled (see [Build the project](<Build and Module Layout.md#build-the-project>)).

## Defaults and types

| C++ value type | Blueprint metadata | LuaLS |
|---|---|---|
| Named bound type | Full module/type reference | Qualified type name |
| Bound native or LuaSF enum | `{ enum = "Enums.<Module>.<Name>" }` | Existing qualified enum type |
| `std::vector<T>` or `std::array<T, N>` | `{ list = T }` | Array of `T` |
| String-keyed map | `{ dict = T }` | String-keyed dictionary of `T` |
| `std::pair<T1, T2>` or `std::tuple<T...>` | `{ tuple = { T1, T2, ... } }` | Fixed numeric fields (for example, `{ [1]: integer, [2]: string }`) |
| `std::variant<T...>` | `{ union = { T1, T2, ... } }` | Union of the alternatives |
| `std::monostate` | `"nil"` | `nil` |
| `std::optional<T>` | `T` | `T` or `nil` |
| `pure_data` type | `any` | Recursive `<Module>.<Name>Value` alias |
| `std::function` or `StrictFunction` | `function` | Declared callback signature |

Mappings apply recursively. The explicit metadata type `Pair` selects a two-component numeric editor control, distinct from a heterogeneous C++ pair or tuple. See [Lua value conversion](<Runtime Value Boundaries.md#lua-value-conversion>) for runtime conversion rules and [Declare strict callbacks](<Functions Events and Execution.md#declare-strict-callbacks>) for callback result validation.

- **Default values:** Bindgen derives defaults from boolean, numeric, string and empty-table member initialisers where supported; complex defaults use the brace syntax in [Declare properties](<Binding a Class.md#declare-properties>). A variant `{}` initialiser selects its first alternative; specify the branch explicitly when ambiguous. Unsupported explicit union defaults fail generation.
- **Metadata `self` receiver:** Ordinary non-static methods receive a metadata-only first `self` parameter typed as the owning module/class with `default[1] = "self"`. Static functions and registered event entries omit it. C++ declarations, native Lua registration and LuaLS colon-call signatures keep their original business arguments.
- **Parameter and `nil` alignment:** Annotation `defaults` and `parameter_types` describe the C++ business parameters; Bindgen shifts their metadata indices after `self` while preserving values. Explicit `nil` defaults emit named `defaultUnset` entries so editor inputs remain unset. See [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md>) for the schema, tagged union literals and editor behaviour.
- **Singleton function groups:** A `BIND_CLASS` singleton also generates metadata for its bound function group. Those wrappers already target the singleton, so their metadata parameters omit `self`, while class methods keep their typed receiver (`Engine.Input` and `Engine.Service` follow this split).
- **LuaSF callback codecs:** LuaSF supplies `LUASF_CALLBACK_CODECS_FILE` for callback aliases with special calling conventions. Bindgen matches codecs by semantic alias before canonical type expansion and preserves them through nested containers. Missing or incompatible manifests and mismatched canonical types fail generation or compilation.

## Generated enum modules

A desktop native build invokes `<Module>_write_enum_catalogue` via `NativeStubDump` (without creating a Lua VM) to read actual C++ enumerator integers and scalar values from `const` maps marked `BIND_MODULE_PROPERTY(enum = true)`, writing a catalogue under `Intermediate`. `ludork-codegen enum-modules` then emits one annotated local constant table and return statement per enum under `Enums.Engine`, `Enums.GlobalCore` or the owning module's namespace, matching the native table exports.

- **LuaSF enums:** Modules under `Enums.sf` are generated from `sfml_api.json`. Nested names map to subdirectories (such as `Enums/sf/Keyboard/Scan.lua`), and the `Scancode` alias resolves to `Enums.sf.Keyboard.Scan`. Generating them does not add Blueprint nodes.
- **Annotation format:** Enum modules carry their own annotations with no mirrored stub files. Native enums define a `---@class` table for `Enums.<Module>.<Name>`, and each scalar constant carries its native API type (for example, `A = (0 --[[@as sf.Keyboard.Key]])`), while the native enum definitions live in `Engine.d.lua`, `LuaSF.d.lua` and other owning stubs. Primitive constant maps share the `---@class` table identity with inferred scalar member types. Each module stores every constant once and returns the local table, without `---@meta` or runtime class construction.
- **Ownership and packaging:** The generator writes only changed content, deletes stale outputs and obsolete enum stubs only when they carry its generation marker, and rejects collisions with handwritten files. Editor templates and native caches ship enum source modules without an enum stub tree (keeping other native API stubs); game packages include enum source modules and exclude the stub tree.
- **Cross-build requirement:** Cross-builds and builds with static Lua modules require enum source modules generated by a prior desktop native build. Missing files fail preflight with their paths rather than guessing constant values; rebuild on desktop after changing native enums or marked maps.
- **Project-owned enums:** General Data, animation and particle documents generate project enums. Native metadata can reference them via `metadata_type = { enum = "Enums.Particle", valueType = "string" }` without changing the C++ string parameter or Lua API value. See [General Data and Text Config](<../Editor User Guide/General Data and Text Config.md>).

## Regeneration

Run the normal CMake build to add, update or remove generated class units and module outputs. Bindgen scans public headers in case-sensitive path order using `/` separators on every platform and writes UTF-8 without BOM using LF line endings.

- **Atomic writes and markers:** Bindgen compares bytes before writing, preserving modification times on unchanged files (regenerating a CRLF file normalises it to LF). Changed files are written beside their destination and replaced atomically, preserving file permissions and leaving the previous file intact on failure. Generator-marked outputs never overwrite handwritten files, and obsolete binding units are removed only after verifying their marker.
- **Completion stamp:** The bindings completion stamp updates after every successful generation, even when all output bytes are unchanged, signalling to CMake that inputs are processed without churning generated timestamps. Changes to included headers can still recompile dependent class units.
- **Stale outputs:** If generated output appears stale, verify that the annotated header belongs to the module's configured public-header set, inspect generator diagnostics and rebuild.

## Generated UI Views

UI Views and window declarations use a separate generator, `ludork-assets ui-assets generate <project-root>`. See [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) for generated file ownership and export steps.
