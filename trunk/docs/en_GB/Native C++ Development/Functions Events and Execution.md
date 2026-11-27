# Functions, Events and Execution

Expose C++ class methods, grouped free functions, events and Blueprint execution protocols from one declaration source inside a bound class or public free-function header.

## Bind methods and free functions

`BIND_METHOD` exposes a class method to native Lua and LuaLS, with logical public methods entering Blueprint metadata by default. `BIND_FUNCTION` exposes a free function under its established metadata-selection rules. Declare `Pure = true` only when the Blueprint node has no execution input or output; a non-void return type does not imply purity.

```cpp
BIND_FUNCTION(name = "clampValue", Pure = true)
float clampValue(float value, float minimum, float maximum);
```

- **Module-root vs grouped paths:** An ungrouped free function is exposed only at the module root (for example, `Engine.clampValue`), following the `name` rule in [Declare the class](<Binding a Class.md#declare-the-class>). Use `BIND_FUNCTION_GROUP` when a group table is the canonical owner.
- **Return pins and `self` receiver:** Non-void functions generate one metadata output named `return`, while `void` functions generate an empty return table. Ordinary instance methods receive a visible, typed first `self` input defaulting to the graph owner; static methods, free functions and registered event entries do not. See [Defaults and types](<Generated Metadata and Stubs.md#defaults-and-types>) for parameter and default alignment.

## Metadata-only signature schemas

`metadata_parameters` maps business parameter names to Blueprint schemas, and `metadata_returns` maps return-pin names (a single ordinary result is named `return`). Neither option alters the C++ signature, LuaLS declaration or metadata visibility.

```cpp
BIND_METHOD(
    Pure = true,
    metadata_parameters = {direction = {enum = "Enums.Engine.Direction"}},
    metadata_returns = {return = {enum = "Enums.Engine.Direction"}})
int oppositeDirection(int direction) const;
```

Use annotation brace syntax, including the unquoted `return` token above — Bindgen emits the quoted Lua key automatically. Unknown pin names fail generation. Because instance `self` is generated separately, override names refer directly to business parameters without a receiver offset.

Property overrides use `metadata_type`. Recursive enum schemas compose with containers and unions as described in [Enum schemas](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#enum-schemas>).

## Assign function groups

Begin a header containing only free functions with `BIND_FUNCTION_GROUP`:

```cpp
BIND_FUNCTION_GROUP(name = "Manager")

BIND_FUNCTION(name = "playSE")
void playSE(const std::string &path);
```

Every bound function in that header registers only under the declared group table in native Lua, LuaLS stubs and Blueprint metadata. Ungrouped functions stay at the module root.

## Declare execution behaviour

A non-pure method uses ordinary single-flow execution by default and requires no execution annotation:

```cpp
BIND_METHOD()
void refresh();
```

### Branching execution (`outpins`)

When a result selects a named execution branch, list the branches inside an inline `outpins(...)` block. Bindgen preserves declaration order:

```cpp
BIND_METHOD(outpins(default = nil))
void setCollisionEnabled(bool enabled);

BIND_METHOD(outpins(success = true, fail = false))
bool tryMove();
```

### Latent operations (`latent`)

For latent work, list resume states inside `latent(...)` and return `std::shared_ptr<AsyncOperation>` from `<Runtime/Async/AsyncOperation.hpp>`:

```cpp
BIND_METHOD(nonnull_return = true, latent(TimeUp = true),
            defaults = {nil, nil, {}, false},
            parameter_types = {float, function, any[], bool})
std::shared_ptr<AsyncOperation> addTimer(
    float interval,
    RuntimeIdentityPtr task = {},
    RuntimeValue::Array params = {},
    bool blocking = false);
```

Construct native latent operations with `AsyncOperation::create(poll?)`, publish intermediate states with `emit`, and settle them with `complete` or `cancel`.

### Execution option rules

- **`defaults` vs `outpins`:** `defaults` specifies parameter default values (also supported on constructors and free functions) and is unrelated to `outpins(default = nil)`. Lowercase tokens and brace-enclosed option lists follow [Macro Reference](<Macro Reference.md>).
- **Mutual exclusions:** `Pure = true` cannot combine with any execution protocol. A getter-backed property cannot declare an execution protocol, and its singular top-level `default` sets the property default.
- **Loops and events:** Inline `loop_node(ForEach)` declares a loop node; standalone `BIND_REGISTER_EVENT()` registers an event entry (an event without explicit outputs receives the default branch value `nil`).
- **Inline metadata only:** Place display, editor and execution options inside `BIND_METHOD(...)` or the corresponding function/property annotation. Separate standalone metadata, purity, execution-split and latent markers are unsupported.
- **Non-null shared pointers:** `nonnull_return = true` declares that a method or free function returns a non-null `std::shared_ptr`. Bindgen strips the outer `nil` alternative from its LuaLS declaration and raises a binding error if native code returns null, without changing the C++ type or Blueprint execution pins.

> **Important:** Annotations describe an execution protocol but do not implement it. The C++ implementation and runtime scheduler must emit every declared branch, latent state or loop transition.

## Declare strict callbacks

Use `ludork::runtime::StrictFunction<Return(Arguments...)>` from `<Runtime/StrictFunction.hpp>` to enforce exact callback return counts: a non-void signature requires exactly one Lua result, and `void` requires none. Return values are converted and validated against the declared type.

```cpp
using Number = std::variant<std::int64_t, double>;
using Constraint = ludork::runtime::StrictFunction<Number(Number)>;

BIND_METHOD()
void setConstraint(Constraint constraint);
```

Validation also applies inside variants and containers. `nil` represents an empty callback. Lua-owned callbacks preserve their function identity and stay bound to their originating session, and Lua errors or conversion failures propagate to the caller. See [Lua references and session lifetime](<Runtime Value Boundaries.md#lua-references-and-session-lifetime>) for reference rules.
