# GlobalFunctions.Save

`GlobalFunctions.Save` provides Blueprint nodes for saving game state, loading from a save file and resolving numbered save-slot file paths.

**Metadata source:** `Scripts/GlobalFunctions/Save_meta.lua`

## Save

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `SaveGame` | `filePath: string = ""` | — | Saves the current game state to `filePath`. |
| `LoadGame` | `filePath: string = ""` | `int` | Execution branches: `Loaded` (`0`), `NotFound` (`1`). Loads a saved game from `filePath`. |
| `GetSavePath` | `slot: int = 1` | `path: string` | Pure. Resolves the save file path for `slot`. |
