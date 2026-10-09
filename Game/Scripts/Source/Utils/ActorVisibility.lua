local ActorVisibility = {}

function ActorVisibility.Evaluate(variable, operator, comparison, authoredVisible, variables)
    assert(Class.isInstance(variable, "string"), "ConditionalActor variable name must be a string")
    if variable == "" then
        return authoredVisible
    end
    local current = variables[variable]
    assert(current ~= nil, "ConditionalActor variable is missing: " .. variable)
    local isNumber = Class.isInstance(current, "number") and Class.isInstance(comparison, "number")
    local isBoolean = Class.isInstance(current, "boolean") and Class.isInstance(comparison, "boolean")
    local isString = Class.isInstance(current, "string") and Class.isInstance(comparison, "string")
    assert(
        isNumber or isBoolean or isString,
        "ConditionalActor comparison requires matching number, boolean or string values"
    )
    ---@cast operator string
    local visible
    if operator == "==" then
        visible = current == comparison
    elseif operator == "~=" then
        visible = current ~= comparison
    else
        assert(isNumber, "ConditionalActor ordering requires numeric values")
        ---@cast current number
        ---@cast comparison number
        if operator == ">" then
            visible = current > comparison
        elseif operator == ">=" then
            visible = current >= comparison
        elseif operator == "<" then
            visible = current < comparison
        elseif operator == "<=" then
            visible = current <= comparison
        else
            error("Unsupported ConditionalActor operator: " .. tostring(operator))
        end
    end
    return visible
end

return ActorVisibility
