local _METADATA = {
    Scene = {
        attrs = {},
        bases = {
            { "GlobalCore", "SceneBase" },
        },
        getGameMap = {
            type = "function",
            parameters = {},
            ["return"] = {
                "gameMap",
                gameMap = { "Global.GameMap", "GameMap" },
            },
            Pure = true,
        },
        showMessage = {
            type = "function",
            parameters = {
                "name",
                "message",
                "refActor",
                "localeArgs",
                name = "string",
                message = "string",
                refActor = { "Engine", "Actor" },
                localeArgs = "Dict[string, any]",
            },
            default = {
                [4] = {},
            },
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "FinishedDialogue",
                FinishedDialogue = { true },
            },
        },
        showSelection = {
            type = "function",
            parameters = {
                "name",
                "options",
                "refActor",
                "allowCancel",
                "localeArgs",
                name = "string",
                options = "string[]",
                refActor = { "Engine", "Actor" },
                allowCancel = "bool",
                localeArgs = "Dict[string, any]",
            },
            default = {
                [4] = true,
                [5] = {},
            },
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "Selected0",
                "Selected1",
                "Selected2",
                "Selected3",
                "Cancelled",
                Selected0 = { 0 },
                Selected1 = { 1 },
                Selected2 = { 2 },
                Selected3 = { 3 },
                Cancelled = { -1 },
            },
        },
        applyLoadedGame = {
            type = "function",
            parameters = {
                "inst",
                inst = { "Source.GameInstance", "GameInstance" },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        showEnemyBook = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        showFloorTeleporter = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        openMenu = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        recordAddedActor = {
            type = "function",
            parameters = {
                "actor",
                actor = { "Engine", "Actor" },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        recordActorPosition = {
            type = "function",
            parameters = {
                "actor",
                actor = { "Engine", "Actor" },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        recordDestroyedActor = {
            type = "function",
            parameters = {
                "actor",
                actor = { "Engine", "Actor" },
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
