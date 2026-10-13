# Script Mixin Runtime Contract

The runtime loads the selected module as `Mixins.<path-with-dots>` and requires it to return a table with no metatable. This contract governs generated Blueprint-class members, instance defaults, reserved names and parent-method dispatch.

## Table rules

- Every key must be a string.
- A function becomes a method on the generated Blueprint class.
- A non-function value is deep-copied into class and instance defaults.
- An inherited member cannot change from field to function, or from function to field.
- A saved Blueprint attribute cannot replace a mixin method.

The following names are reserved:

```text
__*  init  new  scriptMixin  scriptPath
_GENERATED_CLASS  _graph  _hasImplementationOwner
```

Because `init` is reserved, a Direct Script Mixin does not declare a constructor. Initialise private instance state from the lifecycle callback supplied by the owning type, such as `onCreate`. Release subscriptions or resources in the corresponding teardown callback.

## Parent method calls

Use `super()` inside a mixin method to continue the Blueprint and native inheritance chain:

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

The parent class must provide the native properties and methods the mixin uses. Accessing a missing member raises an error.

## Hot reload

Console `$r` updates existing methods in used Script Mixins without rerunning `onCreate` or teardown. Mixin defaults and instance state remain unchanged. Structural changes require a restart. See [Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) for compatibility and failure rules.

Lua compilation and `.ldpak` packaging preserve mixin module paths and fresh-load behaviour. See [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>) for packaging options.

## Related pages

- [Script Mixin Authoring Workflow](<Authoring Workflow.md>)
- [Ludork Lua Advanced](<../../Getting Started/Ludork Lua Advanced.md>)
