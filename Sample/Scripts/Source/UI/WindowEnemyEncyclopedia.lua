local Engine = require("Engine")
local Data = require("Source.Data")
local Locale = require("Source.Locale.Core")
local TextLayout = require("Source.TextLayout")
local NodeUtils = require("Source.NodeFunctions.Utils")
local Ui = require("Source.UI.Ui")
local WindowEnemyBookUI = require("Source.UI.WindowEnemyBook")
local ActorPreviewController = require("Source.UI.Parts.Shared.ActorPreviewController")
local EnemyEncyclopediaInfoPairUI = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair")
local EnemyEncyclopediaSpecialRowUI = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow")

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local ToShortNumber = NodeUtils.ToShortNumber

local _PORTRAIT_AREA_HEIGHT = Engine.CellSize
local _NAME_TEXT_CONFIG = "UI/Text20"
local _NAME_TOP_MARGIN = 8
local _INFO_TEXT_CONFIG = "UI/LeftText16"
local _INFO_TOP_MARGIN = 8
local _INFO_COLUMN_GAP = 203
local _INFO_PAIR_WIDTH = 200
local _INFO_ROW_GAP = 32
local _INFO_LAYER_HEIGHT = 96
local _DESC_TOP_MARGIN = 8
local _DESC_LINE_GAP = 22
local _DESC_MAX_LINES = 2
local _SPECIAL_TOP_MARGIN = 16

---@class Source.UI.WindowEnemyEncyclopedia
local WindowEnemyEncyclopediaUI = {}

function WindowEnemyEncyclopediaUI:init(model, size)
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self._infoPairControllers = {}
    self._specialRowControllers = {}
    self._entry = nil
    super(WindowEnemyEncyclopediaUI, self).init(model, nil)
end

function WindowEnemyEncyclopediaUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._portraitControl = self:requireControl("Portrait")
    self._previewController = ActorPreviewController.new(self._portraitControl)
    self._nameControl = self:requireControl("Name")
    self._infoLayer = self:requireControl("InfoLayer")
    self._descriptionControl = self:requireControl("Description")
    self._specialList = self:requireControl("SpecialList")
end

function WindowEnemyEncyclopediaUI:refresh()
    self:clearEnemyControls()
end

function WindowEnemyEncyclopediaUI:prepare()
    return super(WindowEnemyEncyclopediaUI, self).prepare(self._logicalSize)
end

function WindowEnemyEncyclopediaUI:attach()
    self:attachWindowView(self.model)
end

function WindowEnemyEncyclopediaUI:getWindowFrame()
    return self._windowFrame
end

function WindowEnemyEncyclopediaUI:getContent()
    return self._content
end

function WindowEnemyEncyclopediaUI:open(entry)
    self:clearEnemyControls()
    self._entry = entry
    self._previewController:setEntry(entry)
    self:_renderEntry(entry)
end

function WindowEnemyEncyclopediaUI:refreshLocale()
    local entry = self._entry
    if entry == nil then
        return
    end
    WindowEnemyBookUI.RefreshEntryLocale(entry)
    self:_clearTextControls()
    self:_renderEntry(entry)
end

