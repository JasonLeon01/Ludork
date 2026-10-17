local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local ComponentsFunctions = GlobalFunctions.Components
local TextureManager = GlobalCore.TextureManager

local BlueprintActorOverrides = {}

function BlueprintActorOverrides.ResolveValue(actorType, key, value, descriptor, configResolver)
    if Class.isInstance(value, "string") and not bool(value) then
        local configName, settingName = Engine.resolveConfigVar(actorType, key)
        if configName ~= nil then
            local resolved = configResolver(configName, settingName)
            return Class.isInstance(resolved, "string") and resolved or tostring(resolved)
        end
    end
    return deepcopy(Engine.resolveTypedDataValue(value, descriptor.type, nil, descriptor.module))
end

function BlueprintActorOverrides.IsBlueprintOnly(descriptor)
    local fieldMetadata = descriptor ~= nil and descriptor.metadata or nil
    local value = fieldMetadata ~= nil and fieldMetadata.Meta ~= nil and fieldMetadata.Meta.BlueprintOnly or nil
    return value == true
end

local function applyTexture(actor, applyRect)
    local texturePath = actor.texturePath
    if texturePath == nil then
        texturePath = ""
    end
    local texture = bool(texturePath) and TextureManager.load(texturePath) or nil
    if texture ~= nil then
        actor:setTexture(texture, true)
    end
    local rect = actor.defaultRect
    if rect ~= nil and applyRect then
        actor:setTextureRect(rect)
    end
end

local function applyComponentChange(actor, componentName, componentType, value)
    assert(Class.isInstance(value, "table"), "Blueprint component override " .. componentName .. " must be a table")
    local component = actor[componentName]
    assert(
        Class.isInstance(component, componentType),
        "Blueprint component field " .. componentName .. " is not an instance of its declared type"
    )
    local fieldDefaults = ComponentsFunctions.getComponentFieldDefaults(componentType)
    for fieldName, fieldValue in pairs(value) do
        assert(
            Class.isInstance(fieldName, "string")
                and (fieldDefaults[fieldName] ~= nil or Engine.resolveAttrMetadata(componentType, fieldName) ~= nil),
            "Unknown component member " .. tostring(fieldName) .. " in " .. componentName
        )
        local descriptor = Engine.resolveAttrMetadata(componentType, fieldName)
        local resolvedValue = fieldValue
        if descriptor ~= nil then
            resolvedValue = Engine.resolveTypedDataValue(fieldValue, descriptor.type, nil, descriptor.module)
        end
        Engine.setRuntimeTypedAttribute(component, fieldName, deepcopy(resolvedValue))
    end
end

function BlueprintActorOverrides.ApplyGeneration(actor)
    actor:setTranslation(actor.defaultTranslation)
    actor:setRotation(actor.defaultRotation)
    actor:setScale(actor.defaultScale)
    actor:setOrigin(actor.defaultOrigin)
end

function BlueprintActorOverrides.ApplyChanges(actor, changes, configResolver)
    assert(Class.isInstance(changes, "table"), "Blueprint instance overrides must be a table")
    local actorType = Class.type(actor)
    local descriptors = {}
    for key in pairs(changes) do
        assert(Class.isInstance(key, "string"), "Blueprint instance overrides must use string keys")
        local descriptor = Engine.resolveAttrMetadata(actorType, key)
        assert(descriptor ~= nil, "Undeclared Blueprint instance override '" .. key .. "'")
        descriptors[key] = descriptor
    end
    ---@type Global.Data.GeneratedActor
    local generatedActor = actor
    local storedChanges = generatedActor.classVarChanges
    if storedChanges == nil then
        storedChanges = {}
    else
        storedChanges = deepcopy(storedChanges)
    end
    local componentTypes = ComponentsFunctions.getComponentTypes(actorType)
    for _, key in ipairs(table.orderedStringKeys(changes, { "material", "materialParameters" })) do
        local value = changes[key]
        if Class.isInstance(key, "string") then
            local descriptor = descriptors[key]
            if not BlueprintActorOverrides.IsBlueprintOnly(descriptor) then
                storedChanges[key] = deepcopy(value)
                local componentType = componentTypes[key]
                if componentType ~= nil then
                    applyComponentChange(actor, key, componentType, value)
                else
                    local resolved = BlueprintActorOverrides.ResolveValue(
                        actorType, key, value, descriptor, configResolver
                    )
                    if key == "material" and actor.material ~= resolved then
                        storedChanges.materialParameters = nil
                    end
                    Engine.setRuntimeTypedAttribute(actor, key, resolved)
                end
            end
        end
    end
    if bool(storedChanges) then
        generatedActor.classVarChanges = storedChanges
    end
    actor:normaliseAutoSoundParams()
    if changes.texturePath ~= nil or changes.defaultRect ~= nil then
        applyTexture(actor, true)
    end
end

return BlueprintActorOverrides
