# 声明式 UI Controller 示例

本页通过一系列示例演示如何在已有 JSON UI 资产之上构建背包窗口。文件归属与导出步骤见[声明式 UI 编写工作流](<编写工作流.md>)。

## 从原始模板开始

为已有的 `Inventory.json` 创建 `Scripts/Source/Windows/WindowInventory.lua`：

```lua
local Ui = require("Internal.UIBase.Ui")
local InventoryView = require("Internal.UI.Inventory")

local Controller = {}

return Ui.DefineWindow(InventoryView, Controller)
```

在所属场景中创建并挂载该窗口：

```lua
local WindowInventory = require("Source.Windows.WindowInventory")

local window = WindowInventory.new():mount(uiManager)
```

窗口将按模板编排的内容与尺寸直接渲染。持有者应保留该引用，并在生命周期结束时调用 `window:dispose()`。

## 替换内容并响应字段变化

假设 `Inventory.json` 包含 `WindowFrame`、`Content`、共享资产 `WindowChrome` 与 `WindowPauseMark`，以及名为 `Title` 和 `Count` 的 `Engine.PlainText` 节点。在同一窗口模块中补充业务逻辑：

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

`watch` 会立即渲染当前 `count`，并在字段变化时自动更新文本：

```lua
local WindowInventory = require("Source.Windows.WindowInventory")

local inventoryModel = { count = 3 }
local window = WindowInventory.new(inventoryModel):mount(uiManager)
inventoryModel.count = 4
```

无需手动调用 `refresh`，文本即更新为 `4`。若需响应运行时语言切换，请将标题更新移入 `refresh`，并在 `refreshEvents` 中注册语言变更事件。

### 业务成员只声明一次

手写 `Scripts/stub/Source/Windows/WindowInventory.d.lua`，仅声明私有 Controller 及其业务类型：

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

运行 `ludork-assets ui-assets generate <project-root>` 生成公开窗口声明。调用方直接 `require("Source.Windows.WindowInventory")`；`Source.Windows.WindowInventory.Controller` 仅为私有类型而非可导入模块。

## 绑定原生回调

独立行 Controller 使用 `Ui.Define(RowView, RowController)` 与 `RowController.new(model, ui?)`，在 `bind` 中绑定原生控件回调：

```lua
function SettingRowController:bind()
    self.ui.controls.DropBox:setItems(self.model.items)
    self.ui.controls.DropBox:setOnSelectedIndexChanged(self:bindCallback(SettingRowController.onSelectedIndexChanged))
end

function SettingRowController:onSelectedIndexChanged(index)
    self.model:setSelectedIndex(index)
end
```

向 `bindCallback` 传入未绑定的类方法即可；包装器会自动注入 `self`，并在 Controller 释放后自动失效。参见[观察模型字段与绑定回调](<运行时契约.md#观察模型字段与绑定回调>)。

## 为已有子 View 添加行为

当父级 JSON 嵌入了名为 `DetailsAsset` 的子资产时，可在父 Controller 中用对应窗口类包装它：

```lua
local WindowDetails = require("Source.Windows.WindowDetails")

function Controller:init(inventory)
    self.details = self:createChild("DetailsAsset", WindowDetails, inventory)
end
```

`createChild` 复用 `self.ui.assets.DetailsAsset` 及其编排好的布局，子窗口的释放由父 View 托管。

独立的固定行则直接使用其独立 Controller 模块：

```lua
local ItemRowController = require("Source.Windows.WindowInventory.ItemRow.Controller")

local row = ItemRowController.new(itemModel, self.ui.assets.ItemRow)
row:prepare()
```

## 填充依赖模型的行

将可变长度的 `ItemList` 绑定到 `self.model.items`：

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

上述代码演示的是独立的 `Ui.Define` Controller。在 `Ui.DefineWindow` 的 Controller 中，直接在业务 `init(...)` 中创建集合即可，无需调用 `super`（窗口工厂已在 `init` 前构造完 View）。`ItemRowController` 应放在模块顶层与其他依赖一并 `require`。

集合会替换资产中的预览子项，并在 `clear` 与 `add` 之间自动池化复用行——行 Controller 绝不直接调用 `Pool`。复用与释放规则见[动态集合](<运行时契约.md#动态集合>)。

## 发布更新

调用 `WindowInventory.Publish(payload)` 可通过资产的 `EventBus` 通道通知所有存活且已准备好的 Controller。重写 `onViewUpdate(payload)` 可在刷新与重排之前更新状态，必要时可将语言切换等事件加入 `refreshEvents`。独立行 Controller 与场景 Controller 通过 `Ui.Define` 暴露 `Publish`；未绑定 Controller 的 View 实例不会订阅该通道。
