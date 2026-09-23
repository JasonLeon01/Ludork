local Engine
local function loadEngine()
    if Engine == nil then
        Engine = require("Engine")
    end
    return Engine
end

local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local AudioEffects
local function loadAudioEffects()
    if AudioEffects == nil then
        AudioEffects = require("Source.Utils.AudioEffects")
    end
    return AudioEffects
end

local Context
local function loadContext()
    if Context == nil then
        Context = require("GlobalFunctions.Context")
    end
    return Context
end

local Audio = {}

---@param constructor fun(values: table): Engine.SoundFilter
---@return Engine.SoundFilter
local function constructSoundFilter(constructor)
    ---@cast constructor function
    return constructor()
end

---@param constructor fun(values: table): Engine.MusicFilter
---@return Engine.MusicFilter
local function constructMusicFilter(constructor)
    ---@cast constructor function
    return constructor()
end

local function ensureFilters()
    if Audio.SoundFilter == nil then
        Audio.SoundFilter = constructSoundFilter(loadEngine().SoundFilter.new)
    end
    if Audio.MusicFilter == nil then
        Audio.MusicFilter = constructMusicFilter(loadEngine().MusicFilter.new)
    end
end

function Audio.EditSoundFilter(attr, value)
    ensureFilters()
    Audio.SoundFilter[attr] = value
end

function Audio.EditMusicFilter(attr, value)
    ensureFilters()
    Audio.MusicFilter[attr] = value
end

function Audio.SetEffect(audioType, effect)
    loadGlobalCore().AudioManager.setEffect(audioType, loadAudioEffects().Get(effect))
end

function Audio.PlaySound(soundFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    if applyFilter then
        ensureFilters()
        loadGlobalCore().AudioManager.playSound(soundFileName, Audio.SoundFilter)
    else
        loadGlobalCore().AudioManager.playSound(soundFileName)
    end
end

function Audio.PlayMusic(musicFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    if applyFilter then
        ensureFilters()
    end
    local scene = loadGlobalCore().SceneManager.getScene()
    local musicFilter = applyFilter and Audio.MusicFilter or nil
    if bool(scene) then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        scene:playBgm(musicFileName, musicFilter)
        return
    end
    local music = loadGlobalCore().AudioManager.playMusic("BGM", musicFileName, musicFilter)
    if music ~= nil then
        music:setLooping(true)
    end
end

function Audio.SetBgmFilter(attr, value)
    loadContext().RequireSceneMap():setBgmFilter(attr, value)
end

function Audio.SetBgsFilter(attr, value)
    loadContext().RequireSceneMap():setBgsFilter(attr, value)
end

return Audio
