---@meta Source.Configs.HotKey

---@class Source.Configs.HotKey.Binding
---@field Scene                Class.ClassType<Source.Scenes.SceneMap.SceneMap>
---@field Filter               string[] | nil
---@field FunctionWhenPressed  fun(scene: Source.Scenes.SceneMap.SceneMap) | nil
---@field FunctionWhenReleased fun(scene: Source.Scenes.SceneMap.SceneMap) | nil

---@type table<sf.Keyboard.Key, Source.Configs.HotKey.Binding>
local HotKey = {}

return HotKey
