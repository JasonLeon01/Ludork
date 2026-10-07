local _METADATA = {
    Movement = {
        attrs = {},
        SetMoveEnabledByTag = {
            type = "function",
            parameters = {
                "tag",
                "enabled",
                tag = "string",
                enabled = "bool"
            },
            default = {
                [2] = true
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SetMoveRoute = {
            type = "function",
            parameters = {
                "actor",
                "route",
                actor = {
                    "Engine",
                    "Actor"
                },
                route = "sf.Vector2i[]"
            },
            default = {
                [2] = {}
            },
            ["return"] = {
                "return",
                ["return"] = { "Engine", "AsyncOperation" }
            },
            Latent = true,
            LatentStates = {
                "Started",
                "Finished",
                Started = {
                    0
                },
                Finished = {
                    1
                }
            },
            Meta = {
                MoveRouteVars = {
                    "route"
                }
            }
        },
        SetAutoPathToDestination = {
            type = "function",
            parameters = {
                "actor",
                "destination",
                actor = {
                    "Engine",
                    "Actor"
                },
                destination = "sf.Vector2i"
            },
            default = {
                [2] = {
                    0,
                    0
                }
            },
            ["return"] = {
                "return",
                ["return"] = { "Engine", "AsyncOperation" }
            },
            Latent = true,
            LatentStates = {
                "Started",
                "Finished",
                Started = {
                    0
                },
                Finished = {
                    1
                }
            }
        },
        SetAutoPathToDestinationByTag = {
            type = "function",
            parameters = {
                "tag",
                "destination",
                tag = "string",
                destination = "sf.Vector2i"
            },
            default = {
                [2] = {
                    0,
                    0
                }
            },
            ["return"] = {
                "return",
                ["return"] = { "Engine", "AsyncOperation" }
            },
            Latent = true,
            LatentStates = {
                "Started",
                "Finished",
                Started = {
                    0
                },
                Finished = {
                    1
                }
            }
        }
    }
}

return _METADATA
