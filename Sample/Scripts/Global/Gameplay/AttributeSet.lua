local AttributeSet = {}

AttributeSet.ATTRIBUTE_NAMES = {}
AttributeSet.SCHEMA = {}
AttributeSet.ID = ""

function AttributeSet:init(values)
    values = values or {}
    local attributeType = Class.type(self)
    for _, name in ipairs(attributeType.ATTRIBUTE_NAMES or {}) do
        local schema = assert(attributeType.SCHEMA[name], "Attribute schema is missing for " .. name)
        local value = rawget(values, name)
        if value == nil then
            value = schema.default
        end
        self[name] = deepcopy(value)
    end
    self.ID = deepcopy(rawget(values, "ID") or attributeType.ID or "")
end

function AttributeSet:getAttributeNames()
    return copy(Class.type(self).ATTRIBUTE_NAMES or {})
end

function AttributeSet:getAttributeSchema(name)
    return Class.type(self).SCHEMA[name]
end

return class(AttributeSet)
