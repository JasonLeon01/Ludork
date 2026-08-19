local SceneMap = require("Source.Scenes.SceneMap")

return {
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
    }
}
