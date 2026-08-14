---@meta Source.Windows.WindowSaveLoad

---@class Source.Windows.SaveFileMTime
---@field [1] integer
---@field [2] number

---@param owner Source.Windows.WindowSaveLoad
---@return Source.UI.Helpers.CommandRowModel[]
function WindowSaveCommandController.createCommands(owner) end

---@class Source.Windows.WindowSaveLoadExports
---@field WindowSaveCommand Source.Windows.WindowSaveCommand & Class.ClassType<Source.Windows.WindowSaveCommand>
---@field WindowSaveSlot    Source.Windows.WindowSaveSlot & Class.ClassType<Source.Windows.WindowSaveSlot>
---@field WindowSaveDetail  Source.Windows.WindowSaveDetail & Class.ClassType<Source.Windows.WindowSaveDetail>
---@field WindowSaveLoad    Source.Windows.WindowSaveLoad & Class.ClassType<Source.Windows.WindowSaveLoad>
local WindowSaveLoadExports = {}

--- @brief Horizontal load/save command bar; selecting a command picks the slot mode.
---@class Source.Windows.WindowSaveCommand: Source.Windows.WindowCommand
local WindowSaveCommand = {}

--- @brief Construct the save/load command bar.
---
--- - @param rect The window rectangle.
--- - @param owner The parent save/load UI coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveCommand:init(rect, owner) end

---@param kwargs table
function WindowSaveCommand:onKeyDown(kwargs) end

---@param kwargs table
---@return boolean
function WindowSaveCommand:onMouseButtonDown(kwargs) end

--- @brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection.
---@class Source.Windows.WindowSaveSlot: Source.Windows.Base.WindowSelectable
local WindowSaveSlot = {}

--- @brief Construct the save slot list window.
---
--- - @param rect The window rectangle.
--- - @param owner The parent save/load UI coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveSlot:init(rect, owner) end

---@param kwargs table
function WindowSaveSlot:onKeyDown(kwargs) end

---@param deltaTime number
function WindowSaveSlot:onTick(deltaTime) end

---@param kwargs table
---@return boolean
function WindowSaveSlot:onMouseButtonDown(kwargs) end

--- @brief Save-file detail panel showing the current slot's screenshot and timestamp.
---
--- Renders the snapshot horizontally filling the content area at a 4:3 ratio
--- and displays the file's last-modified timestamp underneath. When the slot
--- has no save file on disk, both the snapshot and timestamp stay hidden.
---@class Source.Windows.WindowSaveDetail: Source.Windows.Base.WindowBase
local WindowSaveDetail = {}

--- @brief Construct the detail panel.
---
--- - @param rect The window rectangle (expected 256x256).
---@param rect sf.IntRect
function WindowSaveDetail:init(rect) end

--- @brief Set the slot index to display, or ``nil`` to clear the panel.
---
--- - @param slot Zero-based slot index or ``nil``.
---@param slot integer | nil
function WindowSaveDetail:setSlot(slot) end

--- @brief Force-refresh the panel against the current slot's save file.
function WindowSaveDetail:refresh() end

---@param deltaTime number
function WindowSaveDetail:onTick(deltaTime) end

--- @brief Integrated save/load UI: command bar, slot list, and detail panel.
---
--- Owner-agnostic coordinator. Hosts pass callbacks for close and load events
--- instead of being referenced directly, so the same UI can serve the in-game
--- menu, the title screen, or any other entry point.
---@class Source.Windows.WindowSaveLoad
---@field _mode "load" | "save"
local WindowSaveLoad = {}

--- @brief Construct the save/load UI coordinator and child windows.
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

--- @brief Get the horizontal load/save command window.
---
--- - @return The command window instance, or nil when running in load-only mode.
---@return Source.Windows.WindowSaveCommand | nil
function WindowSaveLoad:getCommandWindow() end

--- @brief Get the save slot list window.
---
--- - @return The slot list window instance.
---@return Source.Windows.WindowSaveSlot
function WindowSaveLoad:getSlotWindow() end

--- @brief Get the save detail panel window.
---
--- - @return The detail panel instance.
---@return Source.Windows.WindowSaveDetail
function WindowSaveLoad:getDetailWindow() end

--- @brief Get the visibility of the save/load UI.
---
--- - @return Whether the slot list is visible (treated as the canonical state).
---@return boolean
function WindowSaveLoad:getVisible() end

--- @brief Set the visibility of all save/load child windows.
---
--- - @param visible Whether to show or hide the windows.
---@param visible boolean
function WindowSaveLoad:setVisible(visible) end

--- @brief Open the save/load UI.
---
--- In load-only mode the slot list is activated directly. Otherwise the
--- command bar is activated first and the user picks load/save before
--- choosing a slot.
function WindowSaveLoad:open() end

--- @brief Close the save/load UI and deactivate all child windows.
function WindowSaveLoad:close() end

--- @brief Close the save/load UI via cancel and notify the host.
function WindowSaveLoad:closeByCancel() end

--- @brief Cancel from the slot list.
---
--- In load-only mode this closes the UI; otherwise focus returns to the
--- command bar so the user can pick a different mode.
---@return boolean
function WindowSaveLoad:cancelSlotSelection() end

--- @brief Return focus from the slot list to the command bar.
---
--- - @param playSE Whether to play the cancel sound effect.
--- - @return True if a command window was available and focused.
---@param playSE boolean | nil
---@return boolean
function WindowSaveLoad:returnToCommandWindow(playSE) end

--- @brief Confirm the load/save command and switch focus to the slot list.
---
--- - @param mode The selected mode, either ``"load"`` or ``"save"``.
---@param mode "load" | "save"
function WindowSaveLoad:onCommandConfirm(mode) end

--- @brief Switch focus from the command bar to the slot list.
function WindowSaveLoad:focusSlotList() end

--- @brief Switch focus from the slot list to the command bar.
function WindowSaveLoad:focusCommand() end

--- @brief Notify the coordinator that the slot list cursor index may have changed.
---
--- - @param index The current zero-based slot index, or ``nil`` if no selection.
---@param index integer | nil
function WindowSaveLoad:notifySlotIndexMaybeChanged(index) end

--- @brief Handle slot confirmation for saving or loading.
---
--- - @param slot Zero-based slot index selected by the player.
---@param slot integer
function WindowSaveLoad:onSlotConfirm(slot) end

return WindowSaveLoadExports
