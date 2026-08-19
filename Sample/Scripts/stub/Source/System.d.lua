---@meta Source.System

--- @brief Game system bootstrap that initialises engine subsystems.
---@class Source.System.Module
---@field init                     fun()
---@field getConfigValue           fun(configName: string, settingName: string): string
---@field getTitle                 fun(): string
---@field getFonts                 fun(): sf.Font[]
---@field getFontSize              fun(): integer
---@field getDecisionSE            fun(): sf.SoundBuffer
---@field getCancelSE              fun(): sf.SoundBuffer
---@field getBuzzerSE              fun(): sf.SoundBuffer
---@field getCursorSE              fun(): sf.SoundBuffer
---@field getSaveSE                fun(): sf.SoundBuffer
---@field getLoadSE                fun(): sf.SoundBuffer
---@field getGetSE                 fun(): sf.SoundBuffer
---@field getTitleBGM              fun(): string
---@field getTitleBackgroundFile   fun(): string
---@field getStartMap              fun(): string
---@field GetStartPlayerClassPath  fun(): string
---@field GetStartRegion           fun(): string
---@field getStartPos              fun(): sf.Vector2u
---@field getSavedScreenImage      fun(): sf.Image | nil
---@field setSavedScreenImage      fun(image: sf.Image | nil)
local System = {}

--- @brief Initialise the game system from configuration files.
---
--- Loads system.json, sets up the window, fonts, cursor, and global settings.
function System.init() end

--- @brief Get a runtime config value by config file and setting key.
---
--- - @param configName The config data name, such as `Audio`.
--- - @param settingName The setting key inside that config.
--- - @return The resolved config value, or an empty string when missing.
---@param configName  string
---@param settingName string
---@return string
function System.getConfigValue(configName, settingName) end

--- @brief Get the game window title.
---
--- - @return The window title string.
---@return string
function System.getTitle() end

--- @brief Get the list of loaded fonts.
---
--- - @return A list of Font objects.
---@return sf.Font[]
function System.getFonts() end

--- @brief Get the default font size.
---
--- - @return The font size in pixels.
---@return integer
function System.getFontSize() end

--- @brief Get the window skin texture name.
---
--- - @return The windowskin name.
---@return string
function System.getWindowskinName() end

--- @brief Get the title screen background texture name.
---
--- - @return The title background file name.
---@return string
function System.getTitleBackgroundFile() end

--- @brief Set the window skin texture name.
---
--- - @param name The new windowskin name.
---@param name string
function System.setWindowskinName(name) end

--- @brief Get the starting map path.
---
--- - @return The start map path.
---@return string
function System.getStartMap() end

--- @brief Get the configured new-game player Blueprint class path.
---
--- - @return The `Data.Blueprints.*` class path derived from `startPlayerBlueprint`.
---@return string
function System.GetStartPlayerClassPath() end

--- @brief Get the configured new-game region.
---
--- - @return The initial region name used by a new GameInstance.
---@return string
function System.GetStartRegion() end

--- @brief Get the starting position on the map.
---
--- - @return The start position.
---@return sf.Vector2u
function System.getStartPos() end

--- @brief Get the cursor sound effect filename.
---
--- - @return The cursor SE filename.
---@return string
function System.getCursorSE() end

--- @brief Get the decision sound effect filename.
---
--- - @return The decision SE filename.
---@return string
function System.getDecisionSE() end

--- @brief Get the cancel sound effect filename.
---
--- - @return The cancel SE filename.
---@return string
function System.getCancelSE() end

--- @brief Get the buzzer sound effect filename.
---
--- - @return The buzzer SE filename.
---@return string
function System.getBuzzerSE() end

--- @brief Get the shop sound effect filename.
---
--- - @return The shop SE filename.
---@return string
function System.getShopSE() end

--- @brief Get the save sound effect filename.
---
--- - @return The save SE filename.
---@return string
function System.getSaveSE() end

--- @brief Get the load sound effect filename.
---
--- - @return The load SE filename.
---@return string
function System.getLoadSE() end

--- @brief Get the gate sound effect filename.
---
--- - @return The gate SE filename.
---@return string
function System.getGateSE() end

--- @brief Get the stair sound effect filename.
---
--- - @return The stair SE filename.
---@return string
function System.getStairSE() end

--- @brief Get the item get sound effect filename.
---
--- - @return The get SE filename.
---@return string
function System.getGetSE() end

--- @brief Get the equip sound effect filename.
---
--- - @return The equip SE filename.
---@return string
function System.getEquipSE() end

--- @brief Get the title screen BGM filename.
---
--- - @return The title BGM filename.
---@return string
function System.getTitleBGM() end

--- @brief Get the most recently captured screen snapshot.
---
--- - @return The captured Image scaled to game size, or nil if no snapshot exists.
---@return sf.Image | nil
function System.getSavedScreenImage() end

--- @brief Set the captured screen snapshot used for save thumbnails.
---
--- - @param image The captured Image scaled to game size, or nil to clear.
---@param image sf.Image | nil
function System.setSavedScreenImage(image) end

return System
