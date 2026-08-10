local _METADATA = {
    PlayerInfoComponent = {
        attrs = {
            "name",
            "desc",
            "LEVEL",
            "CLASS",
        },
        bases = {
            { "Source.Components.BattlerInfoComponent", "BattlerInfoComponent" },
        },
        name = {
            type = "string",
            default = "",
        },
        desc = {
            type = "string",
            default = "",
        },
        LEVEL = {
            type = "int",
            default = 1,
        },
        CLASS = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "Class",
            },
        },
        Meta = {
            GeneralDataVars = {
                { "CLASS", "Class" },
            },
        },
    },
}

return _METADATA
