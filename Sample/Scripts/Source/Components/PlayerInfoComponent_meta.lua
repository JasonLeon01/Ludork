local _METADATA = {
    PlayerInfoComponent = {
        attrs = {
            "HP",
            "name",
            "desc",
            "LEVEL",
            "CLASS",
        },
        bases = {
            { "Source.Components.BattlerInfoComponent", "BattlerInfoComponent" },
        },
        HP = {
            type = "int",
            default = 0,
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
