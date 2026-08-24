---@meta Source.Windows.WindowSaveLoad.Command

---@brief Horizontal load/save command bar; selecting a command picks the slot mode.
---@class Source.Windows.WindowSaveCommand: Source.Windows.WindowCommand
---@field new fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad): Source.Windows.WindowSaveCommand
local WindowSaveCommand = {}

---@brief Construct the save/load command bar.
---
--- - @param rect The window rectangle.
--- - @param owner The parent save/load UI coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveCommand:init(rect, owner) end

function WindowSaveCommand:onReturn() end

return WindowSaveCommand
