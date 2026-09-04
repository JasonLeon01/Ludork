local _METADATA = {
    Bottle = {
        attrs = {
            "HP_plus",
            "getSE",
        },
        HP_plus = {
            type = "int",
            default = 0,
        },
        getSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "/Game/Assets/Sounds",
                ConfigVars = { "Audio", "getSE" },
            },
        },
        Meta = {
            PathVars = {
                { "getSE", "/Game/Assets/Sounds" },
            },
            ConfigVars = {
                { "getSE", "Audio", "getSE" },
            },
        },
    },
}

return _METADATA
