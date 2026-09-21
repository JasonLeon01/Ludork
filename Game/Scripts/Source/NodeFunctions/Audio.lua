local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local AudioEffects = require("Source.AudioEffects")
local Context = require("Source.NodeFunctions.Context")

local AudioManager = GlobalCore.AudioManager
local SceneManager = GlobalCore.SceneManager
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

Audio.SoundFilter = constructSoundFilter(Engine.SoundFilter.new)
Audio.MusicFilter = constructMusicFilter(Engine.MusicFilter.new)

function Audio.EditSoundFilter(attr, value)
    Audio.SoundFilter[attr] = value
end

function Audio.EditMusicFilter(attr, value)
    Audio.MusicFilter[attr] = value
end

function Audio.SetEffect(audioType, effect)
    AudioManager.setEffect(audioType, AudioEffects.Get(effect))
end

function Audio.PlaySound(soundFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    if applyFilter then
        AudioManager.playSound(soundFileName, Audio.SoundFilter)
    else
        AudioManager.playSound(soundFileName)
    end
end

function Audio.PlayMusic(musicFileName, applyFilter)
    applyFilter = applyFilter == nil and false or applyFilter
    local scene = SceneManager.getScene()
    local musicFilter = applyFilter and Audio.MusicFilter or nil
    if bool(scene) then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        scene:playBgm(musicFileName, musicFilter)
        return
    end
    local music = AudioManager.playMusic("BGM", musicFileName, musicFilter)
    if music ~= nil then
        music:setLooping(true)
    end
end

function Audio.SetBgmFilter(attr, value)
    Context.RequireSceneMap():setBgmFilter(attr, value)
end

function Audio.SetBgsFilter(attr, value)
    Context.RequireSceneMap():setBgsFilter(attr, value)
end

return Audio
