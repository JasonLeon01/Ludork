# Macro Reference

Reference of the `BIND_*` and `LUDORK_CAST_*` declaration markers used across native C++ modules. Option syntax and workflow details are covered on the dedicated task pages linked below.

## Binding macros

| Macro | Purpose |
|---|---|
| `BIND_CLASS` | Declare a bound class and its unique module-root name. `callbacks = true` selects this class's non-static virtual bound methods for C++-to-Lua dispatch |
| `BIND_ENUM` | Expose a scoped C++ enum as one module-root Lua table and LuaLS type |
| `BIND_FUNCTION` | Bind a free function; `outpins(...)` and `latent(...)` are inline options |
| `BIND_FUNCTION_GROUP` | Place all bound free functions declared in a header into one table |
| `BIND_METHOD` | Bind a method or getter-backed property. Methods enter metadata unless `metadata = false`; `callback = false` excludes a virtual method from automatic callback selection |
| `BIND_PROPERTY` | Bind an instance data property |
| `BIND_INIT` | Bind a constructor |
| `BIND_REGISTER_EVENT` | Register a Blueprint event entry |
| `BIND_INJECT` | Inject declaration-driven binding code at a supported point |
| `BIND_MODULE_PROPERTY` | Expose a module-level property |
| `BIND_CLASS_PROPERTY` | Expose a class-level property; `readonly = true` blocks Lua writes, and `type` sets the generated Lua declaration type without altering C++ storage or runtime binding |
| `BIND_LUA_REVERSE` | Describe reverse Lua/C++ lookup adaptation |
| `BIND_LUA_HELPER` | Declare a reusable Lua binding helper |
| `BIND_MODULE_INIT` | Add module initialisation behaviour |

Leave unexposed declarations unmarked. Class decorators such as `invalid_vars(...)` and `rect_range_vars(...)` belong inside `BIND_CLASS`. Loop protocols use inline `loop_node(...)`, whereas `BIND_REGISTER_EVENT()` is standalone.

## Annotation syntax and topic links

Variadic `BIND_*` markers compile to nothing, and Bindgen parses their raw tokens directly:

- **Raw tokens and literals:** Do not quote keywords such as `outpins(...)`, `latent(...)`, `nil` or `function` to satisfy the C++ parser. Always write lowercase `nil`, `true` and `false`.
- **Lists and nested data:** `defaults` and `parameter_types` use brace lists. Structured `meta(...)` values and property `default` values use nestable brace syntax rather than quoted JSON.
- **String defaults and literal types:** Quote scalar string defaults (including `default = "None"`) to preserve the exact string value. A class property can declare a narrow Lua string literal type with `BIND_CLASS_PROPERTY(readonly = true, type = "'Add'")` while keeping `std::string` in C++.

Detailed rules for specific macro options live on their owning pages:

- **Class paths, bases and `cast_bases`:** [Declare the class](<Binding a Class.md#declare-the-class>)
- **Class conversion modes (`table_init`, `pure_data`, `dynamic_value`, `opaque_identity`):** [Value conversion contracts](<Binding a Class.md#value-conversion-contracts>)
- **Scoped enums and `BIND_MODULE_PROPERTY(enum = true)`:** [Bind scoped enums](<Binding a Class.md#bind-scoped-enums>) and [Generated enum modules](<Generated Metadata and Stubs.md#generated-enum-modules>)
- **Metadata schema overrides (`metadata_type`, `metadata_parameters`, `metadata_returns`):** [Metadata-only signature schemas](<Functions Events and Execution.md#metadata-only-signature-schemas>) and [Bind scoped enums](<Binding a Class.md#bind-scoped-enums>)
- **Function groups and `nonnull_return = true`:** [Assign function groups](<Functions Events and Execution.md#assign-function-groups>) and [Declare execution behaviour](<Functions Events and Execution.md#declare-execution-behaviour>)
- **Injection options (`global`, `variadic`):** [Initialisation, injection and value traits](<Advanced Binding Patterns.md#initialisation-injection-and-value-traits>)

## Native cast declarations

In a class's `public` section, declare `LUDORK_CAST_ROOT(Type)` for a query root or `LUDORK_CAST_DERIVED(Type, Bases...)` for a derived type, listing its participating direct bases:

- Every dynamic cast target and every polymorphic bound class must declare its own macro; an inherited declaration does not register a derived type.
- Non-polymorphic value types require no cast declaration.
- Generated Lua adapters emit the protocol automatically; their public host classes must declare it themselves.

The macros generate virtual subobject query methods and `std::string_view ludorkDynamicTypeKey() const noexcept`, without affecting Lua names or binding bases. See [Native pointer casts](<Build and Module Layout.md#native-pointer-casts>) for headers and pointer semantics, and [Native object wrappers](<Runtime Value Boundaries.md#native-object-wrappers>) for Lua wrapping behaviour.

> **Note:** The table above lists supported markers rather than every permitted option combination. `Pure`, execution protocols and property declarations are subject to the validation rules on their dedicated pages.
