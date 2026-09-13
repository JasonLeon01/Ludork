---@meta Source.Scenes.SceneMap.Windows

local Windows = {}

--- Register factories and empty focus groups without constructing popup windows.
---@param self Source.Scenes.SceneMap.SceneMap
function Windows.Create(self) end

--- Release existing popup windows and discard unused factories.
---@param self Source.Scenes.SceneMap.SceneMap
function Windows.Dispose(self) end

return Windows
