local GlobalCore = require("GlobalCore")
local Render = require("Global.Utils.Render")
local GameSystem = require("Source.System")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local Data = require("Source.Data")
local Locale = require("Source.Locale.Core")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Enemy = require("Source.Enemy")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local IconTexture = require("Source.UIBase.IconTexture")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowEnemyBook")
local WindowEnemyBookCellController = require("Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local Special = GeneralEnum.Special
local GameplayEventData = GlobalCore.GameplayEventData
local AudioManager = GlobalCore.AudioManager

local _CELL_WIDTH = 320
local _CELL_HEIGHT = 64

---@class Source.Windows.WindowEnemyBook.Controller
local Controller = {}

Controller.windowOptions = {
    centered = true,
    hidden = true,
    returnButton = true,
    list = "EnemyList",
    scroll = "EnemyScrollBox",
    itemWidth = _CELL_WIDTH,
    itemHeight = _CELL_HEIGHT
}

local function formatSpecialDescription(descSource, specialID, value)
    local valueText = tostring(value)
    if specialID == Special.FixDmg then
        valueText = string.replace(valueText, "{m", "{m}{")
        valueText = string.replace(valueText, "{e", "{e}{")
        valueText = LOC(valueText)
    end
    return string.pformat(Controller.FormatLocaleText(descSource), { value = valueText })
end

---@param text string | nil
---@return string
function Controller.FormatLocaleText(text)
    return (LOC(tostring(text or "")):gsub("\\n", "\n"))
end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller.RefreshEntryLocale(entry)
    entry.name = Controller.FormatLocaleText(entry.nameSource)
    entry.desc = Controller.FormatLocaleText(entry.descSource)
    for _, display in ipairs(entry.specialDisplays) do
        display.name = Controller.FormatLocaleText(display.nameSource)
    end
    for _, detail in ipairs(entry.specialDetails) do
        detail.name = Controller.FormatLocaleText(detail.nameSource)
        detail.desc = formatSpecialDescription(detail.descSource, detail.specialID, detail.value)
    end
end

function Controller:refreshEnemies(gameMap)
    local entries = {}
    ---@type dict<string, boolean>
    local seen = dict()
    if gameMap ~= nil then
        for _, actor in ipairs(gameMap:getAllActors()) do
            if Class.isInstance(actor, Enemy) and not actor:isDestroyed() and actor:isVisibleInHierarchy() then
                ---@cast actor Source.Enemy
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
    self._enemies = entries
    self._cells:clear()
    for _, entry in ipairs(entries) do
        local enemyEntry = entry
        local logicalSize = sf.Vector2u.new(_CELL_WIDTH, _CELL_HEIGHT)
        ---@cast logicalSize sf.Vector2u
        self._cells:add({
            entry = enemyEntry,
            callback = function (_obj, _kwargs)
                self:confirmEnemy(enemyEntry)
            end
        }, logicalSize)
    end
    self._cells:layout()
    self.host:resetSelection()
    self.host:detachSelectionRect()
end

function Controller:refreshLocale()
    for _, entry in ipairs(self._enemies) do
        Controller.RefreshEntryLocale(entry)
    end
    for _, cellController in ipairs(self._cells.items) do
        cellController:refreshLocale()
    end
end

function Controller:buildEntry(enemy, visual)
    visual = visual or Render.CaptureActorVisual(enemy)
    local abilitySystem = enemy:getAbilitySystemComponent()
    local battleResult = MotaBattleAbility
        .new()
        :calculate(abilitySystem, GameplayEventData.new(nil, self:getPlayer()))
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
        critical = MotaBattleAbility.CalculateCriticalValue(enemy, self:getPlayer()),
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

function Controller:buildSpecialDisplays(special)
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
            texture = IconTexture.Load(iconPath),
            nameSource = nameSource,
            name = self:formatName(nameSource)
        }
    end
    return displays
end

function Controller:buildSpecialDetails(special)
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

function Controller:formatName(name)
    return self:formatText(name)
end

---@diagnostic disable-next-line: unused
function Controller:formatText(text)
    return Controller.FormatLocaleText(text)
end

function Controller:init(player, onClose, onConfirm)
    self._player = player
    self._onCloseCallback = onClose
    self._onConfirmCallback = onConfirm
    self._enemies = {}
    self._cells = self:createCollection(self.ui.controls["EnemyList"], WindowEnemyBookCellController)
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:open(gameMap)
    self:refreshEnemies(gameMap)
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
end

function Controller:close(onHidden)
    self.host:setActive(false)
    self.host:hideWithAnimation("FadeOut", onHidden)
end

---@diagnostic disable-next-line: unused
function Controller:_getRectPositionForIndex(index)
    return sf.Vector2f.new(0.0, index * _CELL_HEIGHT)
end

---@diagnostic disable-next-line: unused
function Controller:getItemWidth()
    return _CELL_WIDTH
end

function Controller:onReturn()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function Controller:confirmEnemy(entry)
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:close()
    if self._onConfirmCallback ~= nil then
        self._onConfirmCallback(entry)
    end
end

function Controller:getPlayer()
    return self._player
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
