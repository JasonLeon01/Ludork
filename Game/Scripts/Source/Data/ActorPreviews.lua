local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local BlueprintActorOverrides = require("Source.Data.BlueprintActorOverrides")

local ComponentsFunctions = GlobalFunctions.Components
local Actor = Engine.Actor

local displayFields = {
    texturePath = "texturePath",
    defaultRect = "rect",
    defaultTranslation = "translation",
    defaultRotation = "rotation",
    defaultScale = "scale",
    defaultOrigin = "origin",
    visible = "visible",
    hue = "hue",
    shaderPath = "shaderPath",
    conditionVariable = "conditionVariable",
    conditionOperator = "conditionOperator",
    conditionValue = "conditionValue"
}

---@class Source.Data.ActorPreviews
local ActorPreviews = {}

function ActorPreviews:init(classDict, resolveClassPath)
    self._classDict = classDict
    self._resolveClassPath = resolveClassPath
    self:clear()
end

function ActorPreviews:clear()
    self._templates = {}
end

---@param source Source.Data.ActorPreviewData
---@return Source.Data.ActorPreviewData
local function copyData(source)
    return {
        texturePath = source.texturePath,
        rect = source.rect:copy(),
        translation = source.translation:copy(),
        rotation = source.rotation,
        scale = source.scale:copy(),
        origin = source.origin:copy(),
        visible = source.visible,
        hue = source.hue,
        shaderPath = source.shaderPath,
        conditionVariable = source.conditionVariable,
        conditionOperator = source.conditionOperator,
        conditionValue = source.conditionValue,
        childOffset = source.childOffset:copy()
    }
end

function ActorPreviews:_getTemplate(classPath)
    if self._templates[classPath] ~= nil then
        return self._templates[classPath]
    end
    local classModel = self._classDict:get(classPath)
    if classModel == nil then
        return nil
    end
    assert(Class.isSubclass(classModel, Actor), "Actor preview class must derive from Engine.Actor: " .. classPath)
    local ConditionalActor = require("Source.MapActors.ConditionalActor")

    local conditional = Class.isSubclass(classModel, ConditionalActor)
    ---@type Source.Data.ActorPreviewData
    local data = {
        texturePath = classModel.texturePath or "",
        rect = classModel.defaultRect:copy(),
        translation = classModel.defaultTranslation:copy(),
        rotation = classModel.defaultRotation,
        scale = classModel.defaultScale:copy(),
        origin = classModel.defaultOrigin:copy(),
        visible = classModel.visible,
        hue = classModel.hue,
        shaderPath = classModel.shaderPath or "",
        conditionVariable = conditional and classModel.conditionVariable or "",
        conditionOperator = conditional and classModel.conditionOperator or "==",
        conditionValue = conditional and classModel.conditionValue or 0,
        childOffset = sf.Vector2f.new(0.0, 0.0)
    }
    if conditional then
        data.conditionValue = classModel.conditionValue
    end
    local childClassName = ""
    local componentType = ComponentsFunctions.getComponentTypes(classModel).childActorComp
    if componentType ~= nil then
        local ChildActorComponent = require("Source.Components.ChildActorComponent")

        if Class.isSubclass(componentType, ChildActorComponent) then
            local component = classModel.childActorComp
            local defaults = ComponentsFunctions.getComponentFieldDefaults(componentType)
            childClassName = component ~= nil and component.className or defaults.className
            local offset = component ~= nil and component.relativePosition or defaults.relativePosition
            data.childOffset = offset:copy()
        else
            componentType = nil
        end
    end
    ---@type Source.Data.ActorPreviewTemplate
    local template = {
        classModel = classModel,
        conditional = conditional,
        data = data,
        childClassName = childClassName,
        componentType = componentType
    }
    self._templates[classPath] = template
    return template
end

---@param template Source.Data.ActorPreviewTemplate
---@param data     Source.Data.ActorPreviewData
---@param changes  table<string, Source.Data.ClassVarValue>
---@return string
local function applyChildChanges(template, data, changes)
    assert(Class.isInstance(changes, "table"), "Blueprint component override childActorComp must be a table")
    local componentType = assert(template.componentType)
    local defaults = ComponentsFunctions.getComponentFieldDefaults(componentType)
    local childClassName = template.childClassName
    for fieldName, value in pairs(changes) do
        local descriptor = Engine.resolveAttrMetadata(componentType, fieldName)
        assert(
            Class.isInstance(fieldName, "string") and (defaults[fieldName] ~= nil or descriptor ~= nil),
            "Unknown component member " .. tostring(fieldName) .. " in childActorComp"
        )
        if fieldName == "className" or fieldName == "relativePosition" then
            local resolved = descriptor ~= nil
                and Engine.resolveTypedDataValue(value, descriptor.type, nil, descriptor.module)
                or value
            if fieldName == "className" then
                childClassName = resolved
            else
                data.childOffset = resolved:copy()
            end
        end
    end
    return childClassName
end

function ActorPreviews:_get(classPath, classVarChanges, ancestors)
    classPath = self._resolveClassPath(classPath)
    if not bool(classPath) then
        return nil
    end
    assert(not ancestors[classPath], "Cyclic child Actor preview reference: " .. classPath)
    local template = self:_getTemplate(classPath)
    if template == nil then
        return nil
    end
    local data = copyData(template.data)
    local childClassName = template.childClassName
    if classVarChanges ~= nil then
        assert(Class.isInstance(classVarChanges, "table"), "Blueprint instance overrides must be a table")
        for key, value in pairs(classVarChanges) do
            assert(Class.isInstance(key, "string"), "Blueprint instance overrides must use string keys")
            local descriptor = Engine.resolveAttrMetadata(template.classModel, key)
            assert(descriptor ~= nil, "Undeclared Blueprint instance override '" .. key .. "'")
            if not BlueprintActorOverrides.IsBlueprintOnly(descriptor) then
                local target = displayFields[key]
                local conditionField = key == "conditionVariable" or key == "conditionOperator"
                    or key == "conditionValue"
                if target ~= nil and (not conditionField or template.conditional) then
                    data[target] = BlueprintActorOverrides.ResolveValue(template.classModel, key, value, descriptor)
                elseif key == "childActorComp" and template.componentType ~= nil then
                    assert(
                        Class.isInstance(value, "table"), "Blueprint component override childActorComp must be a table"
                    )
                    ---@cast value table<string, Source.Data.ClassVarValue>
                    childClassName = applyChildChanges(template, data, value)
                end
            end
        end
    end
    childClassName = string.strip(childClassName)
    if bool(childClassName) then
        ancestors[classPath] = true
        data.child = self:_get(childClassName, nil, ancestors)
        ancestors[classPath] = nil
    end
    return data
end

function ActorPreviews:get(classPath, classVarChanges)
    return self:_get(classPath, classVarChanges, {})
end

return class(ActorPreviews)
