---@meta Global.Gameplay.GameplayAbilityResult

---@alias Global.Gameplay.GameplayAbilityResultCode string | integer

---@class Global.Gameplay.GameplayAbilityResult
---@field ok   boolean
---@field code Global.Gameplay.GameplayAbilityResultCode
---@field data table<string, any>
---@field new  fun(ok: boolean, code?: Global.Gameplay.GameplayAbilityResultCode, data?: table<string, any>): Global.Gameplay.GameplayAbilityResult
local GameplayAbilityResult = {}

---@param ok    boolean
---@param code? Global.Gameplay.GameplayAbilityResultCode
---@param data? table<string, any>
function GameplayAbilityResult:init(ok, code, data) end

---@param code? Global.Gameplay.GameplayAbilityResultCode
---@param data? table<string, any>
---@return Global.Gameplay.GameplayAbilityResult
function GameplayAbilityResult.Success(code, data) end

---@param code  Global.Gameplay.GameplayAbilityResultCode
---@param data? table<string, any>
---@return Global.Gameplay.GameplayAbilityResult
function GameplayAbilityResult.Failure(code, data) end

return GameplayAbilityResult
