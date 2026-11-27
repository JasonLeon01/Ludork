# Advanced Binding Patterns

Use these engine-infrastructure patterns when ordinary class, property, method and free-function bindings cannot represent a required contract.

## Class metadata decorators

Nest class decorators inside the owning class's `BIND_CLASS` annotation:

```cpp
BIND_CLASS(callbacks = true,
           invalid_vars(defaultRect, internalState),
           rect_range_vars(defaultRect = texturePath))
class Character : public Actor {
public:
  BIND_METHOD(metadata = false)
  virtual void _applyDirection(float x, float y);
};
```

- `invalid_vars(...)` hides fields in editor metadata, and `rect_range_vars(...)` links rectangle fields to resource or range fields. Both accept ordered identifiers and are valid only inside `BIND_CLASS`.
- Property-level rectangle semantics use `meta(RectRangeVars = "texturePath")` inside `BIND_PROPERTY`.
- `metadata = false` hides a method from Blueprint metadata without disabling virtual callbacks; use `callback = false` to opt out of callback dispatch. See [Binding a Class](<Binding a Class.md>) and [Declare properties](<Binding a Class.md#declare-properties>).

## Events, latent work and loops

```cpp
BIND_REGISTER_EVENT()
BIND_METHOD()
virtual void onCreate();

BIND_METHOD(loop_node(ForEach))
void forEach(const RuntimeValue::Array &values);
```

Event and loop annotations generate Blueprint execution metadata; the C++ or Lua implementation must carry out the declared protocol. See [Declare execution behaviour](<Functions Events and Execution.md#declare-execution-behaviour>) for latent operations, return constraints and defaults.

## Values, reverse maps and helpers

```cpp
BIND_MODULE_PROPERTY()
extern std::string LANGUAGE;

class FocusTransition {
public:
  BIND_CLASS_PROPERTY(readonly = true)
  static const std::string DIRECTIONAL;
};

BIND_LUA_REVERSE(path = "Input.KeyName", source = "sf.Keyboard.Key")
BIND_LUA_HELPER(kind = "cast", path = "Cast")
```

- **Module and class properties:** Module properties are exposed at the module root, and class properties on their owning class. Bindgen never duplicates an existing Lua value at a second runtime path.
- **Reverse lookup maps:** `BIND_LUA_REVERSE` generates a reverse-name lookup table at its declared canonical path.
- **Binding helpers:** `BIND_LUA_HELPER` attaches generic native helpers. The `eval` helper evaluates only the caller's expression, with optional locals taking precedence over runtime globals; a missing or empty expression returns `nil`, and non-empty evaluation runs synchronously and raises an error if called from a yieldable coroutine.

## Initialisation, injection and value traits

```cpp
BIND_MODULE_INIT()
void initialiseRuntimeServices(lua_State *state);

BIND_INJECT(global = "_LUDORK_BLUEPRINT_EVENT_VALIDATOR")
void setBlueprintEventValidator(BlueprintEventValidator validator);

BIND_CLASS(opaque_identity = true)
class RuntimeIdentity {};

BIND_CLASS(dynamic_value = true)
class RuntimeValue {};
```

- **Module init:** `BIND_MODULE_INIT` runs while opening the native module.
- **Callback injection:** `BIND_INJECT` names a Lua source callback via `global`, wires it to a C++ setter and remains valid only while that Lua state stays alive. With `variadic = true`, the setter must accept `std::function<std::vector<T>(Fixed..., const std::vector<T>&)>`; zero returns and interior or trailing `nil` values stay distinct without wrapper tables.
- **Value and identity traits:** `opaque_identity = true` and `dynamic_value = true` select conversion and identity traits and cannot combine with incompatible table-value modes. Engine subsystems rely on typed runtime facades and fixed provider slots rather than a generic string operation dispatcher.

> **Important:** Bindgen must remain declaration-driven. Do not introduce generator branches keyed to a gameplay module, class or member name, and do not use an infrastructure marker to bypass an unsupported conversion.
