# Generated Metadata and Stubs

Generated files are build artefacts. Do not edit them by hand.

The native binding pipeline uses one declaration to produce five outputs:

1. stable `<Module>.<NativeClass>.auto.cpp` registration units and exactly one `<Module>.stub.auto.cpp` used by the native Lua module;
2. `Scripts/stub/<Module>.d.lua` used by Lua language tooling;
3. `Scripts/<Module>_meta.lua` used by the editor;
4. `<Module>.traits.auto.hpp` containing the module's private conversion traits;
5. pure `Scripts/Enums/<Module>/<Name>.lua` modules and matching `Scripts/stub/Enums/<Module>/<Name>.d.lua` declarations for bound enums and marked constant maps.

These outputs must agree on the canonical module path, class name, function group and parameter order. Metadata additionally carries Blueprint execution and editor information.

Class filenames derive from the qualified C++ type name, independently of the Lua `name` option. `<Module>.stub.auto.cpp` registers the module, calls class units in binding order and provides the LuaLS stub writer.

Generated names and metadata inclusion follow the declarations described in [Binding a Class](<Binding a Class.md>) and [Functions, Events and Execution](<Functions Events and Execution.md>). LuaLS stubs cover the scripting API, including members excluded from Blueprint metadata.

Core bindgen and LuaSF preserve multiline `///` documentation in `.d.lua` files and convert recognised Doxygen commands, such as `\param` and `\return`, to the corresponding `@` form.

Desktop builds finish native stubs by invoking the exported `<Module>_write_stub` function with `Engine/Tools/NativeStubDump`. The writer appends `copy` and `deepcopy` only for types whose compiled `StructTraits` confirms independent value semantics. It creates no Lua VM. Module runtime dependencies are copied before loading, and the aggregate native build refreshes these stubs even when the module did not relink. Cross-builds retain their source stubs. Generated conversion traits are force-included only in generated binding units.

## Defaults and types

| C++ value type | Blueprint metadata | LuaLS |
|---|---|---|
| Named bound type | Full module/type reference | Qualified type name |
| Bound native or LuaSF enum | `{ enum = "Enums.<Module>.<Name>" }` | Existing qualified enum type |
| `std::vector<T>` or `std::array<T, N>` | `{ list = T }` | Array of `T` |
| String-keyed map | `{ dict = T }` | String-keyed dictionary of `T` |
| `std::pair<T1, T2>` or `std::tuple<T...>` | `{ tuple = { T1, T2, ... } }` | Fixed numeric fields, such as `{ [1]: integer, [2]: string }` |
| `std::variant<T...>` | `{ union = { T1, T2, ... } }` | Union of the alternatives |
| `std::monostate` | `"nil"` | `nil` |
| `std::optional<T>` | `T` | `T` or nil |
| `pure_data` type | `any` | Recursive `<Module>.<Name>Value` alias |
| `std::function` or `StrictFunction` | `function` | Declared callback signature |

Mappings apply recursively. The explicit metadata type `Pair` selects a two-component numeric control, which differs from a heterogeneous C++ pair or tuple. See [Runtime Value Boundaries](<Runtime Value Boundaries.md#lua-value-conversion>) for Lua conversion rules, and [Functions, Events and Execution](<Functions Events and Execution.md#declare-strict-callbacks>) for strict callback result validation.

Bindgen derives defaults from boolean, numeric, string or empty-table member initialisers where supported. Complex defaults use the annotation brace syntax in [Binding a Class](<Binding a Class.md#declare-properties>). A variant initialiser `{}` selects its first declared alternative. Specify the branch explicitly when a default is ambiguous. Unsupported explicit union defaults fail generation.

Ordinary non-static methods generate a first `self` parameter typed as the owning module/class, with `default[1] = "self"`. Static functions and registered event entries have no receiver parameter. This receiver is metadata-only: C++ declarations, native Lua registration and LuaLS colon-call signatures keep their original business arguments.

Annotation defaults and parameter type overrides describe the original C++ arguments. Bindgen shifts their metadata positions after the receiver while preserving their values. Explicit nil defaults produce named `defaultUnset` entries, keeping those editor inputs unset. The [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md>) page defines the generated schema, tagged union literals and editor behaviour.

LuaSF supplies `LUASF_CALLBACK_CODECS_FILE` for callback aliases with special calling conventions. Bindgen selects the codec by semantic alias before canonical type expansion and preserves it through nested containers. Missing or incompatible manifests and mismatched canonical types fail generation or compilation.

A `BIND_CLASS` singleton declaration also generates metadata for its bound function group. Those wrappers already select the singleton, so their parameters contain no `self`; the class methods retain their typed receiver. `Engine.Input` and `Engine.Service` are the corresponding function-group and class surfaces.

## Generated enum modules

A desktop native build uses `NativeStubDump` to invoke `<Module>_write_enum_catalogue`. The compiled writer obtains integer values from the actual C++ enumerators and scalar values from const maps marked `BIND_MODULE_PROPERTY(enum = true)`. It writes a catalogue under `Intermediate` without creating a Lua VM. `ScriptTools enum-modules` then produces one directly returned constant table per enum under `Enums.Engine`, `Enums.GlobalCore` or the owning native module's namespace. The existing native table exports continue to use the same source values.

LuaSF enum modules under `Enums.sf` come from `sfml_api.json`. Nested names become directories, such as `Enums/sf/Keyboard/Scan.lua`; the `Scancode` alias resolves to the canonical `Enums.sf.Keyboard.Scan` schema. Their generation does not expose additional Blueprint nodes. Stubs declare the require path with `---@meta Enums.<Module>.<Name>` and retain the appropriate LuaLS value types.

The generator writes changed content only and removes stale outputs only when they carry its generation marker. It rejects a collision with a handwritten file. Editor templates and native caches include both enum source modules and stubs. Game packages keep the enum source modules and exclude the stub tree.

Cross-builds and builds with static Lua modules require the enum source modules and mirrored stubs from a prior desktop native build. Missing generated files fail the preflight with their paths; the build does not guess C++ constant values. Rebuild on desktop after changing native enum declarations or marked maps before preparing those builds.

## Regeneration

Run the normal CMake build to add, update or remove generated class units and module outputs. Bindgen writes only changed content, preserving unchanged files' names and timestamps. If output appears stale, confirm that the annotated header belongs to the module's configured public-header set, then inspect the generator diagnostics and rebuild.

Changes to included headers may still recompile dependent class units even when their generated content is unchanged.

## Generated UI Views

UI Views and window declarations use a separate generator, `ScriptTools ui-assets generate <project-root>`. Generated file ownership is described in the [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>). Commands and checks are in [Declarative UI Validation and Packaging](<../Lua and Blueprint Scripting/Declarative UI/Validation and Packaging.md>).

## Related pages

- [Binding a Class](<Binding a Class.md>)
- [C++ Binding Troubleshooting](<C++ Binding Troubleshooting.md>)
- [Functions, Events and Execution](<Functions Events and Execution.md>)
