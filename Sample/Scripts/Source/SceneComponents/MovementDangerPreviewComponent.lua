local Engine = require("Engine")
local ComponentBase = require("Global.Components.ComponentBase")
local Data = require("Source.Data")
local EnemyDamageText = require("Source.EnemyDamageText")
local Battler = require("Source.Battler")
local Utils = require("Source.NodeFunctions.Utils")

local PlainText = Engine.PlainText
local DamageType = Battler.DamageType

local MovementDangerPreviewComponent = {}

function MovementDangerPreviewComponent:init(gameMap, dangerState)
    super(MovementDangerPreviewComponent, self).init(gameMap)
    self._dangerState = dangerState
    self._cachedRevision = -1
    self._cachedDisplayScale = nil
    self._cachedEntries = {}
    self._texts = {}
end

function MovementDangerPreviewComponent:onRender(camera)
    local player = self._parent:getPlayer()
    if player == nil or EnemyDamageText.EnemyDamageHintLevel < EnemyDamageText.DamageHintLevel.MAP
        or not player:hasItem(EnemyDamageText.requiredItemID) then
        return
    end
    local revision = self._dangerState:getRevision()
    local displayScale = Engine.Scale
    if revision ~= self._cachedRevision or displayScale ~= self._cachedDisplayScale then
        self:_refreshEntries(player)
        self._cachedRevision = revision
        self._cachedDisplayScale = displayScale
    end
    if not bool(self._cachedEntries) then
        return
    end
    for index in ipairs(self._cachedEntries) do
        camera:render(self._texts[index])
    end
end

---@param player Source.Player.Player
function MovementDangerPreviewComponent:_refreshEntries(player)
    self._cachedEntries = self._dangerState:getEntries()
    local cellSize = Engine.CellSize
    local displayScale = math.max(Engine.Scale, 0.000001)
    local inverseScale = 1.0 / displayScale
    for index, entry in ipairs(self._cachedEntries) do
        if self._texts[index] == nil then
            self._texts[index] = PlainText.new(Data.GetPlainTextConfig(EnemyDamageText.textConfig), "")
        end
        self._texts[index]:setString(tostring(Utils.ToShortNumber(entry.damage)))
        self._texts[index]:setScale(sf.Vector2f.new(inverseScale, inverseScale))
        self._texts[index]:setColour(
            EnemyDamageText.GetDamageColor(DamageType.NORMAL, entry.damage, player.infoComp.HP)
        )
        local bounds = self._texts[index]:getLocalBounds()
        local worldX = entry.position.x * cellSize + (cellSize - bounds.size.x) * 0.5 - bounds.position.x
        local worldY = entry.position.y * cellSize + (cellSize - bounds.size.y) * 0.5 - bounds.position.y
        self._texts[index]:setPosition(sf.Vector2f.new(worldX / displayScale, worldY / displayScale))
    end
end

return class(MovementDangerPreviewComponent, ComponentBase)
