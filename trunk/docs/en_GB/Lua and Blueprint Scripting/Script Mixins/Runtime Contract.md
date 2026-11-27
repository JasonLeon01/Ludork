# Script Mixin Runtime Contract

At runtime, Ludork loads a Script Mixin as `Mixins.<path-with-dots>` and requires it to return a plain Lua table with no metatable. This contract defines generated class members, instance defaults, reserved names, parent calls and hot reload.

## Table rules

- Every table key must be a string.
- Function entries become methods on the generated Blueprint class.
- Non-function values are deep-copied into class and instance defaults.
- An inherited member cannot change kind between field and function.
- Saved Blueprint attributes cannot replace a mixin method.

The following member names are reserved:

```text
__*  init  new  scriptMixin  scriptPath
_GENERATED_CLASS  _graph  _hasImplementationOwner
```

> **Note:** Because `init` is reserved, a Script Mixin cannot declare a constructor. Initialise private instance state in the owning type's lifecycle callback (such as `onCreate`) and release subscriptions or resources in the matching teardown callback.

## Parent method calls

Call `super()` inside a mixin method to continue the Blueprint and native inheritance chain:

```lua
local Door = {}

function Door:onCollision(other)
    if self.opening then
        return
    end
    super().onCollision(other)
    self:openDoor()
    self.opening = true
end

return Door
```

The parent class must supply every native property and method the mixin accesses; referencing a missing member raises an error.

## Hot reload

[Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) applies to active Script Mixins: `$r` updates compatible methods without rerunning lifecycle callbacks or overwriting defaults and live instance state. Structural changes require a restart.

Lua compilation and `.ldpak` packaging preserve mixin module paths and fresh-load behaviour. See [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>) for packaging options.
