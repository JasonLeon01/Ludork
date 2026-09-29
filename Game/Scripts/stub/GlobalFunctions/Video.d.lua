---@meta GlobalFunctions.Video

local Video = {}

---@param videoFileName     string
---@param mute              boolean
---@param skipable          boolean
---@param subtitleFileName? string  Project-relative Data/Subtitles/...json path; omitted or empty disables subtitles.
function Video.PlayVideo(videoFileName, mute, skipable, subtitleFileName) end

return Video
