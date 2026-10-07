# Functions, Events and Execution

## Goal

Expose methods, grouped free functions, events and Blueprint execution protocols from one declaration source.

## Prerequisites

- A bound class or public free-function header in the owning module.
- A defined Blueprint contract for purity, branch outputs, latent states or loop behaviour.

## Steps

### Bind methods and free functions

`BIND_METHOD` exposes a class method to native Lua and LuaLS, and logical public methods enter Blueprint metadata by default. `BIND_FUNCTION` exposes a free function without changing its established metadata-selection rules. Declare `Pure = true` only when the Blueprint node has no execution input or output. A non-void return does not imply purity.

```cpp
BIND_FUNCTION(name = "clampValue", Pure = true)
float clampValue(float value, float minimum, float maximum);
```

An ungrouped free function is exposed only at the module root, such as `Engine.clampValue`. The `name` option selects the Lua identifier for that root field. Use `BIND_FUNCTION_GROUP` when a group table is the function's canonical owner.

Non-void functions receive one metadata output named `return`. Void functions receive an empty return table. Ordinary instance methods also receive a typed first `self` input that defaults to the graph owner and remains visible. Static methods, free functions and event entries do not add it. See [Generated Metadata and Stubs](<Generated Metadata and Stubs.md#defaults-and-types>) for parameter/default alignment.

### Metadata-only signature schemas

`metadata_parameters` maps existing business parameter names to Blueprint schemas. `metadata_returns` does the same for existing return-pin names; a single ordinary result is named `return`. These options change neither the C++ signature nor its LuaLS declaration, and keep the function's existing metadata visibility.

```cpp
BIND_METHOD(
    Pure = true,
    metadata_parameters = {direction = {enum = "Enums.Engine.Direction"}},
    metadata_returns = {return = {enum = "Enums.Engine.Direction"}})
int oppositeDirection(int direction) const;
```

Use annotation brace syntax, including the unquoted `return` token above. Bindgen emits the required quoted Lua key. Unknown pin names fail generation. Instance `self` is still generated separately, so the override names refer to business parameters without a receiver offset. Property schemas use `metadata_type`; recursive enum schemas compose with containers and unions as described in [Metadata Schema and Decorators](<../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#enum-schemas>).

### Assign function groups

A header containing only free functions can begin with:

```cpp
BIND_FUNCTION_GROUP(name = "Manager")

BIND_FUNCTION(name = "playSE")
void playSE(const std::string &path);
```

Every bound function in that header is registered only in the group table. Registration, LuaLS stub and metadata use the same group. Ungrouped functions remain at the module root.

### Declare execution behaviour

A non-pure method uses the ordinary execution flow by default and needs no explicit execution annotation:

```cpp
BIND_METHOD()
void refresh();
```

When the result selects a named execution branch, place the branches in an inline `outpins(...)` block. Bindgen preserves declaration order:

```cpp
BIND_METHOD(outpins(default = nil))
void setCollisionEnabled(bool enabled);

BIND_METHOD(outpins(success = true, fail = false))
bool tryMove();
```

For latent work, place the resume states inside `latent(...)`:

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

`defaults` describes parameter defaults and is unrelated to `outpins(default = nil)`. The list form also applies to constructor and free-function `defaults`. Lowercase tokens and brace-enclosed option lists follow [Macro Reference](<Macro Reference.md>).

`Pure = true` cannot combine with an execution protocol. A getter-backed property cannot carry one, and its singular top-level `default` remains the property default. Inline `loop_node(ForEach)` declares a loop, while standalone `BIND_REGISTER_EVENT()` creates an event entry. An event without explicit outputs receives the default branch value `nil`.

Place display, editor and execution information in `BIND_METHOD(...)` or the corresponding function or property annotation. Independent metadata, purity, execution-split and latent markers are not supported.

`nonnull_return = true` declares that a method or free function returns one non-null `std::shared_ptr` result. Bindgen removes the outer `nil` alternative from its Lua declaration and raises a binding error if native code returns null. It does not change the C++ type or Blueprint execution pins.

Native latent implementations return `std::shared_ptr<AsyncOperation>` from `<Runtime/Async/AsyncOperation.hpp>`. Construct with `AsyncOperation::create(poll?)`, publish intermediate states with `emit`, and settle with `complete` or `cancel`.

### Declare strict callbacks

Use `ludork::runtime::StrictFunction<Return(Arguments...)>` from `<Runtime/StrictFunction.hpp>` to enforce the callback result count: a non-void signature requires exactly one Lua result, and `void` requires none. Results are converted and validated against the declared type.

```cpp
using Number = std::variant<std::int64_t, double>;
using Constraint = ludork::runtime::StrictFunction<Number(Number)>;

BIND_METHOD()
void setConstraint(Constraint constraint);
```

These checks also apply inside variants and containers. Nil represents an empty callback. Lua-owned callbacks keep their function identity and remain bound to their originating session. Lua errors and conversion failures propagate to the caller. See [Runtime Value Boundaries](<Runtime Value Boundaries.md#lua-references-and-session-lifetime>) for Lua reference and session lifetime rules.

## Limitations

Annotations describe an execution protocol but do not implement its runtime behaviour. The C++ body and runtime scheduler must emit every declared branch, latent state or loop transition.

## Related pages

- [Macro Reference](<Macro Reference.md>)
- [Advanced Binding Patterns](<Advanced Binding Patterns.md>)
- [Runtime Value Boundaries](<Runtime Value Boundaries.md>)
