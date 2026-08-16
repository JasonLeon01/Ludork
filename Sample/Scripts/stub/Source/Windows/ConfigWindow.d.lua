---@meta Source.Windows.ConfigWindow
---
--- Each setting row combines a label and an interactive control.
---@class Source.Windows.ConfigWindow: Source.Windows.Base.WindowSelectable
---@field _open boolean
---@field _capturedTouchSlider Engine.Slider?
---@field _capturedTouchSliderIndex integer?
---@field _capturedTouchOwner "list" | "slider" | nil
local ConfigWindow = {}

---@param onClose function | nil
---@return Source.Windows.ConfigWindow
function ConfigWindow.new(onClose) end

--- @brief Construct the configuration window.
---
--- - @param rect        Window rectangle in logical UI units
--- - @param windowSkin  Optional windowskin image
--- - @param onClose     Optional callback when the window is closed
---@param onClose function | nil
function ConfigWindow:init(onClose) end

--- @brief Get the language DropBox on the first settings row.
---
--- - @return  Language DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getLanguageDropBox() end

--- @brief Get the scale DropBox on the scale settings row.
---
--- Mobile builds do not create this row.
---
--- - @return  Scale DropBox coordinator, or nil on mobile
---@return Engine.DropBox | nil
function ConfigWindow:getScaleDropBox() end

--- @brief Get the framerate DropBox on the framerate settings row.
---
--- - @return  Framerate DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getFramerateDropBox() end

--- @brief Get the vertical-sync CheckBox on the settings list.
---
--- - @return  Vertical-sync CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getVerticalSyncCheckBox() end

--- @brief Get the music-enabled CheckBox on the settings list.
---
--- - @return  Music-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getMusicOnCheckBox() end

--- @brief Get the music-volume Slider on the settings list.
---
--- - @return  Music-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getMusicVolumeSlider() end

--- @brief Get the sound-enabled CheckBox on the settings list.
---
--- - @return  Sound-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getSoundOnCheckBox() end

--- @brief Get the sound-volume Slider on the settings list.
---
--- - @return  Sound-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getSoundVolumeSlider() end

--- @brief Get the voice-enabled CheckBox on the settings list.
---
--- - @return  Voice-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getVoiceOnCheckBox() end

--- @brief Get the voice-volume Slider on the settings list.
---
--- - @return  Voice-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getVoiceVolumeSlider() end

--- @brief Check whether this window is currently open.
---
--- - @return  True if open, False otherwise
---@return boolean
function ConfigWindow:isOpen() end

--- @brief Show and activate the configuration window.
function ConfigWindow:open() end

--- @brief Hide and deactivate the configuration window.
function ConfigWindow:close() end

function ConfigWindow:dispose() end

--- @brief Cancel an expanded DropBox, or close the configuration window.
function ConfigWindow:onReturn() end

--- @brief Update the window, delegating input to an expanded DropBox when needed.
---
--- - @param deltaTime  Elapsed time in seconds
---@param deltaTime number
function ConfigWindow:onTick(deltaTime) end

--- @brief Close the window on cancel when no DropBox is expanded.
---
--- - @param kwargs  Event arguments
---@param kwargs table
function ConfigWindow:onKeyDown(kwargs) end

--- @brief Close the window on right-click.
---@param kwargs table
---@return boolean
function ConfigWindow:onMouseButtonDown(kwargs) end

---@param position sf.Vector2f
function ConfigWindow:_onCapturedTouchBegan(position) end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchDrag(position) end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchTap(position) end

function ConfigWindow:_onCapturedTouchReset() end

return ConfigWindow
