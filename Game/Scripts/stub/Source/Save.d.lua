---@meta Source.Save

--- Save/Load system for persisting and restoring game state.
---
--- Save data is serialized as JSON and stored in plain `.json` or encrypted
--- `.ldc` files.

---@brief Save game state to the file format selected by `SAVE_AS_LDC`.
---
--- - @param filePath Path whose extension matches the configured save format.
--- - @param instance GameInstance object to serialize and save.
---@param filePath string
---@param instance Source.GameInstance.GameInstance
function Save.SaveGame(filePath, instance) end

---@brief Load game state from the file format selected by `SAVE_AS_LDC`.
---
--- - @param filePath Path whose extension matches the configured save format.
--- - @return Restored GameInstance object, or nil if the file doesn't exist.
---@param filePath string
---@return Source.GameInstance.GameInstance | nil
function Save.LoadGame(filePath) end

---@brief Get the platform-specific save file path.
---
--- - @param slot Save slot number (1-based).
---   The `SAVE_AS_LDC` global selects `.ldc`; otherwise `.json` is used.
--- - @return Full path to the save file.
---@param slot integer
---@return string
function Save.GetSavePath(slot) end

---@brief Find the most recently modified existing standard save slot.
---@param maxSlots integer
---@return integer | nil
function Save.FindLatestSlot(maxSlots) end

return Save
