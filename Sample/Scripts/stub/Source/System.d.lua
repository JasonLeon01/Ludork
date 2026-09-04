---@meta Source.System

---@brief Game system bootstrap that initialises engine subsystems.
---@class Source.System.Module
---@field Init                    fun()
---@field GetConfigValue          fun(configName: string, settingName: string): string
---@field GetTitle                fun(): string
---@field GetFonts                fun(): sf.Font[]
---@field GetFontSize             fun(): integer
---@field GetWindowskinName       fun(): string
---@field SetWindowskinName       fun(name: string)
---@field GetDecisionSE           fun(): string
---@field GetCancelSE             fun(): string
---@field GetBuzzerSE             fun(): string
---@field GetCursorSE             fun(): string
---@field GetShopSE               fun(): string
---@field GetSaveSE               fun(): string
---@field GetLoadSE               fun(): string
---@field GetGateSE               fun(): string
---@field GetStairSE              fun(): string
---@field GetGetSE                fun(): string
---@field GetEquipSE              fun(): string
---@field GetTitleBGM             fun(): string
---@field GetTitleBackgroundFile  fun(): string
---@field GetStartMap             fun(): string
---@field GetStartPlayerClassPath fun(): string
---@field GetStartRegion          fun(): string
---@field GetStartPos             fun(): sf.Vector2u
---@field GetSavedScreenImage     fun(): sf.Image | nil
---@field SetSavedScreenImage     fun(image: sf.Image | nil)
local System = {}

---@brief Initialise the game system from configuration files.
---
--- Loads system.json, sets up the window, fonts, cursor, and global settings.
function System.Init() end

---@brief Get a runtime config value by config file and setting key.
---
--- - @param configName The config data name, such as `Audio`.
--- - @param settingName The setting key inside that config.
--- - @return The resolved config value, or an empty string when missing.
---@param configName  string
---@param settingName string
---@return string
function System.GetConfigValue(configName, settingName) end

---@brief Get the game window title.
---
--- - @return The window title string.
---@return string
function System.GetTitle() end

---@brief Get the list of loaded fonts.
---
--- - @return A list of Font objects.
---@return sf.Font[]
function System.GetFonts() end

---@brief Get the default font size.
---
--- - @return The font size in pixels.
---@return integer
function System.GetFontSize() end

---@brief Get the window skin texture resource path.
---
--- - @return The canonical windowskin path under `/Game/Assets/System`.
---@return string
function System.GetWindowskinName() end

---@brief Get the title screen background texture resource path.
---
--- - @return The canonical title background path under `/Game/Assets/System`.
---@return string
function System.GetTitleBackgroundFile() end

---@brief Set the window skin texture resource path.
---
--- - @param name The new canonical windowskin path.
---@param name string
function System.SetWindowskinName(name) end

---@brief Get the starting map path.
---
--- - @return The start map path.
---@return string
function System.GetStartMap() end

---@brief Get the configured new-game player Blueprint class path.
---
--- - @return The `Data.Blueprints.*` class path derived from `startPlayerBlueprint`.
---@return string
function System.GetStartPlayerClassPath() end

---@brief Get the configured new-game region.
---
--- - @return The initial region name used by a new GameInstance.
---@return string
function System.GetStartRegion() end

---@brief Get the starting position on the map.
---
--- - @return The start position.
---@return sf.Vector2u
function System.GetStartPos() end

---@brief Get the cursor sound effect resource path.
---
--- - @return The canonical cursor SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetCursorSE() end

---@brief Get the decision sound effect resource path.
---
--- - @return The canonical decision SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetDecisionSE() end

---@brief Get the cancel sound effect resource path.
---
--- - @return The canonical cancel SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetCancelSE() end

---@brief Get the buzzer sound effect resource path.
---
--- - @return The canonical buzzer SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetBuzzerSE() end

---@brief Get the shop sound effect resource path.
---
--- - @return The canonical shop SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetShopSE() end

---@brief Get the save sound effect resource path.
---
--- - @return The canonical save SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetSaveSE() end

---@brief Get the load sound effect resource path.
---
--- - @return The canonical load SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetLoadSE() end

---@brief Get the gate sound effect resource path.
---
--- - @return The canonical gate SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetGateSE() end

---@brief Get the stair sound effect resource path.
---
--- - @return The canonical stair SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetStairSE() end

---@brief Get the item get sound effect resource path.
---
--- - @return The canonical get SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetGetSE() end

---@brief Get the equip sound effect resource path.
---
--- - @return The canonical equip SE path under `/Game/Assets/Sounds`.
---@return string
function System.GetEquipSE() end

---@brief Get the title screen BGM resource path.
---
--- - @return The canonical title BGM path under `/Game/Assets/Musics`.
---@return string
function System.GetTitleBGM() end

---@brief Get the most recently captured screen snapshot.
---
--- - @return The captured Image scaled to game size, or nil if no snapshot exists.
---@return sf.Image | nil
function System.GetSavedScreenImage() end

---@brief Set the captured screen snapshot used for save thumbnails.
---
--- - @param image The captured Image scaled to game size, or nil to clear.
---@param image sf.Image | nil
function System.SetSavedScreenImage(image) end

return System
