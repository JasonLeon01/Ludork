# PreviewHost

PreviewHost supplies native rendering for editor previews of maps, worlds, Actors, UI, particles and subtitles. This chapter covers using those previews, preparing their runtime and understanding how the editor obtains their frames.

## Choose a preview

| Preview | Entry point | Guide |
|---|---|---|
| Maps, worlds and child-map thumbnails | Main workspace map viewport and world canvas | [Map and World Previews](<Map and World Previews.md>) |
| UI assets and their animations | UI Asset Editor Designer and Timeline | [UI previews](<UI Actor Particle and Subtitle Previews.md#ui-previews>) |
| Actor appearances | Blueprint previews, Actor Library and placed map Actors | [Actor previews](<UI Actor Particle and Subtitle Previews.md#actor-previews>) |
| Common particles | Common Particle Editor | [Particle previews](<UI Actor Particle and Subtitle Previews.md#particle-previews>) |
| Video subtitles | Subtitle Editor | [Subtitle previews](<UI Actor Particle and Subtitle Previews.md#subtitle-previews>) |

## Preview and gameplay

Editor previews display the content being authored. Map and Actor appearance previews resolve visual data without executing Actor creation, Tick or Blueprint gameplay events. The UI Designer displays authored assets without running their Lua Controllers. Particle and subtitle playback have their own preview controls, described in the guides above.

Use **Play** to test gameplay. **Live Debug** inspects an already running game and retains its own editing permissions; its map image follows the [map preview display rules](<Map and World Previews.md>). See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for game execution and live editing.

## Prepare or develop PreviewHost

- [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>) explains Source and Standalone preparation, runtime files, rebuilds and final-game packaging.
- [Architecture and Data Flow](<Architecture and Data Flow.md>) explains shared rendering, scene updates, frame transport and diagnostics.
