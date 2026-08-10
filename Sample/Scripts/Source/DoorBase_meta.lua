local _METADATA = {
    DoorBase = {
        attrs = {
            "collisionEnabled",
            "tickable",
            "openInterval",
            "gateSE",
            "opening",
            "closing",
        },
        bases = {
            { "Engine", "Actor" },
        },
        collisionEnabled = {
            type = "bool",
            default = true,
        },
        tickable = {
            type = "bool",
            default = true,
        },
        openInterval = {
            type = "float",
            default = 0.05,
        },
        gateSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "Sounds",
                ConfigVars = { "Audio", "gateSE" },
            },
        },
        opening = {
            type = "bool",
            default = false,
        },
        closing = {
            type = "bool",
            default = false,
        },
        openDoor = {
            type = "function",
            parameters = {},
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "Started",
                "Finished",
                Started = { 0 },
                Finished = { 1 },
            },
        },
        closeDoor = {
            type = "function",
            parameters = {},
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "Started",
                "Finished",
                Started = { 0 },
                Finished = { 1 },
            },
        },
        onTick = {
            type = "event",
            parameters = {
                "deltaTime",
                deltaTime = "float",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        Meta = {
            PathVars = {
                { "gateSE", "Sounds" },
            },
            ConfigVars = {
                { "gateSE", "Audio", "gateSE" },
            },
        },
    },
}

return _METADATA
