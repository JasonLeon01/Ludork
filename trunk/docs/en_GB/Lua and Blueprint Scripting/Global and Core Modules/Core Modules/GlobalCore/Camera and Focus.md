# GlobalCore: Camera and Focus

This page documents `Camera`, floating tip notifications (`CommonTipController`) and keyboard/gamepad UI focus navigation (`FocusNeighbor`, `FocusGroup`, `FocusManager`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin.

## Camera

Controls the map view transform, viewport, Actor attachment, bound map and offscreen render texture.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setViewport` | `inViewport: sf.FloatRect` | — | Exec (`default`). |
| `getView` | — | `view: sf.View` | Pure. |
| `getViewPosition` | — | `position: sf.Vector2f` | Pure. |
| `setViewPosition` | `inPosition: sf.Vector2f` | — | Exec (`default`). |
| `getViewSize` | — | `size: sf.Vector2f` | Pure. |
| `setViewSize` | `inSize: sf.Vector2f` | — | Exec (`default`). |
| `getViewRotation` | — | `rotation: sf.Angle` | Pure. |
| `setViewRotation` | `inRotation: sf.Angle` | — | Exec (`default`). |
| `moveView` | `delta: sf.Vector2f` | — | Exec (`default`). |
| `rotateView` | `delta: sf.Angle` | — | Exec (`default`). |
| `resumeViewport` | — | — | Exec (`default`). |
| `getPosition` | — | `position: sf.Vector2f` | Pure. |
| `setPosition` | `inPosition: sf.Vector2f` | — | Exec (`default`). |
| `move` | `delta: sf.Vector2f` | — | Exec (`default`). |
| `getRotation` | — | `rotation: sf.Angle` | Pure. |
| `setRotation` | `inRotation: sf.Angle` | — | Exec (`default`). |
| `rotate` | `delta: sf.Angle` | — | Exec (`default`). |
| `getScale` | — | `scale: sf.Vector2f` | Pure. |
| `setScale` | `factors: sf.Vector2f` | — | Exec (`default`). |
| `scale` | `delta: sf.Vector2f` | — | Exec (`default`). |
| `getParent` | — | `parent: Engine.Actor` | Pure. |
| `setParent` | `actor: Engine.Actor` | — | Exec (`default`). |
| `setMap` | `map: GlobalCore.GameMapBase` | — | Exec (`default`). |
| `getMap` | — | `map: GlobalCore.GameMapBase` | Pure. |
| `mapPixelToCoords` | `point: sf.Vector2i` | `sf.Vector2f` | — |
| `mapCoordsToPixel` | `point: sf.Vector2f` | `sf.Vector2i` | — |
| `getRenderTexture` | — | `renderTexture: sf.RenderTexture` | Pure. |

## CommonTipController

Manages transient on-screen text tip banners.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `addTip` | `text: string` | — | — |
| `onTick` | `deltaTime: float` | — | — |

## FocusNeighbor

Describes a directional navigation edge from one `FocusGroup` to a neighbouring group.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `group` | `GlobalCore.FocusGroup` | — | Non-owning reference. |
| `transition` | `string` | — | — |

> **Note:** `group` is non-owning, so `FocusManager` or the caller must retain the target group. If the target is destroyed, `group` reads as `nil` and the edge is ignored. Reciprocal neighbours do not extend each other's lifetimes.

## FocusGroup

Groups focusable `Engine.FunctionalBase` controls and defines directional transitions between neighbouring groups.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | — | — |
| `activeOwner` | `Engine.FunctionalBase` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `addItem` | `item: Engine.FunctionalBase` | — | — |
| `removeItem` | `item: Engine.FunctionalBase` | — | — |
| `getItems` | — | `Engine.FunctionalBase[]` | — |
| `setNeighbor` | `direction: string`, `neighbor: GlobalCore.FocusGroup`, `transition: string = "directional"` | — | — |
| `getNeighbor` | `direction: string` | `GlobalCore.FocusNeighbor` | — |
| `canEnter` | — | `bool` | — |
| `findInitialFocus` | — | `Engine.FunctionalBase` | — |
| `rememberFocus` | `element: Engine.FunctionalBase` | — | — |
| `moveWithin` | `current: Engine.FunctionalBase`, `direction: string` | `Engine.FunctionalBase` | — |

## FocusManager

Coordinates active keyboard and gamepad focus across registered UI elements and `FocusGroup` instances.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setNavigationEnabled` | `enabled: bool` | — | — |
| `getNavigationEnabled` | — | `bool` | — |
| `isRoutingKeyboard` | — | `bool` | — |
| `registerElement` | `element: Engine.FunctionalBase` | — | — |
| `unregisterElement` | `element: Engine.FunctionalBase` | — | — |
| `registerFocusGroup` | `group: GlobalCore.FocusGroup` | — | — |
| `unregisterFocusGroup` | `group: GlobalCore.FocusGroup` | — | — |
| `getFocus` | — | `Engine.FunctionalBase` | — |
| `setFocus` | `element: Engine.FunctionalBase` | `bool` | — |
| `clearFocus` | — | — | — |
| `prepareFrame` | — | — | — |
| `shouldDispatchKeyboardTo` | `element: Engine.FunctionalBase` | `bool` | — |
| `isFocused` | `element: Engine.FunctionalBase` | `bool` | — |
| `isCursorFocusOwner` | `element: Engine.FunctionalBase` | `bool` | — |
| `requestDirectionalMove` | `element: Engine.FunctionalBase`, `direction: string` | `bool` | — |
| `moveFocus` | `direction: string`, `source: Engine.FunctionalBase = nil` | `bool` | — |
| `activateGroup` | `group: GlobalCore.FocusGroup` | `bool` | — |
