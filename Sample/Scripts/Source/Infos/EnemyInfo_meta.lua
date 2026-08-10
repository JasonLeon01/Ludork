local _METADATA = {
    EnemyInfo = {
        attrs = {},
        bases = {
            { "Engine", "InfoBase" },
        },
        onDefeat = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
    },
}

return _METADATA
