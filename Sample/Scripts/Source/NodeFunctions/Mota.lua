local Context = require("Source.NodeFunctions.Context")
local LocaleCore = require("Source.Locale.Core")

local LOC = LocaleCore.ApplyStringLocaleFormat

local Mota = {}

local function teleportResult(scene, succeeded)
    if succeeded then
        return 0
    end
    scene:addCommonTip(LOC("FLY_FAIL"))
    return 1
end

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

function Mota.CenterSymmetricTeleport()
    local scene = Context.requireSceneMap()
    return teleportResult(scene, scene:tryCenterSymmetricTeleport())
end

function Mota.GoUpstairsSamePos()
    local scene = Context.requireSceneMap()
    return teleportResult(scene, scene:tryAdjacentFloorSamePos(1))
end

function Mota.GoDownstairsSamePos()
    local scene = Context.requireSceneMap()
    return teleportResult(scene, scene:tryAdjacentFloorSamePos(-1))
end

return Mota
