local SceneMap = require("Source.Scenes.SceneMap")

---@cast SceneMap Source.Scenes.SceneMap.SceneMap & Class.ClassType<Source.Scenes.SceneMap.SceneMap>

---@class Source.Configs.HotKey.Binding
---@field Scene                Class.ClassType<Source.Scenes.SceneMap.SceneMap>
---@field Filter               string[] | nil
---@field FunctionWhenPressed  (fun(scene: Source.Scenes.SceneMap.SceneMap): boolean | nil) | nil
---@field FunctionWhenReleased (fun(scene: Source.Scenes.SceneMap.SceneMap): boolean | nil) | nil

---@type table<sf.Keyboard.Key, Source.Configs.HotKey.Binding>
local HotKey = {
    [sf.Keyboard.Key.Escape] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.openMenu,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.D] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = nil,
        FunctionWhenReleased = SceneMap.showEnemyBook
    },
    [sf.Keyboard.Key.G] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = nil,
        FunctionWhenReleased = SceneMap.showFloorTeleporter
    },
    [sf.Keyboard.Key.V] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = nil,
        FunctionWhenReleased = SceneMap.showShopRecords
    },
    [sf.Keyboard.Key.S] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.openSaveUI,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.L] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.openLoadUI,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.I] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.openItemUI,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.E] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.openEquipUI,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.LBracket] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.quickSave,
        FunctionWhenReleased = nil
    },
    [sf.Keyboard.Key.RBracket] = {
        Scene = SceneMap,
        Filter = { "casual" },
        FunctionWhenPressed = SceneMap.quickLoad,
        FunctionWhenReleased = nil
    }
}

return HotKey
