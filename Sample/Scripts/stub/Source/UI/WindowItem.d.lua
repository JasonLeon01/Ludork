---@meta Source.UI.WindowItem

function WindowItemUI:init(model) end

function WindowItemUI:attach() end

function WindowItemUI:refresh() end

function WindowItemUI:refreshItems() end

function WindowItemUI:tick() end

function WindowItemUI:wrapDescription(text) end

function WindowItemUI:updateDescription() end

function WindowItemUI:open() end

---@param onHidden function | nil
function WindowItemUI:close(onHidden) end

return WindowItemUI
