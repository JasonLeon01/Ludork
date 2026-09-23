local _METADATA = {
    ConditionalActor = {
        moduleReturn = true,
        attrs = {
            "conditionVariable",
            "conditionOperator",
            "conditionValue"
        },
        bases = {
            { "Engine", "Actor" }
        },
        conditionVariable = {
            type = "string",
            default = "",
            Meta = {
                InstVar = { types = { "int", "float", "bool", "string" } }
            }
        },
        conditionOperator = {
            type = "string",
            default = "==",
            Meta = {
                DropBox = { "==", "~=", ">", ">=", "<", "<=" }
            }
        },
        conditionValue = {
            type = "any",
            default = 0,
            Meta = {
                InstVarValue = "conditionVariable"
            }
        }
    }
}

return _METADATA
