# Ludork

Ludork is a 2D RPG toolkit that pairs a PyQt5 editor with an SFML-based runtime (via PySF and pybind11 native extensions). It is designed for fast iteration: edit content in the editor, run/preview using the same data formats, and keep project code alongside project data.

## Highlights
- Visual Node Graph editor for reusable Common Functions (NodeGraphQt)
- Tileset and map editing with `.dat` persistence
- Asset browsing and preview for images, audio, shaders, and transitions
- Config editors plus localization support (spreadsheet source + per-locale outputs)
- Runtime preview driven by the Project entry script (for example `Entry.py` via `Main.ini`)
- Native-accelerated modules for engine/global/editor helpers (pybind11 + SFML)

## What’s in This Repository
- Editor application code: `EditorGlobal/`, `Widgets/`, `NodeGraph/`, `Utils/`, `Styles/`
- Sample Project (also a template): `Sample/` (contains `Main.ini`, `Entry.py`, `Data/`, `Assets/`, and runtime code packages)
- Native extensions (pybind11): `C_Extensions/` (builds into `EditorExtensions/` and `Sample/*`)
- Packaging & utility scripts: `tools/`, `pack.*`, `run.*`, `init.*`

## Visual Node Graph
- Purpose: author reusable logic blocks as visual graphs
- Node categories: Math, String, Utils, Containers, plus project-specific nodes
- Persistence: graphs saved under the Project `Data/CommonFunctions/*.dat` (see `Sample/Data/CommonFunctions/` for examples)
- Runtime: graphs are loaded and executed by the runtime NodeGraph subsystem

## Quick Start (Editor)
- Prerequisites:
  - Python 3.12
  - CMake + a working C++ toolchain (required for building `C_Extensions/`)
  - macOS/Linux: `curl`, `tar`, `unzip` (used by `init.sh`)
  - Windows: PowerShell (used by `init.bat`)
- Install & run (one step):
  - Windows:
    ```bat
    .\init.bat
    ```
  - macOS/Linux:
    ```bash
    ./init.sh
    ```

The init scripts will:
- Create a virtual environment at `LudorkEnv/` and install `requirements.txt`
- Download SFML 3.1.0 sources under `C_Extensions/SFML/`
- Download a prebuilt PySF package into the repository (used by both editor and runtime)
- Build native extensions via `C_Extensions/build.py`
- Launch the editor (`main.py`)

For subsequent launches:
- Windows: `.\run.bat`
- macOS/Linux: `./run.sh`

## Opening the Sample Project
- Run the editor, then open the `Sample/` folder as the Project root.
- The editor stores per-project editor settings in `Main.proj` inside the Project root (created automatically if missing).

## Running the Game Without the Editor (Sample)
From the Project root (for example `Sample/`):

```bash
cd Sample
python Entry.py
```

Notes:
- The Sample entry reads `Main.ini` and loads content from `Data/` and `Assets/`.
- `Entry.py` attempts to enable `debugpy` on `localhost:2333` if the dependency is available.

## Project Layout (Inside a Project Root)
- `Main.ini`: points to the runtime entry script (for example `[Main] script = Entry.py`)
- `Entry.py`: Project bootstrap (initializes locale/system/scenes)
- `Main.proj`: editor-side per-project settings (JSON)
- `Data/`: structured game data
  - `Configs/`, `Maps/`, `Tilesets/`, `CommonFunctions/`, `Animations/`, `Blueprints/`, `General/`, `Locale/`
- `Assets/`: media resources (images, audio, fonts, shaders, etc.)

## Native Extensions
Native modules are built from `C_Extensions/` and distributed into Python import locations:
- `EditorExt` → `EditorExtensions/`
- `EngineExt` → `Sample/Engine/`
- `GlobalExt` → `Sample/Global/`

To build manually (after installing Python deps and ensuring CMake/toolchain is available):

```bash
python C_Extensions/build.py
```

Useful options:
- `--no-clean` to reuse the existing CMake build directory
- `--only EditorExt|EngineExt|GlobalExt` to build/distribute one module
- `--skip-build` to only distribute artifacts (if you already built them)

## Packaging
- `pack.sh` / `pack.bat` runs `tools/pack.py` (Nuitka-based packaging pipeline)

## Documentation
- Full manual: `docs/en_GB/`
- Chinese translation: `docs/zh_CN/`

## License
This project is licensed as described in [LICENSE.md](LICENSE.md).
