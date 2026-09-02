---@meta Source.SceneComponents.MovementDangerGrid

local MovementDangerGrid = {}

---@param enemy Source.Enemy
---@return boolean
function MovementDangerGrid.HasMovementSpecial(enemy) end

---@param entry           Source.SceneComponents.MovementDangerEntry
---@param ignoredEnemySet table<Source.Enemy, boolean> | nil
---@return integer
function MovementDangerGrid.GetEntryDamage(entry, ignoredEnemySet) end

---@param enemies        Source.Enemy[]
---@param player         Source.Player.Player
---@param areaX          integer
---@param areaY          integer
---@param areaWidth      integer
---@param areaHeight     integer
---@param previewContext Source.MovementSpecials.PreviewContext
---@return Source.SceneComponents.MovementDangerEntry[] entries
---@return table<integer, table<integer, Source.SceneComponents.MovementDangerEntry>> grid
function MovementDangerGrid.Build(enemies, player, areaX, areaY, areaWidth, areaHeight, previewContext) end

---@param enemies            Source.Enemy[]
---@param player             Source.Player.Player
---@param areaX              integer
---@param areaY              integer
---@param areaWidth          integer
---@param areaHeight         integer
---@param previousAreaX      integer
---@param previousAreaY      integer
---@param previousAreaWidth  integer
---@param previousAreaHeight integer
---@param previousGrid       table<integer, table<integer, Source.SceneComponents.MovementDangerEntry>>
---@param previewContext     Source.MovementSpecials.PreviewContext
---@return Source.SceneComponents.MovementDangerEntry[] entries
---@return table<integer, table<integer, Source.SceneComponents.MovementDangerEntry>> grid
function MovementDangerGrid.Refresh(
    enemies, player, areaX, areaY, areaWidth, areaHeight, previousAreaX, previousAreaY, previousAreaWidth,
    previousAreaHeight, previousGrid, previewContext
) end

return MovementDangerGrid
