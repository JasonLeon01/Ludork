local Render = require("Global.Utils.Render")
local Data = require("Source.Data")
local Locale = require("Source.Locale.Core")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Battler = require("Source.Battler")
local Enemy = require("Source.Enemy")
local IconTexture = require("Source.UI.IconTexture")
local Ui = require("Source.UI.Ui")
local WindowEnemyBookCellUI = require("Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local Special = GeneralEnum.Special
local DamageType = Battler.DamageType

local _CELL_WIDTH = 320
local _CELL_HEIGHT = 64
local _SPECIAL_ORDER = { "Poisoning", "Weaken", "Hard", "Magic", "MultiHit", "Compete", "Domain", "Flank", "Blockade" }
local _specialIconCache = {}

local WindowEnemyBookUI = {}

---@param text string | nil
---@return string
function WindowEnemyBookUI.FormatLocaleText(text)
    return LOC(tostring(text or "")):gsub("\\n", "\n")
end

---@param entry Source.UI.WindowEnemyBook.Entry
function WindowEnemyBookUI.RefreshEntryLocale(entry)
    entry.name = WindowEnemyBookUI.FormatLocaleText(entry.nameSource)
    entry.desc = WindowEnemyBookUI.FormatLocaleText(entry.descSource)
    for _, display in ipairs(entry.specialDisplays) do
        display.name = WindowEnemyBookUI.FormatLocaleText(display.nameSource)
    end
    for _, detail in ipairs(entry.specialDetails) do
        detail.name = WindowEnemyBookUI.FormatLocaleText(detail.nameSource)
        detail.desc = WindowEnemyBookUI.FormatLocaleText(detail.descSource)
    end
end

function WindowEnemyBookUI.loadSpecialIcon(iconPath)
    if not bool(iconPath) then
        return nil
    end
    local cached = _specialIconCache[iconPath]
    if cached ~= nil then
        return cached
    end
    local texture = IconTexture.load(iconPath, "Icons/Specials", ".png")
    _specialIconCache[iconPath] = texture
    return texture
end

function WindowEnemyBookUI:init(model, size)
    self._size = size
    self._cellControllers = {}
    super(WindowEnemyBookUI, self).init(model, nil)
end

function WindowEnemyBookUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._listView = self:requireControl("EnemyList")
end

function WindowEnemyBookUI:prepare()
    return super(WindowEnemyBookUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowEnemyBookUI:attach()
    self:attachWindowView(self.model)
end

function WindowEnemyBookUI:getWindowFrame()
    return self._windowFrame
end

function WindowEnemyBookUI:getContent()
    return self._content
end

function WindowEnemyBookUI:getListView()
    return self._listView
end

function WindowEnemyBookUI:refreshEnemies(gameMap)
    local entries = {}
    ---@type dict<string, boolean>
    local seen = dict()
    if gameMap ~= nil then
        for _, actor in ipairs(gameMap:getAllActors()) do
            if Class.isInstance(actor, Enemy) and not actor:isDestroyed() then
                local enemyID = actor.ID
                local visual = Render.CaptureActorVisual(actor)
                local signature = Render.GetActorVisualSignature(enemyID, visual)
                if not seen[signature] then
                    seen[signature] = true
                    entries[#entries + 1] = self:buildEntry(actor, visual)
                end
            end
        end
    end
    self.model._enemies = entries
    self._listView:clearChildren()
    self._cellControllers = {}
    for _, entry in ipairs(entries) do
        local enemyEntry = entry
        local cellController = WindowEnemyBookCellUI.new({
            entry = enemyEntry,
            callback = function (_obj, _kwargs)
                self.model:_confirmEnemy(enemyEntry)
            end
        })
        local logicalSize = sf.Vector2u.new(_CELL_WIDTH, _CELL_HEIGHT)
        ---@cast logicalSize sf.Vector2u
        local cell = cellController:prepare(logicalSize)
        self._cellControllers[#self._cellControllers + 1] = cellController
        self._listView:addChild(cell)
    end
    self.model.index = bool(entries) and 0 or nil
    if self.model._rect:getParent() ~= nil then
        self._content:removeChild(self.model._rect)
    end
end

function WindowEnemyBookUI:tick(deltaTime)
    for _, cellController in ipairs(self._cellControllers) do
        cellController:tick(deltaTime)
    end
end

function WindowEnemyBookUI:refreshLocale()
    for _, entry in ipairs(self.model._enemies) do
        WindowEnemyBookUI.RefreshEntryLocale(entry)
    end
    for _, cellController in ipairs(self._cellControllers) do
        cellController:refreshLocale()
    end
end

function WindowEnemyBookUI:buildEntry(enemy, visual)
    visual = visual or Render.CaptureActorVisual(enemy)
    local damageType, damage = enemy:getDamage(self.model._player)
    local special = enemy:getSpecial()
    local sourceRect = enemy:getTextureRect()
    local textureRect = nil
    if sourceRect ~= nil then
        textureRect = copy(sourceRect)
    end
    local sourceScale = enemy:getScale()
    local scale = copy(sourceScale)
    local nameSource = tostring(enemy.infoComp.name or enemy.ID)
    local descSource = enemy.infoComp.desc
    return {
        nameSource = nameSource,
        descSource = descSource,
        name = self:formatName(nameSource),
        desc = self:formatText(descSource),
        MAXHP = enemy.infoComp.MAXHP,
        ATK = enemy:getATK(self.model._player),
        DEF = enemy:getDEF(self.model._player),
        EXP = enemy.infoComp.EXP,
        GOLD = enemy.infoComp.GOLD,
        damage = damageType == DamageType.UNDEFEATABLE and "???" or damage,
        critical = enemy:getCriticalValue(self.model._player),
        hitCount = enemy:hasSpecial(Special.MultiHit) and enemy:getHitCount() or nil,
        specialDisplays = self:buildSpecialDisplays(special),
        specialDetails = self:buildSpecialDetails(special),
        visual = visual,
        texture = enemy:getTexture(),
        texturePath = enemy.texturePath,
        rect = textureRect,
        scale = scale,
        animatable = enemy:getAnimatable(),
        switchInterval = enemy.switchInterval
    }
end

function WindowEnemyBookUI:buildSpecialDisplays(special)
    if not bool(special) then
        return {}
    end
    local specialKeys = table.orderedStringKeys(special, _SPECIAL_ORDER)
    if #specialKeys > 3 then
        return {
            {
                texture = nil,
                nameSource = "MORE_SPECIAL",
                name = LOC("MORE_SPECIAL")
            }
        }
    end
    local displays = {}
    for _, specialKey in ipairs(specialKeys) do
        local specialData = Data.getGeneralSpecialData(tostring(specialKey))
        local nameSource = tostring(specialData.name or specialKey)
        local iconPath = tostring(specialData.icon or specialKey)
        displays[#displays + 1] = {
            texture = self.loadSpecialIcon(iconPath),
            nameSource = nameSource,
            name = self:formatName(nameSource)
        }
    end
    return displays
end

function WindowEnemyBookUI:buildSpecialDetails(special)
    if not bool(special) then
        return {}
    end
    local details = {}
    for _, specialKey in ipairs(table.orderedStringKeys(special, _SPECIAL_ORDER)) do
        local specialData = Data.getGeneralSpecialData(tostring(specialKey))
        local nameSource = tostring(specialData.name or specialKey)
        local descSource = tostring(specialData.desc or "")
        details[#details + 1] = {
            nameSource = nameSource,
            descSource = descSource,
            name = self:formatText(nameSource),
            desc = self:formatText(descSource)
        }
    end
    return details
end

function WindowEnemyBookUI:formatName(name)
    return self:formatText(name)
end

function WindowEnemyBookUI:formatText(text)
    local _ = self

    return WindowEnemyBookUI.FormatLocaleText(text)
end

return Ui.define("WindowEnemyBook", WindowEnemyBookUI)
