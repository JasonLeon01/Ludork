local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local FunctionalImage = Engine.FunctionalImage
local TextureManager = GlobalCore.TextureManager

local IconTexture = {}

function IconTexture.Load(iconPath)
    if not bool(iconPath) then
        return nil
    end
    return TextureManager.load(iconPath)
end

function IconTexture.Apply(controller, nodeName, texture)
    if texture == nil then
        controller:setProperty(nodeName, "visible", false)
        return false
    end
    local icon = controller:requireControl(nodeName)
    assert(Class.isInstance(icon, FunctionalImage), "Icon control must be a FunctionalImage: " .. nodeName)
    ---@cast icon Engine.FunctionalImage
    icon:setTexture(texture, true)
    controller:setProperty(nodeName, "visible", true)
    return true
end

return IconTexture
