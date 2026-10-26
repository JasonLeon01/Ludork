# UI, Actor, Particle and Subtitle Previews

Open the relevant asset editor to inspect its appearance and timing. These previews follow the [preview/gameplay boundary](<Overview.md#preview-and-gameplay>). For host availability, rebuilds and errors, see [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>).

## UI previews

The [UI Asset Editor](<../Editor User Guide/UI Asset Editor.md>) displays the authored asset without running its Lua Controller. Use **Refresh Preview** to display pending edits. After changing native controls, rebuild the project before checking their result.

Provide representative placeholder content for model-dependent areas. Use `previewText` for strings supplied by code at runtime; it belongs to the node's design-only `editor` data. Representative `#TAB` and `#ITEM` entries and Game visuals make tabs, lists and character displays readable, but are not fallback game data. Before display, business code must supply localised labels and populate dynamic collections. The runtime responsibilities remain in [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>).

Select an animation in the Timeline, then use play, pause, stop and scrubbing to inspect it without changing the saved Slot layout. With no animation selected, the Designer shows the authored UI appearance. Animation definitions, inherited Global animations and instance overrides are edited as described in the [UI Timeline](<../Editor User Guide/UI Asset Editor.md#timeline>).

For nested-asset and repeated-template editing, see the [UI Asset Editor](<../Editor User Guide/UI Asset Editor.md>). Control properties, children and Slot rules remain in [UI Controls in the Palette](<../Editor User Guide/UI Controls in the Palette.md>).

## Actor previews

Selecting a Blueprint JSON file in a file selector previews its current appearance and animation, including unsaved changes. A missing appearance or texture displays the file icon.

File Explorer thumbnails and Blueprint editing surfaces use resolved Actor appearances. Texture regions, character frame animation, `material` and `materialParameters` come from the current Blueprint values. Map instances also apply their instance overrides. Changing inherited defaults, including Undo/Redo, refreshes the affected visuals.

See [Actor placement and Actor Info](<../Editor User Guide/Blueprints Common Functions and Actors.md#actor-placement-and-actor-info>) for selecting Blueprints and editing instances, and [Map and World Previews](<Map and World Previews.md>) for map lighting, selection aids and Live Debug display.

## Material previews

Material, Actor, map and UI previews receive unsaved material snapshots. The native host compiles them in memory without rewriting exported shaders; gameplay loads the exported files. Invalid previews report an error instead of retaining a stale successful image. For the shared compiler, preview controls, error navigation and GLSL view, see [Preview and diagnose](<../Editor User Guide/Materials.md#preview-and-diagnose>).

## Particle previews

Open a particle from **Database → Common Particle Overview** (`F6`) or the project file explorer. The preview can isolate the selected track. Use Play/Pause, Stop, Restart, a single simulation step, time entry, seeking, playback speed and zoom to inspect the effect.

A step advances `1 / simulationRate` seconds. Seeking pauses playback and can take longer for later times. These controls do not change the saved resource; track and property edits retain the normal Save and Undo workflow. A fixed resource seed supports repeatable authoring on the same runtime and GPU, not cross-device deterministic simulation.

Missing resources or unsupported GPU capabilities display an error. Particle preview uses the native GPU implementation and has no CPU fallback. See [Common Particles](<../Editor User Guide/Common Particles.md>) for resource fields, emission and ownership rules, GPU requirements and feature limits.

## Subtitle previews

Open a document under `Data/Subtitles` in the [Subtitle Editor](<../Editor User Guide/Video Subtitles.md>). Use Play/Pause, Stop, the current time, timeline playhead and preview language to check timing. Without a reference video, playback uses a plain background and ends at the last section's end time. An empty subtitle document remains editable.

Choose a project video for temporary reference, or clear it to return to the plain background. The video preserves its aspect ratio and plays synchronised audio, with a mute control. Seeking pauses audio and restores the previous playback state when positioning finishes. Preview duration is the longer of the video and subtitle durations; after the video ends, remaining subtitles appear on the plain background.

A project without FFmpeg reports that it is not an FFmpeg project when a video is selected. Subtitle editing and plain-background preview remain available. Reference video, preview language, playhead and mute belong to the window session and are not saved in the subtitle document.

Native preview and runtime playback share subtitle layout. Subtitle data, language editing, font, wrapping and layout rules remain in [Video Subtitles](<../Editor User Guide/Video Subtitles.md>).

## Related pages

- [Architecture and Data Flow](<Architecture and Data Flow.md>)
