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
                PathVars = "/Game/Assets/Sounds",
                ConfigVars = { "Audio", "stairSE" }
            }
        },
        transitionName = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "/Game/Assets/Transitions"
            }
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
                { "stairSE", "/Game/Assets/Sounds" },
                { "transitionName", "/Game/Assets/Transitions" }
            },
            ConfigVars = {
                { "stairSE", "Audio", "stairSE" }
            }
        }
    }
}

return _METADATA
