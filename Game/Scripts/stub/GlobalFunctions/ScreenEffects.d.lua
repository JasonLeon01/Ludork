---@meta GlobalFunctions.ScreenEffects

local ScreenEffects = {}

---@param red      integer
---@param green    integer
---@param blue     integer
---@param alpha    integer
---@param duration number
function ScreenEffects.FlashScreen(red, green, blue, alpha, duration) end

function ScreenEffects.StopFlashScreen() end

---@param red      integer
---@param green    integer
---@param blue     integer
---@param gray     integer
---@param duration number
function ScreenEffects.ChangeScreenTone(red, green, blue, gray, duration) end

---@param duration number
function ScreenEffects.ClearScreenTone(duration) end

---@param power    number
---@param speed    number
---@param duration number
function ScreenEffects.ScreenShake(power, speed, duration) end

function ScreenEffects.StopScreenShake() end

return ScreenEffects
