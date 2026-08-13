---@meta Source.NodeFunctions.System

---@alias Source.NodeFunctions.System.SoundFilterValue boolean | number | sf.Time | sf.Vector3f | sf.SoundSource.Cone | sf.Music.TimeSpan | nil

---@class Source.NodeFunctions.System.TransitionCondition
---@field _started boolean
---@operator call: boolean
local TransitionCondition = {}

function TransitionCondition:init() end

---@return boolean
function TransitionCondition:poll() end

---@return boolean
function TransitionCondition:isFinished() end

---@class Source.NodeFunctions.System.FrozenCondition
---@operator call: boolean
local FrozenCondition = {}

---@return boolean
function FrozenCondition:poll() end

---@return boolean
function FrozenCondition:isFinished() end

---@param attr  string
---@param value Source.NodeFunctions.System.SoundFilterValue
function System.EditSoundFilter(attr, value) end

---@param attr  string
---@param value Source.NodeFunctions.System.SoundFilterValue
function System.EditMusicFilter(attr, value) end

---Select a native audio-effect preset for future playback in one category.
---Use the literal string `nil` to clear the category preset.
---@param audioType string
---@param effect    string
function System.SetEffect(audioType, effect) end

---@param soundFileName string
---@param applyFilter   boolean
function System.PlaySound(soundFileName, applyFilter) end

---@param musicFileName string
---@param applyFilter   boolean
function System.PlayMusic(musicFileName, applyFilter) end

---@param videoFileName string
---@param mute          boolean
---@param skipable      boolean
function System.PlayVideo(videoFileName, mute, skipable) end

--- @brief Freeze the current frame and wait until it is ready for a transition.
---
--- - @return A condition callable that becomes True when the frame has been captured.
---@return Source.NodeFunctions.System.FrozenCondition
function System.FreezeTransitionBackground() end

--- @brief Request a screen transition and wait until it finishes.
---
--- - @param transitionName Optional transition texture filename.
--- - @param transitionTime Transition duration in seconds.
--- - @return A condition callable that becomes True when the transition is finished.
---@param transitionName string
---@param transitionTime number
---@return Source.NodeFunctions.System.TransitionCondition
function System.RequestTransition(transitionName, transitionTime) end

---@param attr  string
---@param value Source.NodeFunctions.System.SoundFilterValue
function System.SetBgmFilter(attr, value) end

---@param attr  string
---@param value Source.NodeFunctions.System.SoundFilterValue
function System.SetBgsFilter(attr, value) end

---@param red      integer
---@param green    integer
---@param blue     integer
---@param alpha    integer
---@param duration number
function System.FlashScreen(red, green, blue, alpha, duration) end

function System.StopFlashScreen() end

---@param red      integer
---@param green    integer
---@param blue     integer
---@param gray     integer
---@param duration number
function System.ChangeScreenTone(red, green, blue, gray, duration) end

---@param duration number
function System.ClearScreenTone(duration) end

---@param power    number
---@param speed    number
---@param duration number
function System.ScreenShake(power, speed, duration) end

function System.StopScreenShake() end

---@param weatherType integer | string
---@param power       integer
---@param maxCount    integer
function System.SetWeather(weatherType, power, maxCount) end

function System.ClearWeather() end

return System
