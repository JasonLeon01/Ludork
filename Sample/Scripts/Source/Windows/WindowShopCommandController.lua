local LocaleCore = require("Source.Locale.Core")
local WindowCommand = require("Source.Windows.WindowCommand")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@class Source.Windows.WindowShopCommandController
local WindowShopCommandController = {}

function WindowShopCommandController.CreateCommands(owner)
    return {
        {
            key = "Buy",
            text = LOC("SHOP_BUY"),
            callback = function (_obj, _kwargs)
                owner:confirmCommand()
            end
        },
        {
            key = "Sell",
            text = LOC("SHOP_SELL"),
            callback = function (_obj, _kwargs)
                owner:confirmCommand()
            end
        }
    }
end

return class(WindowShopCommandController, WindowCommand.Controller)