---@param entry Source.UI.WindowEnemyBook.Entry
function WindowEnemyEncyclopediaUI:_renderEntry(entry)
    local contentWidth = math.max(1, math.floor(self._content:getSize().x))
    local fittedName = TextLayout.fitPlainText(tostring(entry.name or ""), contentWidth, _NAME_TEXT_CONFIG)
    self:setText("Name", fittedName)
    self:setProperty("Name", "visible", true)
    local displayDescription = WindowEnemyEncyclopediaUI.limitLines(
        TextLayout.wrapPlainText(tostring(entry.desc or ""), contentWidth, _INFO_TEXT_CONFIG),
        _DESC_MAX_LINES, contentWidth
    )
    self:setText("Description", displayDescription)
    self:setProperty("Description", "visible", true)
    self.view:reflow(self._logicalSize)

    local portraitHeight = self:_layoutPortrait()
    local nameY = portraitHeight + _NAME_TOP_MARGIN
    local nameWidth = TextLayout.measurePlainText(_NAME_TEXT_CONFIG, fittedName)
    self._nameControl:setPosition(sf.Vector2f.new((contentWidth - nameWidth) / 2.0, nameY))
    local nameBottom = nameY + Data.getPlainTextConfig(_NAME_TEXT_CONFIG).characterSize
    local infoY = nameBottom + _INFO_TOP_MARGIN
    self:_layoutInfoLayer(contentWidth, infoY)
    self:buildInfo(entry, infoY)
    local descY = infoY + 3 * _INFO_ROW_GAP + _DESC_TOP_MARGIN
    self._descriptionControl:setPosition(sf.Vector2f.new(0.0, descY))
    self.model._infoTexts[#self.model._infoTexts + 1] = self._descriptionControl
    local specialY = descY + _DESC_MAX_LINES * _DESC_LINE_GAP + _SPECIAL_TOP_MARGIN
    self:buildSpecials(entry, specialY)
    self:_syncModelState()
end

---@return number
function WindowEnemyEncyclopediaUI:_layoutPortrait()
    local contentSize = self._content:getSize()
    local bounds = sf.FloatRect.new(
        sf.Vector2f.new(0.0, 0.0), sf.Vector2f.new(contentSize.x, _PORTRAIT_AREA_HEIGHT)
    )
    return self._previewController:layout(bounds, "top").y
end

---@param contentWidth integer
---@param infoY        number
function WindowEnemyEncyclopediaUI:_layoutInfoLayer(contentWidth, infoY)
    local logicalSize = sf.Vector2u.new(contentWidth, _INFO_LAYER_HEIGHT)
    ---@cast logicalSize sf.Vector2u
    self._infoLayer:resize(logicalSize)
    self._infoLayer:setView(self._infoLayer:getDefaultView())
    self._infoLayer:setPosition(sf.Vector2f.new(0.0, infoY))
end

function WindowEnemyEncyclopediaUI:buildInfo(entry, infoY)
    local rows = {
        {
            {
                LOC("HP"),
                entry.MAXHP or 0
            },
            {
                LOC("ATK"),
                entry.ATK or 0
            },
            {
                LOC("DEF"),
                entry.DEF or 0
            }
        },
        {
            {
                LOC("EXP"),
                entry.EXP or 0
            },
            {
                LOC("GOLD"),
                entry.GOLD or 0
            },
            {
                LOC("DMG"),
                entry.damage or "???"
            }
        }
    }
    for rowIndex, row in ipairs(rows) do
        for columnIndex, pair in ipairs(row) do
            self:addInfoPair(
                pair[1], tostring(ToShortNumber(pair[2])), columnIndex - 1, infoY + (rowIndex - 1) * _INFO_ROW_GAP
            )
        end
    end
    local criticalText = WindowEnemyEncyclopediaUI.formatCriticalText(entry.critical or -2)
    local hitCount = WindowEnemyEncyclopediaUI.formatHitCount(entry.hitCount)
    local criticalColumnIndex = 0
    if bool(hitCount) then
        self:addInfoPair(LOC("HIT"), hitCount, 0, infoY + 2 * _INFO_ROW_GAP)
        criticalColumnIndex = 1
    end
    if bool(criticalText) then
        self:addInfoPair(LOC("CRIT"), criticalText, criticalColumnIndex, infoY + 2 * _INFO_ROW_GAP)
    end
end

function WindowEnemyEncyclopediaUI:addInfoPair(label, value, columnIndex, y)
    local controller = EnemyEncyclopediaInfoPairUI.new({
        label = label,
        value = value
    })
    local logicalSize = sf.Vector2u.new(_INFO_PAIR_WIDTH, _INFO_ROW_GAP)
    ---@cast logicalSize sf.Vector2u
    local root = controller:prepare(logicalSize)
    root:setPosition(sf.Vector2f.new(columnIndex * _INFO_COLUMN_GAP, y - self._infoLayer:getPosition().y))
    self._infoLayer:addChild(root)
    self._infoPairControllers[#self._infoPairControllers + 1] = controller
    self.model._infoTexts[#self.model._infoTexts + 1] = controller:getLabel()
    self.model._infoTexts[#self.model._infoTexts + 1] = controller:getValue()
end

function WindowEnemyEncyclopediaUI:buildSpecials(entry, y)
    local contentSize = self._content:getSize()
    local contentWidth = math.max(1, math.floor(contentSize.x))
    local listHeight = math.max(1, math.floor(contentSize.y - y))
    self._specialList:setPosition(sf.Vector2f.new(0.0, y))
    local listSize = sf.Vector2u.new(contentWidth, listHeight)
    ---@cast listSize sf.Vector2u
    self._specialList:setSize(listSize)
    local specialDetails = entry.specialDetails or {}
    for _, special in ipairs(specialDetails) do
        local controller = EnemyEncyclopediaSpecialRowUI.new({
            width = contentWidth,
            name = tostring(special.name or ""),
            description = tostring(special.desc or "")
        })
        local root = controller:prepare()
        self._specialList:addChild(root)
        self._specialRowControllers[#self._specialRowControllers + 1] = controller
        self.model._infoTexts[#self.model._infoTexts + 1] = controller:getNameText()
        self.model._infoTexts[#self.model._infoTexts + 1] = controller:getDescriptionText()
    end
end

function WindowEnemyEncyclopediaUI.formatCriticalText(criticalValue)
    local value = tonumber(criticalValue)
    value = value ~= nil and math.modf(value) or -2
    if value == -2 then
        return ""
    end
    if value == -1 then
        return "???"
    end
    return tostring(ToShortNumber(value))
end

function WindowEnemyEncyclopediaUI.formatHitCount(hitCount)
    if hitCount == nil then
        return ""
    end
    local value = tonumber(hitCount)
    if value == nil then
        return ""
    end
    return tostring(ToShortNumber(math.max(1, math.modf(value))))
end

function WindowEnemyEncyclopediaUI.limitLines(text, maxLines, maxWidth)
    if not bool(text) then
        return ""
    end
    local lines = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end
    if #lines <= maxLines then
        return text
    end
    local limitedLines = {}
    for index = 1, maxLines do
        limitedLines[index] = lines[index]
    end
    if bool(limitedLines) then
        limitedLines[#limitedLines] = TextLayout.fitPlainText(
            limitedLines[#limitedLines] .. ".", maxWidth, _INFO_TEXT_CONFIG
        )
    end
    return table.concat(limitedLines, "\n")
end

function WindowEnemyEncyclopediaUI:tick(deltaTime)
    self._previewController:tick(deltaTime)
    self:_syncModelState()
end

function WindowEnemyEncyclopediaUI:clearEnemyControls()
    self:_clearTextControls()
    self:setProperty("Portrait", "visible", false)
    self._entry = nil
    self._previewController:clear()
    self.model._portrait = nil
    self.model._nameText = nil
    self:_syncModelState()
end

function WindowEnemyEncyclopediaUI:_clearTextControls()
    for _, controller in ipairs(self._infoPairControllers) do
        local root = controller:getRoot()
        if root:getParent() == self._infoLayer then
            self._infoLayer:removeChild(root)
        end
    end
    self._specialList:clearChildren()
    self._infoPairControllers = {}
    self._specialRowControllers = {}
    self:setProperty("Name", "visible", false)
    self:setProperty("Description", "visible", false)
    self:setText("Name", "")
    self:setText("Description", "")
    self.model._infoTexts = {}
end

function WindowEnemyEncyclopediaUI:_syncModelState()
    local state = self._previewController:getState()
    self.model._texture = state.texture
    self.model._rect = state.rect
    self.model._animatable = state.animatable
    self.model._switchInterval = state.switchInterval
    self.model._switchTimer = state.switchTimer
    self.model._portrait = self._entry ~= nil and state.visible and self._portraitControl or nil
    self.model._nameText = self._entry ~= nil and self._nameControl or nil
end

return Ui.define("WindowEnemyEncyclopedia", WindowEnemyEncyclopediaUI)
