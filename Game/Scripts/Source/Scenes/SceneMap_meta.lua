local _METADATA = {
    Scene = {
        moduleReturn = true,
        attrs = {},
        bases = {
            { "Source.Gameplay.GameplayScene", "GameplayScene" }
        },
        getGameMap = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.Scenes.SceneMap", "Scene" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "gameMap",
                gameMap = { "Global.GameMap", "GameMap" }
            },
            Pure = true
        },
        openPlayerName = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.Scenes.SceneMap", "Scene" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = { "return", ["return"] = "function" },
            Latent = true,
            LatentStates = { "Closed", Closed = { true } }
        },
        showMessage = {
            type = "function",
            parameters = {
                "self",
                "name",
                "message",
                "refActor",
                "localeArgs",
                self = { "Source.Scenes.SceneMap", "Scene" },
                name = "string",
                message = "string",
                refActor = { "Engine", "Actor" },
                localeArgs = "Dict[string, any]"
            },
            default = {
                [1] = "self",
                [5] = {}
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = true,
            LatentStates = {
                "FinishedDialogue",
                FinishedDialogue = { true }
            }
        },
        showSelection = {
            type = "function",
            parameters = {
                "self",
                "name",
                "options",
                "refActor",
                "allowCancel",
                "localeArgs",
                self = { "Source.Scenes.SceneMap", "Scene" },
                name = "string",
                options = "string[]",
                refActor = { "Engine", "Actor" },
                allowCancel = "bool",
                localeArgs = "Dict[string, any]"
            },
            default = {
                [1] = "self",
                [5] = true,
                [6] = {}
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = true,
            LatentStates = {
                "Selected0",
                "Selected1",
                "Selected2",
                "Selected3",
                "Cancelled",
                Selected0 = { 0 },
                Selected1 = { 1 },
                Selected2 = { 2 },
                Selected3 = { 3 },
                Cancelled = { -1 }
            }
        },
        applyLoadedGame = {
            type = "function",
            parameters = {
                "self",
                "inst",
                self = { "Source.Scenes.SceneMap", "Scene" },
                inst = { "Source.GameInstance", "GameInstance" }
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
        showEnemyBook = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.Scenes.SceneMap", "Scene" }
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
        showFloorTeleporter = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.Scenes.SceneMap", "Scene" }
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
        openMenu = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.Scenes.SceneMap", "Scene" }
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
        recordAddedActor = {
            type = "function",
            parameters = {
                "self",
                "actor",
                self = { "Source.Scenes.SceneMap", "Scene" },
                actor = { "Engine", "Actor" }
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
        recordActorPosition = {
            type = "function",
            parameters = {
                "self",
                "actor",
                "position",
                self = { "Source.Scenes.SceneMap", "Scene" },
                actor = { "Engine", "Actor" },
                position = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            defaultUnset = { "position" },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        recordDestroyedActor = {
            type = "function",
            parameters = {
                "self",
                "actor",
                self = { "Source.Scenes.SceneMap", "Scene" },
                actor = { "Engine", "Actor" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        }
    }
}

return _METADATA
