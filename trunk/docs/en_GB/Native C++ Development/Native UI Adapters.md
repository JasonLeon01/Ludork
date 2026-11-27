# Native UI Adapters

Add a native C++ control to Ludork's declarative UI contract so the runtime, PreviewHost and editor control registry share one descriptor definition. This workflow applies to C++ Source projects and assumes familiarity with the [Declarative UI Runtime Contract](<../Lua and Blueprint Scripting/Declarative UI/Runtime Contract.md>).

## Descriptor contract

System control descriptors live in `Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp` and use the private `BIND_UI_CONTROL` X-macro (not a Core bindgen annotation):

```cpp
BIND_UI_CONTROL(TAG, CONTROL_ID, ADAPTER, DISPLAY_NAME, CATEGORY,
                CHILD_POLICY, SLOT_TYPE, ...PROPERTIES)
```

| Argument | Meaning |
|---|---|
| `TAG` | Unique C++ tag used to specialise `UiControlAdapterTraits` |
| `CONTROL_ID` | Stable serialised system-control identifier; must be the native class's canonical module-root path |
| `ADAPTER` | Runtime adapter identifier; must match `CONTROL_ID` |
| `DISPLAY_NAME` | Palette display label |
| `CATEGORY` | Palette category |
| `CHILD_POLICY` | `UiChildPolicy::None`, `Single` or `Multiple` |
| `SLOT_TYPE` | `UiControlSlotType::None`, `Canvas` or `List` |
| `...PROPERTIES` | One or more property descriptors |

For every system control, `CONTROL_ID` and `ADAPTER` must be identical canonical paths (for example, `Engine.Canvas` for a bound `Canvas` in `Engine`); nested or abbreviated identifiers are invalid. Before exporting descriptors, the native registry validates IDs, property types and defaults, property responsibilities and adapter registration.

### Property descriptor macros

Each property descriptor takes `(ID, DISPLAY_NAME, TYPE, REQUIRED, DEFAULT_JSON)` and uses one of three macros:

| Macro | Scope |
|---|---|
| `UI_CONTROL_COMMON_PROPERTY` | Shared property applied to every control |
| `UI_CONTROL_PROPERTY` | Serialised property read and applied by the runtime adapter |
| `UI_CONTROL_EDITOR_PROPERTY` | Editor- and preview-only property |

Plain-text controls include `UI_CONTROL_PLAIN_TEXT_PROPERTIES(DEFAULT_SIZE)`, and rich-text controls include `UI_CONTROL_RICH_TEXT_PROPERTIES`. These property groups also determine the expected `TextConfig` type for editor and packaging validation.

### Decoding and special controls

Asset JSON is read as `RuntimeData` and each property is decoded through its descriptor into `UiControlPropertyValue`. Factories receive the decoded `UiControlProperties` dictionary, and setters receive one typed property value.

- **Enum properties (`Image::DrawAs`):** Image `drawAs` uses the bound C++ `Image::DrawAs` enum, exported as `Engine.ImageDrawAs`. JSON stores the member name, whereas native decoding and runtime setters carry the enum value itself (`AssetInstance:setProperty` therefore passes `Engine.ImageDrawAs.Tile`). Keep the decoder, property-value variant, Inspector enum selector, Toolchain validation and generated declarations in sync.
- **Template children (`Engine.WrapBox`):** Uses `UiChildPolicy::Single` and List Slots, treating its authored child as a template for repeated instances. See [UI Asset Schema and Control Registry](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md>).

## Registering a native control

1. Implement the native control type together with generic adapter operations for construction, property assignment, measurement, arrangement and child attachment.
2. Add a `BIND_UI_CONTROL` entry to `LUDORK_UI_CONTROL_DEFINITIONS` with a stable canonical control ID, child policy and Slot type.
3. Declare properties using existing supported property types and resource handling. Introducing a new property type also requires updating the native decoder, editor and Toolchain.
4. Register the tag in `UiControlAdapterRegistry`, providing the factory, setter, arranger and child callbacks required by the descriptor. The registry copies adapter-property IDs directly from the descriptor.
5. Build the C++ project using the platform `tools/build_cpp` script to refresh PreviewHost, update the control registry and validate UI assets. Controls using existing property types do not require rebuilding the editor or Toolchain.

### Built-in reference adapters

- **`Engine.TabView` (childless Input control):** Serialises construction and visual resources only, binding labels and callbacks at runtime. See [UI Asset Schema and Control Registry](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md>) and [Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>).
- **`Engine.CharacterView` (childless animated Visual control):** Constructs native preview state from `size`, texture and `Rect`, Actor scale, animation, material reference and instance parameters. The control class owns cropping, frame advancement and effect composition, while the adapter handles resource-path conversion and property assignment.

For preview build artefacts, registry publication and packaging rules, see [PreviewHost: Build, Publish and Troubleshooting](<../PreviewHost/Build Publish and Troubleshooting.md>).

> **Note:** Native controls must live in the project's Core UI source and satisfy the preview runtime's dependency boundary; editor plug-ins cannot register native controls in the Palette. Controls with `UiChildPolicy::None` cannot host children, and `UiChildPolicy::Single` accepts at most one child.
