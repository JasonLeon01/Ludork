local Engine = require("Engine")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow")

local TextLayout = Engine.TextLayout

local EnemyEncyclopediaSpecialRowController = {}

function EnemyEncyclopediaSpecialRowController:init(model)
    self._displayName = ""
    self._displayDescription = ""
    super(EnemyEncyclopediaSpecialRowController, self).init(model, nil)
    self._height = math.floor(self.ui.designSize.y)
end

function EnemyEncyclopediaSpecialRowController:refresh()
    local descriptionWidth = math.max(1, self.model.width - self.ui.controls["DescriptionArea"]:getPosition().x)
    local nameControl = self.ui.controls["Name"]
    local descriptionControl = self.ui.controls["Description"]
    ---@cast nameControl Engine.PlainText
    ---@cast descriptionControl Engine.PlainText
    self._displayName = TextLayout.fitPlainText(
        tostring(self.model.name or ""), self.ui.controls["NameArea"]:getSize().x, nameControl
    )
    local wrappedDescription = TextLayout.wrapPlainText(
        tostring(self.model.description or ""), descriptionWidth, descriptionControl
    )
    self._displayDescription = wrappedDescription:find("\n", 1, true) ~= nil and wrappedDescription
        or TextLayout.fitPlainText(wrappedDescription, descriptionWidth, descriptionControl)
    local _, newlineCount = self._displayDescription:gsub("\n", "")
    local lineCount = math.max(1, newlineCount + 1)
    self._height = math.floor(lineCount * self.ui.designSize.y)
    self:setText("Name", self._displayName)
    self:setText("Description", self._displayDescription)
end

function EnemyEncyclopediaSpecialRowController:prepare()
    local root = super
        (EnemyEncyclopediaSpecialRowController, self)
        .prepare(sf.Vector2u.new(self.model.width, self._height))
    self.view:reflow(sf.Vector2u.new(self.model.width, self._height))
    return root
end

function EnemyEncyclopediaSpecialRowController:getHeight()
    return self._height
end

return Ui.Define(View, EnemyEncyclopediaSpecialRowController)
