local _METADATA = {
    Gem = {
        attrs = {
            "ATTR_key",
            "plus",
            "getSE",
        },
        ATTR_key = {
            type = "string",
            default = "",
        },
        plus = {
            type = "int",
            default = 0,
        },
        getSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "Sounds",
                ConfigVars = { "Audio", "getSE" },
            },
        },
        Meta = {
            PathVars = {
                { "getSE", "Sounds" },
            },
            ConfigVars = {
                { "getSE", "Audio", "getSE" },
            },
        },
    },
}

return _METADATA
