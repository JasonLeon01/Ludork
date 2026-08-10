local _METADATA = {
    GameMap = {
        attrs = {},
        GetActorByTag = {
            type = "function",
            parameters = {
                "tag",
                tag = "string",
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor",
                },
            },
            Pure = true,
        },
        GetAllActors = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]",
                },
            },
            Pure = true,
        },
    },
}

return _METADATA
