---@meta GlobalFunctions.Video

local Video = {}

---@param videoFileName     string
---@param mute              boolean
---@param skipable          boolean
---@param subtitleFileName? string  Full subtitle asset path; omitted or empty disables subtitles.
function Video.PlayVideo(videoFileName, mute, skipable, subtitleFileName) end

return Video
