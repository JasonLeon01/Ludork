local Context
local function loadContext()
    if Context == nil then
        Context = require("GlobalFunctions.Context")
    end
    return Context
end

local LocaleCore
local function loadLocaleCore()
    if LocaleCore == nil then
        LocaleCore = require("Source.Locale.Core")
    end
    return LocaleCore
end

local Mota = {}

local function teleportResult(scene, succeeded)
    if succeeded then
        return 0
    end
    scene:addCommonTip(loadLocaleCore().ApplyStringLocaleFormat("FLY_FAIL"))
    return 1
end

function Mota.OpenMonsterBook()
    loadContext().RequireSceneMap():showEnemyBook()
end

function Mota.OpenFloorTeleporter()
    loadContext().RequireSceneMap():showFloorTeleporter()
end

function Mota.GetCurrentRegion()
    return loadContext().RequireGameInstance():getCurrentRegion()
end

function Mota.SetCurrentRegion(region)
    loadContext().RequireGameInstance():setCurrentRegion(region)
end

function Mota.CenterSymmetricTeleport()
    local scene = loadContext().RequireSceneMap()
    return teleportResult(scene, scene:tryCenterSymmetricTeleport())
end

function Mota.GoUpstairsSamePos()
    local scene = loadContext().RequireSceneMap()
    return teleportResult(scene, scene:tryAdjacentFloorSamePos(1))
end

function Mota.GoDownstairsSamePos()
    local scene = loadContext().RequireSceneMap()
    return teleportResult(scene, scene:tryAdjacentFloorSamePos(-1))
end

return Mota
