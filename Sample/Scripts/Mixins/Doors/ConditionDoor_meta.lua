local _METADATA = {
    ConditionDoor = {
        attrs = {
            "openConditionName",
            "openConditionVal",
        },
        openConditionName = {
            type = "string",
            default = "",
            Meta = {
                InstVar = {
                    types = {
                        "int",
                        "float",
                    },
                },
            },
        },
        openConditionVal = {
            type = "int",
            default = 0,
            Meta = {
                InstVarValue = "openConditionName",
            },
        },
    },
}

return _METADATA
