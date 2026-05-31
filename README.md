# Ludork

Ludork is a 2D RPG toolkit built with Python, PyQt5, SFML/PySF, and pybind11 native extensions. It provides an editor for creating project data and a Python game runtime that uses the same data directly.

The repository includes the toolkit application, the native extension build chain, and a complete `Sample/` project that can be used as a template for new games.

## What It Contains

- `main.py` and `LoadEditor.py`: application entry and editor bootstrap.
- `EditorGlobal/`, `Widgets/`, `NodeGraph/`, `Utils/`: editor-side application code.
- `Sample/`: runnable RPG sample project, including game scripts, data, assets, save support, and runtime dependencies.
- `Sample/Engine/`, `Sample/Global/`, `Sample/Source/`: game runtime and sample gameplay code.
- `C_Extensions/`: pybind11 and SFML native extension sources.
- `docs/en_GB/` and `docs/zh_CN/`: editor workflow and runtime scripting manual.

## Requirements

- Python 3.12
- CMake and a C++ toolchain for native extensions
- Windows PowerShell, or standard shell tools on macOS/Linux

## Quick Start

Initial setup:

```bat
.\init.bat
```

```bash
./init.sh
```

Start the editor later:

```bat
.\run.bat
```

```bash
./run.sh
```

Run the sample game directly:

```bash
cd Sample
python Entry.py
```

Build native extensions manually:

```bash
python C_Extensions/build.py
```

## Documentation

The manual is split into editor usage first, then runtime and scripting reference:

- English: `docs/en_GB/`
- Chinese: `docs/zh_CN/`

Start with chapters `02`-`06` for editor workflows such as project setup, assets, maps, blueprints, data, localisation, nodes, and testing. Use chapters `07`-`17` for runtime structure, APIs, scripting, configuration, and packaging.

## License

See [LICENSE.md](LICENSE.md).
