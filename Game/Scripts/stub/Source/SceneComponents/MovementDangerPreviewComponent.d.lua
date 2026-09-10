---@meta Source.SceneComponents.MovementDangerPreviewComponent

--- @brief Render centred movement-special expected-damage text at the MAP hint level after the player obtains the Enemy Book.
---@class Source.SceneComponents.MovementDangerPreviewComponent: ComponentBase
---@field _parent GameMap
---@field _dangerState Source.SceneComponents.MovementDangerState
local MovementDangerPreviewComponent = {}

---@param gameMap GameMap
---@param dangerState Source.SceneComponents.MovementDangerState
---@return Source.SceneComponents.MovementDangerPreviewComponent
function MovementDangerPreviewComponent.new(...) end

---@param gameMap GameMap
---@param dangerState Source.SceneComponents.MovementDangerState
function MovementDangerPreviewComponent:init(gameMap, dangerState) end

---@param camera GlobalCore.Camera
function MovementDangerPreviewComponent:onRender(camera) end

return MovementDangerPreviewComponent
