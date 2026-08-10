local _METADATA = {
    BattlerInfoComponent = {
        attrs = {
            "MAXHP",
            "ATK",
            "DEF",
            "EXP",
            "GOLD",
            "ANIMATION_KEY",
            "HP",
        },
        bases = {
            { "Engine", "Component" },
        },
        MAXHP = {
            type = "int",
            default = 1000,
        },
        ATK = {
            type = "int",
            default = 10,
        },
        DEF = {
            type = "int",
            default = 10,
        },
        EXP = {
            type = "int",
            default = 0,
        },
        GOLD = {
            type = "int",
            default = 0,
        },
        ANIMATION_KEY = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "ANIMATION",
            },
        },
        HP = {
            type = "int",
            default = 0,
        },
        Meta = {
            GeneralDataVars = {
                { "ANIMATION_KEY", "ANIMATION" },
            },
        },
    },
}

return _METADATA
