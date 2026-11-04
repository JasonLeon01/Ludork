# Advanced Binding Patterns

These patterns cover engine infrastructure that the ordinary class, property, method and free-function workflow cannot represent.

## Class metadata decorators

Class decorators are nested in the owning class annotation:

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

`invalid_vars(...)` hides fields and `rect_range_vars(...)` links rectangles to resource or range fields. Both accept ordered identifiers, and only inside `BIND_CLASS`. Property-specific rectangle semantics remain `meta(RectRangeVars = "texturePath")` in `BIND_PROPERTY`. `metadata = false` does not disable a callback; use `callback = false` instead. See [Binding a Class](<Binding a Class.md>).

Member-level decorators and structured brace values follow [Declare properties](<Binding a Class.md#declare-properties>).

## Events, latent work and loops

```cpp
BIND_REGISTER_EVENT()
BIND_METHOD()
virtual void onCreate();

BIND_METHOD(loop_node(ForEach))
void forEach(const RuntimeValue::Array &values);
```

Event and loop annotations generate execution metadata; the C++ or Lua implementation must carry out the protocol. For latent operations and their return/default rules, see [Declare execution behaviour](<Functions Events and Execution.md#declare-execution-behaviour>).

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

Module properties are exposed at the module root, and class properties on their own class. Bindgen does not duplicate an existing Lua value under another runtime path. Reverse declarations create a new reverse-name lookup at their declared canonical path. Helpers use generic native support. The `eval` helper evaluates only its caller's expression, and optional locals take precedence over runtime globals. A missing or empty expression returns `nil`. Non-empty evaluation is synchronous and raises when called from a yieldable coroutine.

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

`BIND_MODULE_INIT` runs while opening the module. `BIND_INJECT` names its Lua source callback with `global` and connects it to a C++ setter and is valid only while that Lua state remains available. With `variadic = true`, the setter must accept `std::function<std::vector<T>(Fixed..., const std::vector<T>&)>`. Zero returns and interior or trailing `nil` remain distinct without envelope tables. The two `BIND_CLASS` flags select conversion and identity traits, and they cannot combine with incompatible table-value modes. Engine subsystems use typed runtime facades and fixed provider slots, not a generic string operation dispatcher.

## Limitations

Bindgen must remain declaration-driven. Do not introduce generator branches keyed to a gameplay module, class or member name, and do not use an infrastructure marker to bypass an unsupported conversion.

## Related pages

- [Macro Reference](<Macro Reference.md>)
- [Generated Metadata and Stubs](<Generated Metadata and Stubs.md>)
