# Macro Reference

Select the declaration marker for a supported binding or native UI adapter. Detailed option syntax is on the task and advanced-pattern pages.

| Macro | Purpose |
|---|---|
| `BIND_CLASS` | Declare a bound class and its unique module-root name. `callbacks = true` selects this class's non-static virtual bound methods for C++-to-Lua dispatch |
| `BIND_ENUM` | Expose a scoped C++ enum as one module-root Lua table and LuaLS type |
| `BIND_FUNCTION` | Bind a free function. `outpins(...)` and `latent(...)` are inline options |
| `BIND_FUNCTION_GROUP` | Put all bound free functions in a header into one table |
| `BIND_METHOD` | Bind a method or getter-backed property. Methods enter metadata unless `metadata = false`. `callback = false` excludes a virtual method from its class's automatic callback selection |
| `BIND_PROPERTY` | Bind an instance data property |
| `BIND_INIT` | Bind a constructor |
| `BIND_REGISTER_EVENT` | Register a Blueprint event entry |
| `BIND_INJECT` | Inject declaration-driven binding code at a supported point |
| `BIND_MODULE_PROPERTY` | Expose a module-level property |
| `BIND_CLASS_PROPERTY` | Expose a class-level property |
| `BIND_LUA_REVERSE` | Describe reverse Lua/C++ adaptation |
| `BIND_LUA_HELPER` | Declare a reusable Lua binding helper |
| `BIND_MODULE_INIT` | Add module initialisation behaviour |

Leave unexposed declarations unmarked. Class conversion options are documented in [Binding a Class](<Binding a Class.md#value-conversion-contracts>). Class decorators such as `invalid_vars(...)` and `rect_range_vars(...)` belong inside `BIND_CLASS`. Loop protocols use inline `loop_node(...)`, while `BIND_REGISTER_EVENT()` remains standalone.

Variadic `BIND_*` markers compile to nothing. Bindgen reads raw tokens such as `outpins(...)`, `latent(...)`, `nil` and `function`, which must not be quoted for C++ parsing. Use lowercase `nil`, `true` and `false`. `defaults` and `parameter_types` use brace lists, while structured `meta(...)` and property `default` data use nestable braces rather than quoted JSON.

Base inference and `cast_bases` follow [Binding a Class](<Binding a Class.md#declare-the-class>).

`BIND_ENUM` and `BIND_MODULE_PROPERTY(enum = true)` generate enum modules. Declaration examples, supported map values and `name` behaviour are covered in [Bind scoped enums](<Binding a Class.md#bind-scoped-enums>); output details are in [Generated enum modules](<Generated Metadata and Stubs.md#generated-enum-modules>).

`metadata_type`, `metadata_parameters` and `metadata_returns` override Blueprint schemas without changing native bindings or LuaLS types. See [Metadata-only signature schemas](<Functions Events and Execution.md#metadata-only-signature-schemas>); property examples are in [Bind scoped enums](<Binding a Class.md#bind-scoped-enums>).

Runtime paths follow [Binding a Class](<Binding a Class.md#declare-the-class>) and [Assign function groups](<Functions Events and Execution.md#assign-function-groups>). Injection options, including `global` and `variadic`, are covered in [Advanced Binding Patterns](<Advanced Binding Patterns.md#initialisation-injection-and-value-traits>).

For `nonnull_return = true`, see [Functions, Events and Execution](<Functions Events and Execution.md#declare-execution-behaviour>).

## Native cast declarations

In a class's public section, use `LUDORK_CAST_ROOT(Type)` for a query root or `LUDORK_CAST_DERIVED(Type, Bases...)` for a derived type, listing its participating direct bases. Each dynamic cast target and each polymorphic bound class must declare its own macro; an inherited declaration does not register a new type. Non-polymorphic value types need no cast declaration. Generated Lua adapters declare the protocol automatically; their public host classes must declare it themselves.

The macros generate virtual subobject queries and `std::string_view ludorkDynamicTypeKey() const noexcept`. They do not change Lua names or binding bases. Header requirements and pointer semantics are in [Native pointer casts](<Build and Module Layout.md#native-pointer-casts>); Lua wrapping follows [Native object wrappers](<Runtime Value Boundaries.md#native-object-wrappers>).

## Limitations

The table identifies supported markers, not every permitted option combination. `Pure`, execution protocols and property declarations remain subject to the validation rules on their dedicated pages.

## Related pages

- [Native UI Adapters](<Native UI Adapters.md>)
