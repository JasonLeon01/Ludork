# Assets and Localisation

## How do I enable video playback?

Select **Enable video playback (FFmpeg)** when creating the project. This adds local MOV and MP4 playback for H.264 video and AAC audio and increases the build size. Other codecs, network playback and hardware acceleration are not included. See [FFmpeg option](<../Getting Started/Project Templates.md#ffmpeg-option>) for platform and distribution requirements.

## Why does an asset reference break after I move or rename a file?

Rename or move assets through the editor, then Save. External renames and source-code or dynamic references require manual updates; see [Assets](<../Editor User Guide/Project Settings and Asset Layout.md#assets>) for path requirements.

For an external-change conflict while a move is still unsaved, see [Editing and saving](<../Editor User Guide/Start Page Workspace and Shortcuts.md#editing-and-saving>).

## Why do changed translations not appear in the game?

After editing `Data/Locale/Locale.xlsx`, use Official Locale Tools through **Export**, then restart the game. Display General Data strings through `LOC`; they are not translated automatically. Lua hot reload preserves the loaded language tables. See [Export](<../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#export>) and [Runtime](<../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#runtime>).
