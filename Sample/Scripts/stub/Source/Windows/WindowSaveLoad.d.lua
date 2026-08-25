---@meta Source.Windows.WindowSaveLoad

---@brief Integrated save/load UI: command bar, slot list, and detail panel.
---
--- Owner-agnostic coordinator. Hosts pass callbacks for close and load events
--- instead of being referenced directly, so the same UI can serve the in-game
--- menu, the title screen, or any other entry point.
---@class Source.Windows.WindowSaveLoad
---@field _mode "load" | "save"
---@field new   fun(commandRect?: sf.IntRect, slotRect?: sf.IntRect, detailRect?: sf.IntRect, loadOnly?: boolean, getSaveSource?: function, onClose?: function, onLoaded?: function): Source.Windows.WindowSaveLoad
local WindowSaveLoad = {}

---@brief Construct the save/load UI coordinator and child windows.
---
--- - @param commandRect Rectangle for the load/save command bar (ignored when load-only).
--- - @param slotRect Rectangle for the save slot list window.
--- - @param detailRect Rectangle for the save detail panel.
--- - @param loadOnly When True, no save command is exposed and the slot list opens directly.
--- - @param getSaveSource Callable returning the GameInstance to persist when saving.
--- - @param onClose Callback invoked after the UI closes, with the close reason.
--- - @param onLoaded Callback invoked with the loaded GameInstance after a successful load.
---@param commandRect   sf.IntRect | nil
---@param slotRect      sf.IntRect | nil
---@param detailRect    sf.IntRect | nil
---@param loadOnly      boolean | nil
---@param getSaveSource function | nil
---@param onClose       function | nil
---@param onLoaded      function | nil
function WindowSaveLoad:init(commandRect, slotRect, detailRect, loadOnly, getSaveSource, onClose, onLoaded) end

---@brief Get the horizontal load/save command window.
---
--- - @return The command window instance, or nil when running in load-only mode.
---@return Source.Windows.WindowSaveCommand | nil
function WindowSaveLoad:getCommandWindow() end

---@brief Get the save slot list window.
---
--- - @return The slot list window instance.
---@return Source.Windows.WindowSaveSlot
function WindowSaveLoad:getSlotWindow() end

---@brief Get the save detail panel window.
---
--- - @return The detail panel instance.
---@return Source.Windows.WindowSaveDetail
function WindowSaveLoad:getDetailWindow() end

---@brief Get the visibility of the save/load UI.
---
--- - @return Whether the slot list is visible (treated as the canonical state).
---@return boolean
function WindowSaveLoad:getVisible() end

---@brief Set the visibility of all save/load child windows.
---
--- - @param visible Whether to show or hide the windows.
---@param visible boolean
function WindowSaveLoad:setVisible(visible) end

---@brief Open the save/load UI.
---
--- In load-only mode the slot list is activated directly. Otherwise the
--- command bar is activated first and the user picks load/save before
--- choosing a slot. Every child selector starts at its first item.
function WindowSaveLoad:open() end

---@brief Close the save/load UI and deactivate all child windows.
function WindowSaveLoad:close() end

---@brief Close the save/load UI via cancel and notify the host.
function WindowSaveLoad:closeByCancel() end

---@brief Cancel from the slot list.
---
--- In load-only mode this closes the UI; otherwise focus returns to the
--- command bar so the user can pick a different mode.
---@return boolean
function WindowSaveLoad:cancelSlotSelection() end

---@brief Return focus from the slot list to the command bar.
---
--- - @param playSE Whether to play the cancel sound effect.
--- - @return True if a command window was available and focused.
---@param playSE boolean | nil
---@return boolean
function WindowSaveLoad:returnToCommandWindow(playSE) end

---@brief Confirm the load/save command and switch focus to the slot list.
---
--- - @param mode The selected mode, either ``"load"`` or ``"save"``.
---@param mode "load" | "save"
function WindowSaveLoad:onCommandConfirm(mode) end

---@brief Switch focus from the command bar to the slot list.
function WindowSaveLoad:focusSlotList() end

---@brief Switch focus from the slot list to the command bar.
function WindowSaveLoad:focusCommand() end

---@brief Notify the coordinator that the slot list cursor index may have changed.
---
--- - @param index The current zero-based slot index, or ``nil`` if no selection.
---@param index integer | nil
function WindowSaveLoad:notifySlotIndexMaybeChanged(index) end

---@brief Handle slot confirmation for saving or loading.
---
--- - @param slot Zero-based slot index selected by the player.
---@param slot integer
function WindowSaveLoad:onSlotConfirm(slot) end

return WindowSaveLoad
