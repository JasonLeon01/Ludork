# UI, Actor, Particle and Subtitle Previews

Each asset editor uses PreviewHost to display live appearance and timing within the [preview/gameplay boundary](<Overview.md#preview-and-gameplay>). For host availability, rebuilds and errors, see [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>).

## UI previews

The [UI Asset Editor](<../Editor User Guide/UI Asset Editor.md>) renders the authored asset without running its Lua Controller. Select **Refresh Preview** to display pending edits; after modifying native controls, rebuild the project before checking the result.

Provide representative placeholder content for model-dependent areas. Use `previewText`—part of the node's design-only `editor` data—for runtime strings, and supply representative `#TAB` and `#ITEM` entries alongside Game visuals so tabs, lists and character displays stay readable during authoring.

Placeholders are not fallback game data: business code must supply localised labels and populate dynamic collections before display. See [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) for runtime ownership.

Select an animation in the Timeline, then use play, pause, stop and scrubbing to inspect it without modifying the saved Slot layout. With no animation selected, the Designer shows the authored UI appearance. See [UI Timeline](<../Editor User Guide/UI Asset Editor.md#timeline>) for animation definitions, inherited Global animations and instance overrides.

For nested-asset and repeated-template editing, see [UI Asset Editor](<../Editor User Guide/UI Asset Editor.md>); control properties, children and Slot rules are documented in [UI Controls in the Palette](<../Editor User Guide/UI Controls in the Palette.md>).

## Actor previews

Selecting a Blueprint JSON file in a file selector previews its current appearance and animation, including unsaved changes. A missing appearance or texture displays the file icon.

**File Explorer** thumbnails and Blueprint editing surfaces display resolved Actor appearances: texture regions, character frame animation, `material` and `materialParameters` come from current Blueprint values, with map instances applying their instance overrides. Updating inherited defaults (including Undo/Redo) refreshes affected visuals.

See [Actor placement and Actor Info](<../Editor User Guide/Blueprints Common Functions and Actors.md#actor-placement-and-actor-info>) for selecting Blueprints and editing instances, and [Map and World Previews](<Map and World Previews.md>) for map lighting, selection aids and Live Debug display.

## Material previews

Material, Actor, map and UI previews receive unsaved material snapshots. The native host compiles them in memory without modifying the game's runtime material cache. An invalid preview displays an error instead of showing a stale successful image. See [Preview and diagnose](<../Editor User Guide/Materials.md#preview-and-diagnose>) for the shared compiler, preview controls, error navigation and GLSL view.

## Particle previews

Open a particle from **Database → Common Particle Overview** (`F6`) or **File Explorer**. The preview can isolate the selected track and provides Play/Pause, Stop, Restart, single simulation step, time entry, seeking, playback speed and zoom controls.

A step advances `1 / simulationRate` seconds. Seeking pauses playback and can take longer for later times. These controls do not modify the saved resource; track and property edits follow the normal Save and Undo workflow. A fixed resource seed supports repeatable authoring on the same runtime and GPU, not cross-device deterministic simulation.

Missing resources or unsupported GPU capabilities report an error; particle preview uses the native GPU implementation with no CPU fallback. See [Common Particles](<../Editor User Guide/Common Particles.md>) for resource fields, emission and ownership rules, GPU requirements and feature limits.

## Subtitle previews

Open a document under `Data/Subtitles` in the [Subtitle Editor](<../Editor User Guide/Video Subtitles.md>). Use Play/Pause, Stop, current time, timeline playhead and preview language to check timing. Without a reference video, playback uses a plain background and stops at the last section's end time. An empty subtitle document is editable.

Select a project video for temporary reference, or clear it to return to the plain background. The video preserves its aspect ratio and plays synchronised audio with a mute toggle; seeking pauses audio and restores the previous playback state once positioning finishes. Preview duration is the longer of the video and subtitle durations, and any subtitles after the video ends appear over the plain background.

Selecting a video in a project without FFmpeg reports that it is not an FFmpeg project, while subtitle editing and plain-background preview work normally. Reference video, preview language, playhead and mute are window-session state and are not saved to the subtitle document.

Native preview and runtime playback share subtitle layout. Subtitle data, language editing, font, wrapping and layout rules are documented in [Video Subtitles](<../Editor User Guide/Video Subtitles.md>).
