# Ludork

Ludork is a software project that appears to focus on game development, providing a set of tools and utilities for creating and managing game content. This repository contains various components including UI widgets, game engine utilities, and gameplay-related modules.

## Key Components

### Widgets
- **FileExplorer**: A file explorer widget with features like drag-and-drop, file operations (copy, cut, paste, delete), and filtering of specific file types.
- **ConsoleWidget**: A console interface for interacting with processes, displaying output, and handling user input with history support.
- **FilePreview**: A widget for previewing different file types including images (png, jpg, jpeg, bmp, gif, webp) and audio files (mp3, wav, ogg, flac, aac, m4a).
- **EditModeToggle**: A toggle widget for switching between tile mode and actor mode, likely used in game level editing.
- **ConfigDictPanel**: A panel for editing configuration data with support for various data types and file selection.

### Engine Utilities
- **File Handling (U_File)**: Utilities for reading JSON data and saving/loading data using pickle.
- **Math (U_Math)**: Mathematical utilities, such as checking if a vector is near zero.
- **Sound Processing (F_Sound)**: Audio processing functions with effects like water modulation, bubble noise, and reverb.

### Gameplay Components
- **ParticleSystem (G_ParticleSystem)**: A base class for particle systems with a late tick update method.
- **SceneBase (G_SceneBase)**: A base class for game scenes, handling the main game loop, updates, and fixed ticks.
- **Camera (G_Camera)**: A camera class for controlling the view position.
- **GameMap (G_GameMap)**: A class for managing game maps and updating actor lists.
- **Actors**: Base classes for game actors and characters.

### UI Elements
- **TextBox (UI_TextBox)**: A UI text box widget supporting text input and editing.
- **RectBase (UI_RectBase)**: A base class for rendering UI rectangles with edges and corners.

## License
This project is licensed under terms specified in the [LICENSE.md](LICENSE.md) file. The license includes provisions for Corresponding Source, which includes interface definition files and source code for shared libraries that the work is specifically designed to require.

## Usage
The components in this repository can be used to build game editing tools and game engine functionality. The widgets provide a user interface for managing files, configuring settings, and previewing assets, while the engine utilities and gameplay components offer core functionality for game development.

To get started, explore the various modules and their respective functionalities. The widgets can be integrated into a Qt application, while the engine and gameplay components can be used to build game logic and systems.
