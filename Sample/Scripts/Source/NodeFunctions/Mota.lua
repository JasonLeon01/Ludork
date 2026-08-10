local Context = require("Source.NodeFunctions.Context")

local Mota = {}

function Mota.OpenMonsterBook()
    Context.requireSceneMap():showEnemyBook()
end

function Mota.OpenFloorTeleporter()
    Context.requireSceneMap():showFloorTeleporter()
end

function Mota.GetCurrentRegion()
    return Context.requireGameInstance():getCurrentRegion()
end

function Mota.SetCurrentRegion(region)
    Context.requireGameInstance():setCurrentRegion(region)
end

return Mota
