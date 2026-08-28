local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")

local ComponentsFunctions = GlobalFunctions.Components
local ManagerFunctions = GlobalFunctions.Manager

local BlueprintActorOverrides = {}

local function resolveValue(actorType, key, value, descriptor)
    if type(value) == "string" and not bool(value) then
        local configName, settingName = Engine.resolveConfigVar(actorType, key)
        if configName ~= nil then
            local SourceSystem = require("Source.System")

            local resolved = SourceSystem.GetConfigValue(configName, settingName)
            return type(resolved) == "string" and resolved or tostring(resolved)
        end
    end
    local targetType
    local declaringModule
    if descriptor == nil then
        targetType = Engine.resolveAttrValueType(actorType, key)
    else
        targetType = descriptor.type
        declaringModule = descriptor.module
    end
    if type(value) == "string" and Engine.shouldEvalValueType(targetType) then
        return Engine.evalDataExpression(value)
    end
    if targetType ~= "any" then
        return deepcopy(Engine.resolveTypedDataValue(value, targetType, nil, declaringModule))
    end
    return deepcopy(value)
end

local function isBlueprintOnly(descriptor)
    local fieldMetadata = descriptor ~= nil and descriptor.metadata or nil
    local value = fieldMetadata ~= nil and fieldMetadata.Meta ~= nil and fieldMetadata.Meta.BlueprintOnly or nil
    return value == true
end

local function applyTexture(actor, applyRect)
    local texturePath = actor.texturePath
    if texturePath == nil then
        texturePath = ""
    end
    local texture = bool(texturePath) and ManagerFunctions.loadCharacter(texturePath) or nil
    if texture ~= nil then
        actor:setTexture(texture, true)
    end
    local rect = actor.defaultRect
    if rect ~= nil and applyRect then
        actor:setTextureRect(rect)
    end
end

local function normaliseObjects(actor)
    if actor.material ~= nil and not Class.isInstance(actor.material, Engine.Material) then
        actor.material = Engine.Material.fromData(actor.material)
    end
    actor:normaliseAutoSoundParams()
end

local function applyComponentChange(actor, componentName, componentType, value)
    assert(type(value) == "table", "Blueprint component override " .. componentName .. " must be a table")
    local component = actor[componentName]
    assert(
        Class.isInstance(component, componentType),
        "Blueprint component field " .. componentName .. " is not an instance of its declared type"
    )
    local fieldDefaults = ComponentsFunctions.getComponentFieldDefaults(componentType)
    for fieldName, fieldValue in pairs(value) do
        assert(
            type(fieldName) == "string" and fieldDefaults[fieldName] ~= nil,
            "Unknown component member " .. tostring(fieldName) .. " in " .. componentName
        )
        component[fieldName] = ComponentsFunctions._cloneComponentFieldValue(componentType, fieldName, fieldValue)
    end
end

function BlueprintActorOverrides.ApplyGeneration(actor)
    actor:setTranslation(actor.defaultTranslation)
    actor:setRotation(actor.defaultRotation)
    actor:setScale(actor.defaultScale)
    actor:setOrigin(actor.defaultOrigin)
end

function BlueprintActorOverrides.ApplyChanges(actor, changes)
    ---@type Source.Data.GeneratedActor
    local generatedActor = actor
    local storedChanges = generatedActor._classVarChanges
    if storedChanges == nil then
        storedChanges = {}
    else
        storedChanges = deepcopy(storedChanges)
    end
    local actorType = Class.type(actor)
    local componentTypes = ComponentsFunctions.getComponentTypes(actorType)
    for key, value in pairs(changes) do
        if type(key) == "string" then
            local descriptor = Engine.resolveAttrMetadata(actorType, key)
            if not isBlueprintOnly(descriptor) then
                storedChanges[key] = deepcopy(value)
                local componentType = componentTypes[key]
                if componentType ~= nil then
                    applyComponentChange(actor, key, componentType, value)
                else
                    actor[key] = resolveValue(actorType, key, value, descriptor)
                end
            end
        end
    end
    if bool(storedChanges) then
        generatedActor._classVarChanges = storedChanges
    end
    normaliseObjects(actor)
    if changes.shaderPath ~= nil then
        actor:setShaderPath(actor.shaderPath or "")
    end
    if changes.texturePath ~= nil or changes.defaultRect ~= nil then
        applyTexture(actor, true)
    end
end

return BlueprintActorOverrides
