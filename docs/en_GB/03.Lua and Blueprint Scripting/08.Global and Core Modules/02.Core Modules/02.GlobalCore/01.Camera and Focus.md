# GlobalCore: Camera and Focus

## Camera

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `setViewport` | `function` | self: { "GlobalCore", "Camera" } = "self"; inViewport: sf.FloatRect | — | ExecSplit { [1] = "default", default = nil } |
| `getView` | `function` | self: { "GlobalCore", "Camera" } = "self" | view: sf.View | Pure |
| `getViewPosition` | `function` | self: { "GlobalCore", "Camera" } = "self" | position: sf.Vector2f | Pure |
| `setViewPosition` | `function` | self: { "GlobalCore", "Camera" } = "self"; inPosition: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `getViewSize` | `function` | self: { "GlobalCore", "Camera" } = "self" | size: sf.Vector2f | Pure |
| `setViewSize` | `function` | self: { "GlobalCore", "Camera" } = "self"; inSize: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `getViewRotation` | `function` | self: { "GlobalCore", "Camera" } = "self" | rotation: sf.Angle | Pure |
| `setViewRotation` | `function` | self: { "GlobalCore", "Camera" } = "self"; inRotation: sf.Angle | — | ExecSplit { [1] = "default", default = nil } |
| `moveView` | `function` | self: { "GlobalCore", "Camera" } = "self"; delta: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `rotateView` | `function` | self: { "GlobalCore", "Camera" } = "self"; delta: sf.Angle | — | ExecSplit { [1] = "default", default = nil } |
| `resumeViewport` | `function` | self: { "GlobalCore", "Camera" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `getPosition` | `function` | self: { "GlobalCore", "Camera" } = "self" | position: sf.Vector2f | Pure |
| `setPosition` | `function` | self: { "GlobalCore", "Camera" } = "self"; inPosition: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `move` | `function` | self: { "GlobalCore", "Camera" } = "self"; delta: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `getRotation` | `function` | self: { "GlobalCore", "Camera" } = "self" | rotation: sf.Angle | Pure |
| `setRotation` | `function` | self: { "GlobalCore", "Camera" } = "self"; inRotation: sf.Angle | — | ExecSplit { [1] = "default", default = nil } |
| `rotate` | `function` | self: { "GlobalCore", "Camera" } = "self"; delta: sf.Angle | — | ExecSplit { [1] = "default", default = nil } |
| `getScale` | `function` | self: { "GlobalCore", "Camera" } = "self" | scale: sf.Vector2f | Pure |
| `setScale` | `function` | self: { "GlobalCore", "Camera" } = "self"; factors: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `scale` | `function` | self: { "GlobalCore", "Camera" } = "self"; delta: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `getParent` | `function` | self: { "GlobalCore", "Camera" } = "self" | parent: { "Engine", "Actor" } | Pure |
| `setParent` | `function` | self: { "GlobalCore", "Camera" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `setMap` | `function` | self: { "GlobalCore", "Camera" } = "self"; map: { "GlobalCore", "GameMapBase" } | — | ExecSplit { [1] = "default", default = nil } |
| `getMap` | `function` | self: { "GlobalCore", "Camera" } = "self" | map: { "GlobalCore", "GameMapBase" } | Pure |
| `mapPixelToCoords` | `function` | self: { "GlobalCore", "Camera" } = "self"; point: sf.Vector2i | return: sf.Vector2f | — |
| `mapCoordsToPixel` | `function` | self: { "GlobalCore", "Camera" } = "self"; point: sf.Vector2f | return: sf.Vector2i | — |
| `getRenderTexture` | `function` | self: { "GlobalCore", "Camera" } = "self" | renderTexture: sf.RenderTexture | Pure |

## CommonTipController

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `addTip` | `function` | self: { "GlobalCore", "CommonTipController" } = "self"; text: string | — | — |
| `onTick` | `function` | self: { "GlobalCore", "CommonTipController" } = "self"; deltaTime: float | — | — |

## FocusNeighbor

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `group` | `{ "GlobalCore", "FocusGroup" }` | — | — |
| `transition` | `string` | — | — |

The `group` field is non-owning, so the `FocusManager` or the caller must retain its target. If the target is destroyed, the field reads as `nil` and the edge is ignored. Reciprocal neighbours do not extend the lifetime of either side.

### Functions and events

No Blueprint functions or events are declared.

## FocusGroup

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `name` | `string` | — | — |
| `activeOwner` | `{ "Engine", "FunctionalBase" }` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `addItem` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; item: { "Engine", "FunctionalBase" } | — | — |
| `removeItem` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; item: { "Engine", "FunctionalBase" } | — | — |
| `getItems` | `function` | self: { "GlobalCore", "FocusGroup" } = "self" | return: { "Engine", "FunctionalBase[]" } | — |
| `setNeighbor` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; direction: string; neighbor: { "GlobalCore", "FocusGroup" }; transition: string = "directional" | — | — |
| `getNeighbor` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; direction: string | return: { "GlobalCore", "FocusNeighbor" } | — |
| `canEnter` | `function` | self: { "GlobalCore", "FocusGroup" } = "self" | return: bool | — |
| `findInitialFocus` | `function` | self: { "GlobalCore", "FocusGroup" } = "self" | return: { "Engine", "FunctionalBase" } | — |
| `rememberFocus` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; element: { "Engine", "FunctionalBase" } | — | — |
| `moveWithin` | `function` | self: { "GlobalCore", "FocusGroup" } = "self"; current: { "Engine", "FunctionalBase" }; direction: string | return: { "Engine", "FunctionalBase" } | — |

## FocusManager

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `setNavigationEnabled` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; enabled: bool | — | — |
| `getNavigationEnabled` | `function` | self: { "GlobalCore", "FocusManager" } = "self" | return: bool | — |
| `isRoutingKeyboard` | `function` | self: { "GlobalCore", "FocusManager" } = "self" | return: bool | — |
| `registerElement` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | — | — |
| `unregisterElement` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | — | — |
| `registerFocusGroup` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; group: { "GlobalCore", "FocusGroup" } | — | — |
| `unregisterFocusGroup` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; group: { "GlobalCore", "FocusGroup" } | — | — |
| `getFocus` | `function` | self: { "GlobalCore", "FocusManager" } = "self" | return: { "Engine", "FunctionalBase" } | — |
| `setFocus` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | return: bool | — |
| `clearFocus` | `function` | self: { "GlobalCore", "FocusManager" } = "self" | — | — |
| `prepareFrame` | `function` | self: { "GlobalCore", "FocusManager" } = "self" | — | — |
| `shouldDispatchKeyboardTo` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | return: bool | — |
| `isFocused` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | return: bool | — |
| `isCursorFocusOwner` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" } | return: bool | — |
| `requestDirectionalMove` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; element: { "Engine", "FunctionalBase" }; direction: string | return: bool | — |
| `moveFocus` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; direction: string; source: { "Engine", "FunctionalBase" } = nil | return: bool | — |
| `activateGroup` | `function` | self: { "GlobalCore", "FocusManager" } = "self"; group: { "GlobalCore", "FocusGroup" } | return: bool | — |
