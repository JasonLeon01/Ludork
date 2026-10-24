---@enum Enums.MovementResultCode
local MovementResultCode = {
    HAZARD_BOUNDS = "MovementHazardBounds",
    HAZARD = "MovementHazard",
    NO_HAZARD = "NoMovementHazard",
    HAZARD_COMMITTED = "MovementHazardCommitted"
}

return MovementResultCode
