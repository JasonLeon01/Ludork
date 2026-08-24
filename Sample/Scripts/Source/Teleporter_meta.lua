local _METADATA = {
    Teleporter = {
        attrs = {
            "Offset",
            "stairSE",
            "transitionName",
            "transitionTime"
        },
        bases = {
            { "Engine", "Actor" }
        },
        Offset = {
            type = "sf.Vector2i",
            default = { 0, 0 }
        },
        stairSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "Sounds",
                ConfigVars = { "Audio", "stairSE" }
            }
        },
        transitionName = {
            type = "string",
            default = ""
        },
        transitionTime = {
            type = "float",
            default = 0.5
        },
        goUpstairs = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        goDownstairs = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        Meta = {
            PathVars = {
                { "stairSE", "Sounds" }
            },
            ConfigVars = {
                { "stairSE", "Audio", "stairSE" }
            }
        }
    }
}

return _METADATA
