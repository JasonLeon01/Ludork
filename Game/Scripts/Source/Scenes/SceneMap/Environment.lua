local GlobalCore = require("GlobalCore")
local MapAudio = require("Global.Audio.MapAudio")

local AudioManager = GlobalCore.AudioManager
local PanoramaController = GlobalCore.PanoramaController
local WeatherController = GlobalCore.WeatherController

local WORLD_AMBIENT_TRANSITION_TIME = 0.5

---@param from  sf.Color
---@param to    sf.Color
---@param alpha number
---@return sf.Color
local function interpolateColour(from, to, alpha)
    return sf.Color.new(
        math.round(math.lerp(from.r, to.r, alpha)), math.round(math.lerp(from.g, to.g, alpha)),
        math.round(math.lerp(from.b, to.b, alpha)), math.round(math.lerp(from.a, to.a, alpha))
    )
end

---@class Source.Scenes.SceneMap.Environment
local Environment = {
    _MAP_AUDIO_TRANSITION_TIME = 0.5,
    _ENVIRONMENT_AVAILABILITY = { LOADED = "loaded", EMPTY = "empty" }
}

function Environment:init()
    self._audio = MapAudio.new()
    self._mapAudioData = nil
    self._mapPanorama = nil
    self._needsRestore = false
    self._worldEnvironmentKey = nil
    self._ambientStart = nil
    self._ambientTarget = nil
    self._ambientElapsed = 0
end

function Environment:applyMap(gameMap, mapData)
    self._mapAudioData = nil
    self._mapPanorama = mapData.panorama
    self._worldEnvironmentKey = nil
    self._ambientStart = nil
    self._ambientTarget = nil
    self._ambientElapsed = 0
    if not gameMap:isWorldMap() then
        ---@cast mapData Global.GameMap.MapData
        self._mapAudioData = deepcopy({
            bgm = mapData.bgm,
            bgs = mapData.bgs,
            bgmFilter = mapData.bgmFilter,
            bgsFilter = mapData.bgsFilter
        })
        self._audio:playMapAudio(self._mapAudioData)
        gameMap:setFogSettings(mapData.fog)
        PanoramaController.clear()
        PanoramaController.applyFromMapData(GlobalCore.MapPanoramaSettings.new({ panorama = mapData.panorama }))
    end
end

function Environment:enter(gameMap, player)
    if not self._needsRestore then
        return
    end
    AudioManager.stopMusic("BGM")
    AudioManager.stopMusic("BGS")
    WeatherController.clearWeather()
    local panorama = GlobalCore.MapPanoramaSettings.new({ panorama = self._mapPanorama })
    if gameMap:isWorldMap() then
        PanoramaController.applyWorldFromMapData(panorama)
        self:updateWorld(gameMap, player, 0, true)
    else
        PanoramaController.applyFromMapData(panorama)
        self._audio:playMapAudio(assert(self._mapAudioData))
    end
    self._needsRestore = false
end

function Environment:quit()
    self._audio:stopMapAudio()
    WeatherController.clearWeather()
    PanoramaController.clear()
    self._needsRestore = true
end

function Environment:updateAudio(deltaTime)
    self._audio:onTick(deltaTime)
end

function Environment:updateWorld(gameMap, player, deltaTime, force)
    if not gameMap:isWorldMap() then
        return
    end
    ---@cast gameMap Global.WorldGameMap.WorldGameMap
    local region = gameMap:getRegionPosition(player:getMapPosition())
    local regionPath = region ~= nil and region.path or ""
    local mapData = gameMap:getEnvironmentDataAt(player:getMapPosition())
    local environmentKey = regionPath .. ":"
        .. (mapData ~= nil and Environment._ENVIRONMENT_AVAILABILITY.LOADED
            or Environment._ENVIRONMENT_AVAILABILITY.EMPTY)
    if force or self._worldEnvironmentKey ~= environmentKey then
        self._worldEnvironmentKey = environmentKey
        local targetColour
        if mapData == nil then
            self._audio:playMapAudio({}, Environment._MAP_AUDIO_TRANSITION_TIME)
            targetColour = sf.Color.new(255, 255, 255, 255)
        else
            self._audio:playMapAudio(mapData, Environment._MAP_AUDIO_TRANSITION_TIME)
            targetColour = mapData.ambientLight
        end
        if self._ambientTarget == nil then
            self._ambientTarget = targetColour:copy()
            gameMap:setAmbientLight(self._ambientTarget)
        elseif self._ambientTarget:toInteger() ~= targetColour:toInteger() then
            self._ambientStart = gameMap:getAmbientLight():copy()
            self._ambientTarget = targetColour:copy()
            self._ambientElapsed = 0
        end
    end
    if self._ambientStart == nil then
        return
    end
    self._ambientElapsed = math.clamp(self._ambientElapsed + deltaTime, 0.0, WORLD_AMBIENT_TRANSITION_TIME)
    local alpha = self._ambientElapsed / WORLD_AMBIENT_TRANSITION_TIME
    gameMap:setAmbientLight(interpolateColour(self._ambientStart, assert(self._ambientTarget), alpha))
    if self._ambientElapsed >= WORLD_AMBIENT_TRANSITION_TIME then
        self._ambientStart = nil
    end
end

function Environment:playBgm(bgm, bgmFilter)
    self._audio:playBgm(bgm, bgmFilter)
end

function Environment:setBgmFilter(attr, value)
    self._audio:setBgmFilter(attr, value)
end

function Environment:setBgsFilter(attr, value)
    self._audio:setBgsFilter(attr, value)
end

function Environment:dispose()
    self._audio:stopMapAudio()
end

return class(Environment)
