# Binding a Class

Annotate a C++20 class with `BIND_*` macros to expose it consistently to native Lua, LuaLS declarations and the Blueprint metadata catalogue.

## Declare the class

```cpp
#pragma once
#include <CoreMinimal.hpp>

BIND_CLASS()
class Counter {
public:
  BIND_INIT()
  explicit Counter(int initial = 0);

  BIND_PROPERTY()
  int value = 0;

  BIND_METHOD(Pure = true)
  int getValue() const;

  BIND_METHOD()
  void add(int amount);
};
```

Header dependencies and runtime value types follow [Core headers and Runtime boundary](<Build and Module Layout.md#core-headers-and-runtime-boundary>). Place the declaration in the module's `include/Counter.hpp` and member definitions in `src/Counter.cpp`:

```cpp
#include "Counter.hpp"

Counter::Counter(int initial) : value(initial) {}

int Counter::getValue() const {
  return value;
}

void Counter::add(int amount) {
  value += amount;
}
```

CMake discovers `include/**/*.hpp` and `src/**/*.cpp`. Rebuild the module to regenerate its bindings. For a `Counter` class in `Engine`, Lua code calls:

```lua
local Engine = require("Engine")
local Logging = require("Global.Utils.Logging")

local counter = Engine.Counter.new(4)
counter:add(3)
Logging.info(counter:getValue())
```

### Naming, callbacks, inheritance and metadata visibility

- **Module-root visibility:** The module root is the only public runtime location for the class. Use `name = "..."` on `BIND_CLASS` to choose its Lua root identifier, and `BIND_METHOD(name = "...")` to select a Lua method name (including on `metadata = false` methods).
- **Virtual callbacks:** `BIND_CLASS(callbacks = true)` dispatches this class's non-static virtual bound methods through Lua. This opt-in is not inherited, although an override of a method already selected by a base class remains active. Use `BIND_METHOD(callback = false)` to exclude one method on the current class without removing an inherited callback. Selection is independent of metadata and events; string lists and non-virtual constructor callbacks are unsupported.
- **Conversion bases:** Bindgen infers conversion bases from the C++ inheritance list. Pass `cast_bases = {"BaseA", "BaseB"}` as an ordered list of non-empty type names (never a comma-separated string) to add ancestors or supply bases when `bind_bases = false`.
- **Registered bases and `runtime_bases`:** A conversion base with a registered Lua type also contributes its native methods and properties, including on userdata returned directly from C++. Bindgen emits `BindBase` for registered bases and `BindCast` for interfaces without a Lua type. `bind_bases = false` disables inference from declared bases without hiding members of explicitly supplied registered bases, while `runtime_bases` controls the separate Ludork class composition hierarchy.
- **Metadata exclusion:** Every logical public `BIND_METHOD` enters editor metadata by default. Mark internal helpers or secondary adapter overloads with `metadata = false` so they stay available to native Lua and LuaLS without generating extra Blueprint nodes:

```cpp
BIND_METHOD(metadata = false)
void _updateCache();
```

Method names exposed in Blueprint metadata must be unique within a type.

## Declare properties

`BIND_PROPERTY()` exposes an instance data member. Safe boolean, numeric, string and `{}` initialisers become metadata defaults automatically. Use annotation-native pure data for explicit defaults, and put editor semantics that cannot be inferred from the C++ type inside `meta(...)`:

```cpp
BIND_PROPERTY(
    default = {0.0, 0.0})
sf::Vector2f defaultOrigin{0.0f, 0.0f};
```

Positional entries form arrays and named entries form objects. Bindgen parses this brace syntax directly, so it does not need to be a valid C++ expression. The same singular `default` syntax applies to getter-backed properties declared with `BIND_METHOD(property = ...)`.

Structured pure-data values inside `meta(...)` use the same nestable brace syntax:

```cpp
BIND_PROPERTY(
    meta(Rely = {source = "autoSound", op = "!=", value = ""}))
float autoSoundInterval = 0.0f;

BIND_METHOD(defaults = {{}},
            meta(MoveRouteVars = {"route"}),
            outpins(default = nil))
void setRoute(
    const std::optional<std::vector<sf::Vector2i>>& route = {});
```

Use lowercase `true` and `false`. Brace syntax, protocol `nil` and quoted-JSON rejection are detailed in [Macro Reference](<Macro Reference.md>).

### Display names

Ordinary properties and methods do not declare `DisplayName` or `DisplayDesc`. The editor formats identifiers automatically (`defaultOrigin` displays as `Default Origin` while the C++ and Lua name stays `defaultOrigin`). A true operator alias may declare a literal display name such as `+`, but must not use `LOC(...)` or a default description.

### Getter-backed properties

A no-argument getter can expose a computed property:

```cpp
BIND_METHOD(property = "volume", setter = "setVolume")
float getVolume() const;

void setVolume(float value);
```

The getter return type becomes the property type unless `type` overrides it. This declaration exposes one property and does not also generate getter and setter nodes.

A computed property may also set `component = true`, preserving component metadata while routing assignments through the setter. `Actor.emitterComp` uses this form to release the previous effect immediately when replaced or assigned `nil`, including while its Actor is dormant.

## Review constructors and exclusions

- **Constructors and static properties:** Mark constructors with `BIND_INIT` (invoked as `.new(...)` in Lua), class-level values with `BIND_CLASS_PROPERTY` (C++ static storage) and module-level values with `BIND_MODULE_PROPERTY`. Leave declarations unmarked to exclude them from bindings.
- **Value copying:** A complete record marked `copyable = true` gains `copy()` and `deepcopy()` when every member has verified independent value semantics. Scalars, strings, containers and nested independent values qualify; pointers, Lua references and shared resources do not become deep-copyable from `copyable` alone. Complete native copies include private state only when an explicit complete type policy establishes that it is safe.

## Bind scoped enums

Use `BIND_ENUM` to share a fixed native value set with Lua:

```cpp
BIND_ENUM()
enum class WeatherType {
  NONE = 0,
  RAIN = 1,
};
```

Bindgen exposes `Module.WeatherType` and generates the matching `Enums.Module.WeatherType` module; `name` selects both names. Blueprint fields and signatures use `{ enum = "Enums.Module.WeatherType" }`, while Lua calls and saved Blueprint values carry the underlying integers. Do not duplicate enumerators as module properties. See [Generated enum modules](<Generated Metadata and Stubs.md#generated-enum-modules>) for annotations, stubs and cross-build requirements.

An existing `const` map with string keys and numeric, string or boolean values can also export a scalar enum without defining a C++ `enum`:

```cpp
BIND_MODULE_PROPERTY(enum = true)
extern const std::unordered_map<std::string, std::string> FocusDirection;
```

`Engine.FocusDirection` uses this form, whereas `Engine.Direction` is a bound native enum. Generated enum modules are owned by the build: edit the declaration or map source and rebuild.

Use `metadata_type` when a scalar property should display an enum selector in the editor without changing its C++ or LuaLS type:

```cpp
BIND_PROPERTY(metadata_type = {enum = "Enums.Engine.Direction"})
int direction = 0;
```

The selector saves the referenced constant's value. Parameter and return overrides follow [Metadata-only signature schemas](<Functions Events and Execution.md#metadata-only-signature-schemas>).

## Bind owner-specific nested types

A public nested `struct` or `enum` uses the same binding markers as a top-level declaration:

```cpp
class Owner {
public:
  BIND_CLASS()
  struct Data {
    BIND_PROPERTY()
    int value = 0;
  };
};
```

C++ names this type `Owner::Data`, while Lua exposes it as `Module.Data` (or under `name`). The enclosing owner need not be bound, but the nested type must be publicly accessible through every enclosing class. When using a public forward declaration with an out-of-line qualified definition, place the binding marker on the complete definition. Types sharing a short C++ name within the same module require distinct Lua `name` options.

## Value conversion contracts

Use `BIND_CLASS(table_init = true)` to initialise a fixed record from a Lua table. Supplied non-nil writable properties use their declared codecs, missing or `nil` fields keep their C++ defaults, and existing native instances are accepted directly. Add `strict_fields = true` to reject unknown fields and non-string keys:

```cpp
BIND_CLASS(table_init = true, strict_fields = true)
struct Limits {
  BIND_PROPERTY()
  std::optional<double> minimum;

  BIND_PROPERTY()
  std::optional<double> maximum;
};
```

`strict_fields` does not turn optional fields into required fields.

Use `BIND_CLASS(pure_data = true)` for recursive value containers such as `RuntimeData`; see [Lua value conversion](<Runtime Value Boundaries.md#lua-value-conversion>) for accepted values and variant conversion rules.

`pure_data`, `dynamic_value`, `opaque_identity` and `table_init` are mutually exclusive conversion modes. See [Defaults and types](<Generated Metadata and Stubs.md#defaults-and-types>) for their generated type representations.

> **Note:** Bindgen does not infer project-specific conversion rules or resolve incomplete cross-module types.
