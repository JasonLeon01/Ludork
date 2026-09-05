local _METADATA = {
    Scene = {
        attrs = {},
        GotoMap = {
            type = "function",
            parameters = {
                "mapPath",
                "blockTransition",
                "position",
                mapPath = "string",
                blockTransition = "bool",
                position = "sf.Vector2i"
            },
            default = {
                [1] = "",
                [2] = false
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                Transfer = {
                    {
                        "position",
                        "mapPath"
                    }
                }
            }
        },
        GameOver = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddTimer = {
            type = "function",
            parameters = {
                "interval",
                "blocking",
                interval = "float",
                blocking = "bool"
            },
            default = {
                [2] = false
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "TimeUp",
                TimeUp = {
                    true
                }
            }
        },
        ShowMessageByTag = {
            type = "function",
            parameters = {
                "name",
                "message",
                "refActorTag",
                name = "string",
                message = "string",
                refActorTag = "string"
            },
            default = {
                [3] = ""
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "FinishedDialogue",
                FinishedDialogue = {
                    true
                }
            }
        },
        ShowMessage = {
            type = "function",
            parameters = {
                "name",
                "message",
                "actor",
                name = "string",
                message = "string",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "FinishedDialogue",
                FinishedDialogue = {
                    true
                }
            }
        },
        ShowVoiceMessageByTag = {
            type = "function",
            parameters = {
                "name",
                "message",
                "voiceFileName",
                "refActorTag",
                name = "string",
                message = "string",
                voiceFileName = "string",
                refActorTag = "string"
            },
            default = {
                [4] = ""
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "FinishedDialogue",
                FinishedDialogue = {
                    true
                }
            },
            Meta = {
                PathVars = {
                    {
                        "voiceFileName",
                        "/Game/Assets/Voices"
                    }
                }
            }
        },
        ShowVoiceMessage = {
            type = "function",
            parameters = {
                "name",
                "message",
                "voiceFileName",
                "refActor",
                "minDistance",
                name = "string",
                message = "string",
                voiceFileName = "string",
                refActor = {
                    "Engine",
                    "Actor"
                },
                minDistance = "float"
            },
            default = {
                [5] = 64.0
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "FinishedDialogue",
                FinishedDialogue = {
                    true
                }
            },
            Meta = {
                PathVars = {
                    {
                        "voiceFileName",
                        "/Game/Assets/Voices"
                    }
                }
            }
        },
        ShowSelection = {
            type = "function",
            parameters = {
                "name",
                "options",
                "refActorTag",
                "allowCancel",
                name = "string",
                options = "string[]",
                refActorTag = "string",
                allowCancel = "bool"
            },
            default = {
                [1] = "",
                [2] = {},
                [3] = "",
                [4] = true
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "Selected0",
                "Selected1",
                "Selected2",
                "Selected3",
                "Cancelled",
                Selected0 = {
                    0
                },
                Selected1 = {
                    1
                },
                Selected2 = {
                    2
                },
                Selected3 = {
                    3
                },
                Cancelled = {
                    -1
                }
            }
        },
        ShowRefSelection = {
            type = "function",
            parameters = {
                "name",
                "options",
                "refActor",
                "allowCancel",
                name = "string",
                options = "string[]",
                refActor = {
                    "Engine",
                    "Actor"
                },
                allowCancel = "bool"
            },
            default = {
                [1] = "",
                [2] = {},
                [4] = true
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "Selected0",
                "Selected1",
                "Selected2",
                "Selected3",
                "Cancelled",
                Selected0 = {
                    0
                },
                Selected1 = {
                    1
                },
                Selected2 = {
                    2
                },
                Selected3 = {
                    3
                },
                Cancelled = {
                    -1
                }
            }
        },
        LockCamera = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        UnlockCamera = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AttachCamera = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        MoveCamera = {
            type = "function",
            parameters = {
                "delta",
                delta = "sf.Vector2f"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RecordTelepoint = {
            type = "function",
            parameters = {
                "mapPath",
                "x",
                "y",
                "tag",
                mapPath = "string",
                x = "int",
                y = "int",
                tag = "string"
            },
            default = {
                [1] = "",
                [2] = 0,
                [3] = 0,
                [4] = ""
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        CreateActorFromBPPath = {
            type = "function",
            parameters = {
                "bpPath",
                "layerName",
                "position",
                "tag",
                "emitCreateEvent",
                bpPath = "string",
                layerName = "string",
                position = "sf.Vector2i",
                tag = "string",
                emitCreateEvent = "bool"
            },
            default = {
                [1] = "",
                [2] = "default",
                [4] = "",
                [5] = true
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                BlueprintClassVars = {
                    "bpPath"
                }
            }
        },
        CreateActorFromBPPathWithDefaults = {
            type = "function",
            parameters = {
                "bpPath",
                "defaults",
                "layerName",
                "position",
                "tag",
                "emitCreateEvent",
                bpPath = "string",
                defaults = "any",
                layerName = "string",
                position = "sf.Vector2i",
                tag = "string",
                emitCreateEvent = "bool"
            },
            default = {
                [1] = "",
                [2] = {},
                [3] = "default",
                [5] = "",
                [6] = true
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                BlueprintClassVars = {
                    "bpPath"
                }
            }
        },
        DestroyTerrain = {
            type = "function",
            parameters = {
                "layerName",
                "position",
                "tileID",
                layerName = "string",
                position = "sf.Vector2i",
                tileID = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        DestroyTerrainList = {
            type = "function",
            parameters = {
                "layerName",
                "positions",
                "tileID",
                layerName = "string",
                positions = "sf.Vector2i[]",
                tileID = "any"
            },
            default = {
                [2] = {}
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        GetTerrainTile = {
            type = "function",
            parameters = {
                "layerName",
                "position",
                layerName = "string",
                position = "sf.Vector2i"
            },
            ["return"] = {
                "tileID",
                tileID = "any"
            },
            Pure = true
        },
        GetTerrainTilePositions = {
            type = "function",
            parameters = {
                "layerName",
                "tileID",
                layerName = "string",
                tileID = "any"
            },
            ["return"] = {
                "positions",
                positions = "sf.Vector2i[]"
            },
            Pure = true
        },
        RecordAddedActor = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SelfRecordAdded = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RecordActorPosition = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SelfRecordActorPosition = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RecordDestroyedActor = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SelfRecordDestroyed = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RecordAndDestroyActor = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        SelfRecordAndDestroy = {
            type = "function",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        OpenShop = {
            type = "function",
            parameters = {
                "items",
                "canSell",
                items = "string[]",
                canSell = "bool"
            },
            default = {
                [1] = {},
                [2] = true
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "Closed",
                Closed = {
                    true
                }
            }
        },
        OpenAttrShop = {
            type = "function",
            parameters = {
                "actor",
                "shopName",
                "shopDescription",
                "abilities",
                "price",
                "priceIncrement",
                "moneyName",
                actor = {
                    "Engine",
                    "Actor"
                },
                shopName = "string",
                shopDescription = "string",
                abilities = "any",
                price = "any",
                priceIncrement = "int",
                moneyName = "string"
            },
            default = {
                [2] = "",
                [3] = "",
                [4] = {},
                [5] = 0,
                [6] = 1,
                [7] = "GOLD"
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "Closed",
                Closed = {
                    true
                }
            }
        },
        OpenAttrShopByTag = {
            type = "function",
            parameters = {
                "actorTag",
                "shopName",
                "shopDescription",
                "abilities",
                "price",
                "priceIncrement",
                "moneyName",
                actorTag = "string",
                shopName = "string",
                shopDescription = "string",
                abilities = "any",
                price = "any",
                priceIncrement = "int",
                moneyName = "string"
            },
            default = {
                [1] = "",
                [2] = "",
                [3] = "",
                [4] = {},
                [5] = 0,
                [6] = 1,
                [7] = "GOLD"
            },
            ["return"] = {
                "return",
                ["return"] = "function"
            },
            Latent = {
                "Closed",
                Closed = {
                    true
                }
            }
        }
    }
}

return _METADATA
