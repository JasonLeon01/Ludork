---@meta Source.Windows.WindowSaveLoad

---@brief Integrated save/load UI: tabs, slot list, and detail panel.
---
--- Owner-agnostic coordinator. Hosts pass callbacks for close and load events
--- instead of being referenced directly, so the same UI can serve the in-game
--- menu, the title screen, or any other entry point.
---@class Source.Windows.WindowSaveLoad: Engine.Canvas
---@field _mode      "load" | "save"
---@field _tabWindow Source.Windows.WindowSaveTabs | nil
---@field new        fun(tabRect?: sf.IntRect, slotRect?: sf.IntRect, detailRect?: sf.IntRect, loadOnly?: boolean, getSaveSource?: function, onClose?: function, onLoaded?: function): Source.Windows.WindowSaveLoad
local WindowSaveLoad = {}

---@brief Construct the save/load UI coordinator and child windows.
---
--- - @param tabRect Rectangle for the load/save tabs (ignored when load-only).
--- - @param slotRect Rectangle for the save slot list window.
--- - @param detailRect Rectangle for the save detail panel.
--- - @param loadOnly When True, tabs are omitted and the slot list remains in load mode.
--- - @param getSaveSource Callable returning the GameInstance to persist when saving.
--- - @param onClose Callback invoked after the UI closes, with the close reason.
--- - @param onLoaded Callback invoked with the loaded GameInstance after a successful load.
---@param tabRect       sf.IntRect | nil
---@param slotRect      sf.IntRect | nil
---@param detailRect    sf.IntRect | nil
---@param loadOnly      boolean | nil
---@param getSaveSource function | nil
---@param onClose       function | nil
---@param onLoaded      function | nil
function WindowSaveLoad:init(tabRect, slotRect, detailRect, loadOnly, getSaveSource, onClose, onLoaded) end

---@brief Get the load/save tab window.
---
--- - @return The tab window instance, or nil when running in load-only mode.
---@return Source.Windows.WindowSaveTabs | nil
function WindowSaveLoad:getTabWindow() end

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

---@brief Open the save/load UI in Load mode with the slot list focused.
---
--- Selects the latest existing save, or the first slot when none exists.
function WindowSaveLoad:open() end

---@brief Close the save/load UI and deactivate all child windows.
function WindowSaveLoad:close() end

---@brief Close the save/load UI via cancel and notify the host.
function WindowSaveLoad:closeByCancel() end

---@brief Delegate Q/E and LB/RB navigation to the tab view when present.
---@return boolean
function WindowSaveLoad:handleTabNavigationInput() end

---@brief Apply a zero-based tab selection without changing slot cursor or scroll state.
---@param index integer
function WindowSaveLoad:onTabSelected(index) end

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

---@brief Release tab callbacks, UI subscriptions, and host callbacks.
function WindowSaveLoad:dispose() end

return WindowSaveLoad
