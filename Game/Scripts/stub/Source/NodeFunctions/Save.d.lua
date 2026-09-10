---@meta Source.NodeFunctions.Save

--- @brief Save the current game state to a file.
---
--- - @param filePath Path whose extension matches the format selected by `SAVE_AS_LDC`.
---@param filePath string
function Save.SaveGame(filePath) end

--- @brief Load game state from a file and apply it to the current scene.
---
--- - @param filePath Path whose extension matches the format selected by `SAVE_AS_LDC`.
--- - @return 0 if loaded successfully, 1 if the file was not found.
---@param filePath string
---@return integer
function Save.LoadGame(filePath) end

--- @brief Get the platform-specific save file path for a given slot.
---
--- - @param slot Save slot number (1 = default).
---   The `SAVE_AS_LDC` global selects `.ldc`; otherwise `.json` is used.
--- - @return Full path to the save file.
---@param slot integer
---@return string
function Save.GetSavePath(slot) end

return Save
