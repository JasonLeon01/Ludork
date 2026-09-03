local GlobalCore = require("GlobalCore")
local Render = require("Global.Utils.Render")
local GameplayEventData = GlobalCore.GameplayEventData
local Data = require("Source.Data")
local Locale = require("Source.Locale.Core")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Enemy = require("Source.Enemy")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local IconTexture = require("Source.UI.IconTexture")
local Ui = require("Source.UI.Ui")
local WindowEnemyBookCellUI = require("Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local Special = GeneralEnum.Special

local _CELL_WIDTH = 320
local _CELL_HEIGHT = 64
local _specialIconCache = {}

local WindowEnemyBookUI = {}

local function formatSpecialDescription(descSource, specialID, value)
    local valueText = tostring(value)
    if specialID == Special.FixDmg then
        valueText = string.replace(valueText, "{m", "{m}{")
        valueText = string.replace(valueText, "{e", "{e}{")
        valueText = LOC(valueText)
    end
    return string.pformat(WindowEnemyBookUI.FormatLocaleText(descSource), { value = valueText })
end

---@param text string | nil
---@return string
function WindowEnemyBookUI.FormatLocaleText(text)
    return (LOC(tostring(text or "")):gsub("\\n", "\n"))
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
        detail.desc = formatSpecialDescription(detail.descSource, detail.specialID, detail.value)
    end
end

local function loadSpecialIcon(iconPath)
    if not bool(iconPath) then
        return nil
    end
    local cached = _specialIconCache[iconPath]
    if cached ~= nil then
        return cached
    end
    local texture = IconTexture.Load(iconPath, "Icons/Specials", ".png")
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
    self._scrollBox = self:requireControl("EnemyScrollBox")
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

function WindowEnemyBookUI:getScrollBox()
    return self._scrollBox
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
    self.model:resetSelection()
    self.model:_detachSelectionRect()
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
    local abilitySystem = enemy:getAbilitySystemComponent()
    local battleResult = MotaBattleAbility
        .new()
        :calculate(abilitySystem, GameplayEventData.new(nil, self.model._player))
    local battleData = battleResult.data
    local special = enemy.attributes.special
    local textureRect = copy(assert(visual.rect or visual.textureRect))
    local scale = copy(visual.scale)
    local nameSource = tostring(enemy.attributes.name or enemy.attributes.ID)
    local descSource = enemy.attributes.desc
    return {
        nameSource = nameSource,
        descSource = descSource,
        name = self:formatName(nameSource),
        desc = self:formatText(descSource),
        MAXHP = enemy.attributes.MAXHP,
        ATK = battleData.enemyAttack.attackerATK,
        DEF = battleData.playerAttack.defenderDEF,
        EXP = enemy.attributes.EXP,
        GOLD = enemy.attributes.GOLD,
        damage = battleResult.code == MotaBattleAbility.BattleResult.CANNOT_DAMAGE and "???" or battleData.damage,
        critical = MotaBattleAbility.CalculateCriticalValue(enemy, self.model._player),
        hitCount = abilitySystem:hasMatchingGameplayTag("Special." .. Special.MultiHit)
            and battleData.enemyAttack.hitCount
            or nil,
        specialDisplays = self:buildSpecialDisplays(special),
        specialDetails = self:buildSpecialDetails(special),
        texture = visual.texture,
        texturePath = tostring(visual.texturePath or ""),
        rect = textureRect,
        scale = scale,
        animatable = bool(visual.animatable),
        switchInterval = visual.switchInterval or 0.2,
        shaderPath = tostring(visual.shaderPath or ""),
        hue = visual.hue or 0.0
    }
end

function WindowEnemyBookUI:buildSpecialDisplays(special)
    if not bool(special) then
        return {}
    end
    local specialKeys = table.orderedStringKeys(special)
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
        local specialData = Data.GetGeneralSpecialData(tostring(specialKey))
        local nameSource = tostring(specialData.name or specialKey)
        local iconPath = tostring(specialData.icon or specialKey)
        displays[#displays + 1] = {
            texture = loadSpecialIcon(iconPath),
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
    for _, specialKey in ipairs(table.orderedStringKeys(special)) do
        local specialData = Data.GetGeneralSpecialData(tostring(specialKey))
        local nameSource = tostring(specialData.name or specialKey)
        local descSource = tostring(specialData.desc or "")
        local value = special[specialKey]
        details[#details + 1] = {
            specialID = tostring(specialKey),
            value = deepcopy(value),
            nameSource = nameSource,
            descSource = descSource,
            name = self:formatText(nameSource),
            desc = formatSpecialDescription(descSource, tostring(specialKey), value)
        }
    end
    return details
end

function WindowEnemyBookUI:formatName(name)
    return self:formatText(name)
end

---@diagnostic disable-next-line: unused
function WindowEnemyBookUI:formatText(text)
    return WindowEnemyBookUI.FormatLocaleText(text)
end

return Ui.Define("WindowEnemyBook", WindowEnemyBookUI)
