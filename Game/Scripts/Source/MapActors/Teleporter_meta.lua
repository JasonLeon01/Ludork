local _METADATA = {
    Teleporter = {
        moduleReturn = true,
        attrs = {
            "Offset",
            "stairSE",
            "transitionName",
            "transitionTime"
        },
        bases = {
            { "Source.MapActors.ConditionalActor", "ConditionalActor" }
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
            parameters = {
                "self",
                self = { "Source.MapActors.Teleporter", "Teleporter" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        goDownstairs = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.MapActors.Teleporter", "Teleporter" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        goToMap = {
            type = "function",
            parameters = {
                "self",
                "mapPath",
                "position",
                "record",
                self = { "Source.MapActors.Teleporter", "Teleporter" },
                mapPath = "string",
                position = "sf.Vector2i",
                record = "bool"
            },
            default = {
                [1] = "self",
                [4] = true
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                Transfer = {
                    { "position", "mapPath" }
                }
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
