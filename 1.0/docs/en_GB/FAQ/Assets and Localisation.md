# Assets and Localisation

## How do I enable video playback?

Select **Enable video playback (FFmpeg)** when creating the project. This adds local MOV and MP4 playback for H.264 video and AAC audio and increases the build size. Other codecs, network playback and hardware acceleration are not included. See [FFmpeg option](<../Getting Started/Project Templates.md#ffmpeg-option>) for platform and distribution requirements.

## Why does an asset reference break after I move or rename a file?

Stored resource references use exact-case `/Game/Assets/...` paths with file extensions. Moving an asset does not repair every stored reference automatically. Open its reference tree and update the affected data manually. Symbolic links and paths outside the project are also rejected. See [Assets](<../Editor User Guide/Project Settings and Asset Layout.md#assets>).

## Why do changed translations not appear in the game?

After editing `Data/Locale/Locale.xlsx`, use Official Locale Tools through **Export**, then restart the game. Display General Data strings through `LOC`; they are not translated automatically. Lua hot reload preserves the loaded language tables. See [Export](<../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#export>) and [Runtime](<../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#runtime>).
