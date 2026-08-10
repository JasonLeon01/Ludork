local _METADATA = {
    ConditionDoor = {
        attrs = {
            "openConditionName",
            "openConditionVal",
        },
        openConditionName = {
            type = "string",
            default = "",
        },
        openConditionVal = {
            type = "int",
            default = 0,
        },
    },
}

return _METADATA
