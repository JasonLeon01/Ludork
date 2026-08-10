local _METADATA = {
    StateInfo = {
        attrs = {},
        bases = {
            { "Engine", "InfoBase" },
        },
        onWalk = {
            type = "event",
            parameters = {
                "battler",
                battler = { "Source.Battler", "Battler" },
            },
            default = {
                nil,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        onHookTriggered = {
            type = "event",
            parameters = {
                "battler",
                battler = { "Source.Battler", "Battler" },
            },
            default = {
                nil,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
    },
}

return _METADATA
