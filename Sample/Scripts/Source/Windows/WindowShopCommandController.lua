local WindowCommand = require("Source.Windows.WindowCommand")

---@class Source.Windows.WindowShopCommandController
local WindowShopCommandController = {}

function WindowShopCommandController.CreateCommands(owner)
    return {
        {
            key = "Buy",
            localeKey = "SHOP_BUY",
            callback = function (_obj, _kwargs)
                owner:confirmCommand()
            end
        },
        {
            key = "Sell",
            localeKey = "SHOP_SELL",
            callback = function (_obj, _kwargs)
                owner:confirmCommand()
            end
        }
    }
end

return class(WindowShopCommandController, WindowCommand.Controller)
