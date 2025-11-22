# Ludork
## Quick Start
- Create virtual environment `LudorkEnv` and install dependencies:
  - `py -m venv LudorkEnv`
  - `LudorkEnv\Scripts\activate`
  - `pip install -r requirements.txt`
  - `python main.py`

- Or simply run `init.bat`:
  - If `LudorkEnv` already exists, the script activates it and runs `main.py`
  - If not, it creates `LudorkEnv`, then runs `pip install -r requirements.txt`
  - If creation or dependency installation fails, the script automatically deletes `LudorkEnv` to prevent further errors
  - On success, it activates `LudorkEnv` and runs `main.py`

## Manual Steps
1. Create virtual environment: `py -m venv LudorkEnv`
2. Activate virtual environment: `LudorkEnv\Scripts\activate`
3. Install dependencies: `pip install -r requirements.txt`
4. Run: `python main.py`

## Build & Pack
- Build Sample executable: run `build_exec.bat`
- Package the entire engine: run `pack.bat` (automatically runs `build_exec.bat`)

## Preview
![Preview](docs/images/image-1.png)
![Preview](docs/images/image-2.png)


## Dependencies
- `PyQt5`, `psutil`, `pympler`
- Install via `pip install -r requirements.txt`
