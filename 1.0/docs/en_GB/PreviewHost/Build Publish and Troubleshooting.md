# Build, Publish and Troubleshooting

PreviewHost belongs to the editable project. Keep its executable, dedicated runtime and UI registry matched to the project's native controls and the editor protocol.

## Preparing Source and Standalone projects

For a C++ Source project, **Construct** builds the Debug game and preview runtime without launching gameplay. A full desktop `tools/build_cpp` build also produces the preview artefacts. Standalone projects use the matching preview runtime bundled in `Binaries`. Map edits can be previewed without Play or an Export after every change.

Windows x64 previews support the same MSVC and MinGW-w64 game builds. Follow [Build the project](<../Native C++ Development/Build and Module Layout.md#build-the-project>) for MinGW preparation and the runtime DLLs carried by editable projects and packages.

Build only the host with:

```text
tools\build_preview_host.bat Game Debug
```

On macOS:

```sh
sh tools/build_preview_host.sh Game Debug
```

The tool command family is `ScriptTools preview`. Validate published project artefacts with `ScriptTools preview validate Game`; replace `Game` with the project root when necessary. Native UI control changes require rebuilding the project and publishing its updated registry. Follow [Native UI Adapters](<../Native C++ Development/Native UI Adapters.md>) for registration and [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) for asset and generated-code ownership.

## Build configurations

| Build | `LUDORK_BUILD_PREVIEW_HOST` | `LUDORK_BUILD_GAME_APPLICATION` | UI validation |
|---|---|---|---|
| Full desktop development | `ON` | `ON` | Matching desktop registry |
| Preview only | `ON` | `OFF` | Host publishes the project registry |
| Game only | `OFF` | `ON` | Supply `LUDORK_UI_REGISTRY_PATH` with the matching desktop registry |

Mobile cross-builds require an explicitly supplied desktop UI registry. Native builds do not generate UI Lua Views or declarations; follow the separate [UI export workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>). See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for the build/export records required by Play and Pack.

## Published artefacts and rebuilding

A desktop native build produces:

| Artefact | Purpose |
|---|---|
| `PreviewHost` | Native preview process for maps, Actors, particles, subtitles and UI |
| `PreviewHostRuntime` | Dedicated preview runtime library |
| `EditorCache/Preview.json` | Manifest describing the matching binaries and registry |
| `EditorCache/UiPreview.registry.json` | UI-specific control registry |
| `EditorCache/Preview.building` | Temporary marker suspending connections during a rebuild |

Source preview and game binaries share `bin/<configuration>`; Standalone uses `Binaries`. The editor reconnects after the new artefacts are published. Compilation, linking, description or publication failures leave preview unavailable until the next successful publication clears the build marker. Full UI asset validation runs afterwards, so an asset validation error can fail the build while leaving the newly published preview available for editing. The generic service and snapshot names are `PreviewRuntimeService` and `PreviewRuntimeSnapshot`. UI-specific `UiPreviewClient`, `UiPreviewSession` and `UiPreview.registry.json` retain their names.

Missing, damaged or mismatched Standalone preview JSON is regenerated from the current `Binaries/PreviewHost` during preview preparation, Save, Export or `ScriptTools preview ensure <project-root>`. Recovery replaces only the two preview JSON files and preserves other cache files. A build marker blocks recovery; missing host binaries or dependencies remain errors. Source projects require a build when their selected preview metadata is unavailable.

## Editable templates and final packages

[Source-to-Standalone conversion](<../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>) builds and publishes the matching Release preview with the editable project. On macOS, signing refreshes the preview build identity and validates the host before publishing the output. Only preview metadata is retained in `EditorCache`; the source project’s Export record is not copied.

Source and Standalone editable projects and templates retain preview capabilities. Final game packaging uses the valid project UI registry for asset validation, but excludes preview executables, dedicated runtime libraries (including versioned libraries), manifests, build markers, temporary frame files and the `Engine/PreviewHost` source tree. The game executable and its runtime libraries do not link `PreviewHostRuntime`.

This exclusion applies to loose and `.ldpak` resource layouts and the platform binary/resource directories, including macOS application bundles. Packaging works on a staging copy and does not remove preview support from the editable project. General distribution, encryption and signing options remain in [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md#packaging>).

## Troubleshooting

| Symptom | Action |
|---|---|
| Host, manifest or registry is missing | Construct the C++ Source project. For Standalone, ensure its preview metadata; update the matching runtime if its binaries or dependencies are missing. Then validate the published artefacts. |
| Protocol or capability is incompatible | Rebuild or update the host to supply the [required preview capabilities](<Architecture and Data Flow.md#capabilities-and-timing>). There is no fallback map renderer. |
| Preview pauses during a build | Wait for publication and reconnection. If the build failed, correct the build error and rebuild before retrying preview. |
| A newly added native control is unavailable | Rebuild its adapter and registry together; adding an editor palette entry alone does not register a native control. |
| Packaging rejects UI assets or a registry | Correct the reported asset/registry mismatch using the matching desktop build; do not replace the registry with one from an unrelated project. |
| Map dragging or animation is delayed | Inspect the timing stages in [Architecture and Data Flow](<Architecture and Data Flow.md#capabilities-and-timing>) at the actual output size. Native drawing, GPU readback, pixel copying and UI publication have separate costs. |

View-specific placeholders and unsupported content are described in [UI, Actor, Particle and Subtitle Previews](<UI Actor Particle and Subtitle Previews.md>); map display controls are in [Map and World Previews](<Map and World Previews.md>).
