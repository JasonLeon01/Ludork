# Declarative UI Controller Examples

These examples walk through building an inventory window on top of an existing JSON UI asset. See [Declarative UI Authoring Workflow](<Authoring Workflow.md>) for file ownership and export steps.

## Start with the unchanged template

Create `Scripts/Source/Windows/WindowInventory.lua` for an existing `Inventory.json`:

```lua
local Ui = require("Internal.UIBase.Ui")
local InventoryView = require("Internal.UI.Inventory")

local Controller = {}

return Ui.DefineWindow(InventoryView, Controller)
```

Create and mount the window from its owning Scene:

```lua
local WindowInventory = require("Source.Windows.WindowInventory")

local window = WindowInventory.new():mount(uiManager)
```

The window renders the template's authored content and dimensions. Keep a reference on the owner and call `window:dispose()` when closing out its lifetime.

## Replace content and react to a field

Assume `Inventory.json` contains `WindowFrame`, `Content`, the shared `WindowChrome` and `WindowPauseMark` assets, and `Engine.PlainText` nodes named `Title` and `Count`. Add business logic in the same window module:

```lua
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Internal.UIBase.Ui")
local InventoryView = require("Internal.UI.Inventory")
local WindowBase = require("Internal.UIBase.WindowBase")

---@class Source.Windows.WindowInventory.Controller
local Controller = {}

Controller.windowOptions = { centered = true, returnButton = true }

function Controller:init(inventory)
    self.model = inventory
    self.view:setText("Title", LocaleCore.ApplyStringLocaleFormat("INVENTORY_TITLE"))
    self:watch(inventory, "count", Controller.updateCount)
end

function Controller:updateCount(count)
    self.ui.controls.Count:setString(tostring(count or 0))
end

function Controller:onReturn()
    self.host:hideImmediate()
end

return Ui.DefineWindow(InventoryView, Controller, WindowBase)
```

`watch` renders the current `count` immediately and updates the label whenever the field changes:

```lua
local WindowInventory = require("Source.Windows.WindowInventory")

local inventoryModel = { count = 3 }
local window = WindowInventory.new(inventoryModel):mount(uiManager)
inventoryModel.count = 4
```

The text updates to `4` without manually calling `refresh`. To respond to runtime language switches, move the title update into `refresh` and register the locale event in `refreshEvents`.

### Declare business members once

Handwrite `Scripts/stub/Source/Windows/WindowInventory.d.lua` with only the private Controller and its business types:

```lua
---@meta

---@class InventoryModel
---@field count integer

---@class Source.Windows.WindowInventory.Controller: Internal.UIBase.UiController
---@field host Source.Windows.WindowInventory
---@field ui Internal.UI.Inventory
---@field model InventoryModel
local Controller = {}

---@param inventory InventoryModel
function Controller:init(inventory) end

---@param count integer | nil
function Controller:updateCount(count) end

function Controller:onReturn() end
```

Run `ludork-assets ui-assets generate <project-root>` to generate the public window declaration. Callers require `Source.Windows.WindowInventory`; `Source.Windows.WindowInventory.Controller` is a private type rather than a module.

## Bind a native callback

Independent row Controllers use `Ui.Define(RowView, RowController)` and `RowController.new(model, ui?)`. Bind native control callbacks in `bind`:

```lua
function SettingRowController:bind()
    self.ui.controls.DropBox:setItems(self.model.items)
    self.ui.controls.DropBox:setOnSelectedIndexChanged(self:bindCallback(SettingRowController.onSelectedIndexChanged))
end

function SettingRowController:onSelectedIndexChanged(index)
    self.model:setSelectedIndex(index)
end
```

Pass the unbound class method to `bindCallback`; the wrapper supplies `self` and becomes inert after Controller disposal. See [Observe model fields and bind callbacks](<Runtime Contract.md#observe-model-fields-and-bind-callbacks>).

## Add behaviour to existing child Views

When the parent JSON embeds a child asset named `DetailsAsset`, wrap it in its window class from the parent Controller:

```lua
local WindowDetails = require("Source.Windows.WindowDetails")

function Controller:init(inventory)
    self.details = self:createChild("DetailsAsset", WindowDetails, inventory)
end
```

`createChild` reuses `self.ui.assets.DetailsAsset` and its authored layout, and the parent View owns the child window's disposal.

Independent fixed rows use their standalone Controller module directly:

```lua
local ItemRowController = require("Source.Windows.WindowInventory.ItemRow.Controller")

local row = ItemRowController.new(itemModel, self.ui.assets.ItemRow)
row:prepare()
```

## Populate model-dependent rows

Bind a variable-length `ItemList` to `self.model.items`:

```lua
function Controller:init(model, ui)
    super(Controller, self).init(model, ui)
    self.rows = self:createCollection(self.ui.controls.ItemList, ItemRowController)
end

function Controller:refresh()
    self.rows:clear()
    for _, item in ipairs(self.model.items) do
        self.rows:add(item)
    end
    self.rows:layout()
end
```

This snippet shows an independent `Ui.Define` Controller. Inside a `Ui.DefineWindow` Controller, create the collection in business `init(...)` without calling `super`, because the window factory constructs the View before `init`. Require `ItemRowController` with the module's top-level imports.

The collection replaces authored preview children and automatically pools rows across `clear` and `add` — row Controllers never call `Pool` directly. See [Dynamic collections](<Runtime Contract.md#dynamic-collections>) for reuse and disposal rules.

## Publish updates

Call `WindowInventory.Publish(payload)` to notify every live, prepared Controller via the asset's `EventBus` channel. Override `onViewUpdate(payload)` to update state before refresh and reflow, and add events such as locale changes to `refreshEvents` when needed. Independent row and Scene Controllers expose `Publish` via `Ui.Define`; View instances without a Controller do not subscribe.
