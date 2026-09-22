local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowBase = require("Source.Windows.Base.WindowBase")
local EnemyText = require("Source.EnemyText")
local Locale = require("Source.Locale.Core")
local NumberFormat = require("Source.Utils.NumberFormat")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowEnemyEncyclopedia")
local WindowEnemyBook = require("Source.Windows.WindowEnemyBook")
local EnemyEncyclopediaInfoPairController = require(
    "Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair.Controller"
)
local EnemyEncyclopediaSpecialRowController = require(
    "Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow.Controller"
)

---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat
local TextLayout = Engine.TextLayout
local Input = Engine.Input
local AudioManager = GlobalCore.AudioManager
local ToShortNumber = NumberFormat.ToShortNumber

local _DESC_MAX_LINES = 2

---@class Source.Windows.WindowEnemyEncyclopedia.Controller
local Controller = {}

Controller.windowOptions = { centered = true, hidden = true, returnButton = true, focusable = true }
---@type function
local formatHitCount
---@type function
local limitLines

function Controller:refresh()
    self:clearEnemyControls()
end

function Controller:open(entry)
    self:clearEnemyControls()
    self._entry = entry
    self.ui.controls["Portrait"]:setCharacter(
        entry.texture, entry.rect, entry.scale, entry.animatable, entry.switchInterval, entry.shaderPath or "",
        entry.hue or 0.0
    )
    self:setProperty("Portrait", "visible", true)
    self:_renderEntry(entry)
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
end

function Controller:refreshLocale()
    if self._entry == nil then
        return
    end
    WindowEnemyBook.RefreshEntryLocale(self._entry)
    self:_clearTextControls()
    self:_renderEntry(self._entry)
end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller:_renderEntry(entry)
    local nameWidth = math.max(1, math.floor(self.ui.controls["NameArea"]:getSize().x))
    local descriptionWidth = math.max(1, math.floor(self.ui.controls["DescriptionArea"]:getSize().x))
    local fittedName = TextLayout.fitPlainText(tostring(entry.name or ""), nameWidth, self.ui.controls["Name"])
    self:setText("Name", fittedName)
    self:setProperty("Name", "visible", true)
    local displayDescription = limitLines(
        TextLayout.wrapPlainText(tostring(entry.desc or ""), descriptionWidth, self.ui.controls["Description"]),
        _DESC_MAX_LINES, descriptionWidth, self.ui.controls["Description"]
    )
    self:setText("Description", displayDescription)
    self:setProperty("Description", "visible", true)
    self:buildInfo(entry)
    self:buildSpecials(entry)
    self.view:reflow()
end

function Controller:buildInfo(entry)
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
    for _, row in ipairs(rows) do
        for _, pair in ipairs(row) do
            self:addInfoPair(pair[1], tostring(ToShortNumber(pair[2])))
        end
    end
    local criticalText = EnemyText.FormatCritical(entry.critical)
    local hitCount = formatHitCount(entry.hitCount)
    if bool(hitCount) then
        self:addInfoPair(LOC("HIT"), hitCount)
    end
    if bool(criticalText) then
        self:addInfoPair(LOC("CRIT"), criticalText)
    end
end

function Controller:addInfoPair(label, value)
    self._infoRows:add({
        label = label,
        value = value
    })
    self._infoRows:layout()
end

function Controller:buildSpecials(entry)
    local contentWidth = math.max(1, math.floor(self.ui.controls["SpecialList"]:getSize().x))
    local specialDetails = entry.specialDetails or {}
    for _, special in ipairs(specialDetails) do
        self._specialRows:add({
            width = contentWidth,
            name = tostring(special.name or ""),
            description = tostring(special.desc or "")
        })
    end
    self._specialRows:layout()
end

function formatHitCount(hitCount)
    if hitCount == nil then
        return ""
    end
    return tostring(ToShortNumber(math.max(1, hitCount)))
end

function limitLines(text, maxLines, maxWidth, control)
    if not bool(text) then
        return ""
    end
    local lines = string.split(text, "\n")
    if #lines <= maxLines then
        return text
    end
    local limitedLines = {}
    for index = 1, maxLines do
        limitedLines[index] = lines[index]
    end
    if bool(limitedLines) then
        limitedLines[#limitedLines] = TextLayout.fitPlainText(limitedLines[#limitedLines] .. ".", maxWidth, control)
    end
    return table.concat(limitedLines, "\n")
end

function Controller:clearEnemyControls()
    self:_clearTextControls()
    self:setProperty("Portrait", "visible", false)
    self._entry = nil
    self.ui.controls["Portrait"]:resetAnimation()
end

function Controller:_clearTextControls()
    self._infoRows:clear()
    self._specialRows:clear()
    self:setProperty("Name", "visible", false)
    self:setProperty("Description", "visible", false)
    self:setText("Name", "")
    self:setText("Description", "")
end

function Controller:init(onClose)
    self._onCloseCallback = onClose
    self._entry = nil
    self._infoRows = self:createCollection(self.ui.controls["InfoLayer"], EnemyEncyclopediaInfoPairController)
    self._specialRows = self:createCollection(self.ui.controls["SpecialList"], EnemyEncyclopediaSpecialRowController)
end

function Controller:close()
    self.host:setActive(false)
    self.host:hideWithAnimation("FadeOut", function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function Controller:onKeyDown(_kwargs)
    if Input.isActionTriggered(Input.getConfirmKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getConfirmKeys(), true)
        return
    end
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
    end
end

function Controller:onMouseButtonDown(_kwargs)
    if _kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

function Controller:onReturn()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

return Ui.DefineWindow(View, Controller, WindowBase)
