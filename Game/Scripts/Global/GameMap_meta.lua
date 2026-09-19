local _METADATA = {
    GameMap = {
        moduleReturn = true,
        attrs = {
            "DefaultCoverAlpha",
            "MapViewRect",
            "HideDisconnectedRegions"
        },
        bases = {
            { "GlobalCore", "GameMapBase" }
        },
        DefaultCoverAlpha = {
            type = "int"
        },
        HideDisconnectedRegions = {
            type = "bool",
            default = false
        },
        MapViewRect = {
            type = "sf.IntRect",
            default = {
                { 192, 32, 416, 416 }
            }
        },
        getPlayer = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "player",
                player = {
                    "Engine",
                    "Actor"
                }
            },
            Pure = true
        },
        setPlayer = {
            type = "function",
            parameters = {
                "self",
                "player",
                self = { "Global.GameMap", "GameMap" },
                player = {
                    "Engine",
                    "Actor"
                }
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
        getAllActors = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]"
                }
            },
            Pure = true
        },
        getActorsByPosition = {
            type = "function",
            parameters = {
                "self",
                "position",
                self = { "Global.GameMap", "GameMap" },
                position = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]"
                }
            },
            Pure = true
        },
        getActorByLayerAndPosition = {
            type = "function",
            parameters = {
                "self",
                "layer",
                "position",
                self = { "Global.GameMap", "GameMap" },
                layer = "string",
                position = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            Pure = true
        },
        getActorsByRange = {
            type = "function",
            parameters = {
                "self",
                "position",
                "radius",
                self = { "Global.GameMap", "GameMap" },
                position = "sf.Vector2i",
                radius = "int"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]"
                }
            },
            Pure = true
        },
        getActorByTag = {
            type = "function",
            parameters = {
                "self",
                "tag",
                self = { "Global.GameMap", "GameMap" },
                tag = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor"
                }
            },
            Pure = true
        },
        isPassable = {
            type = "function",
            parameters = {
                "self",
                "actor",
                "targetPosition",
                self = { "Global.GameMap", "GameMap" },
                actor = {
                    "Engine",
                    "Actor"
                },
                targetPosition = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "passable",
                passable = "bool"
            },
            Pure = true
        },
        spawnActor = {
            type = "function",
            parameters = {
                "self",
                "actor",
                "layer",
                "emitCreateEvent",
                self = { "Global.GameMap", "GameMap" },
                actor = {
                    "Engine",
                    "Actor"
                },
                layer = "string",
                emitCreateEvent = "bool"
            },
            default = {
                [1] = "self",
                [4] = true
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        createActor = {
            type = "function",
            parameters = {
                "self",
                "actorClass",
                "layer",
                "kwargs",
                "emitCreateEvent",
                self = { "Global.GameMap", "GameMap" },
                actorClass = {
                    "Engine",
                    "Actor"
                },
                layer = "string",
                kwargs = "any",
                emitCreateEvent = "bool"
            },
            default = {
                [1] = "self",
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
            }
        },
        destroyActor = {
            type = "function",
            parameters = {
                "self",
                "actor",
                self = { "Global.GameMap", "GameMap" },
                actor = {
                    "Engine",
                    "Actor"
                }
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
        getCamera = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "camera",
                camera = { "GlobalCore", "Camera" }
            },
            Pure = true
        },
        setCamera = {
            type = "function",
            parameters = {
                "self",
                "camera",
                self = { "Global.GameMap", "GameMap" },
                camera = { "GlobalCore", "Camera" }
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
        getTilemap = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "tilemap",
                tilemap = { "Engine", "Tilemap" }
            },
            Pure = true
        },
        getTerrainTile = {
            type = "function",
            parameters = {
                "self",
                "layerName",
                "position",
                self = { "Global.GameMap", "GameMap" },
                layerName = "string",
                position = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "tileID",
                tileID = "any"
            },
            Pure = true
        },
        getTerrainTilePositions = {
            type = "function",
            parameters = {
                "self",
                "layerName",
                "tileID",
                self = { "Global.GameMap", "GameMap" },
                layerName = "string",
                tileID = "any"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "positions",
                positions = "sf.Vector2i[]"
            },
            Pure = true
        },
        setTerrainTile = {
            type = "function",
            parameters = {
                "self",
                "layerName",
                "position",
                "tileID",
                self = { "Global.GameMap", "GameMap" },
                layerName = "string",
                position = "sf.Vector2i",
                tileID = "any"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "success",
                success = "bool"
            },
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        setTerrainTiles = {
            type = "function",
            parameters = {
                "self",
                "layerName",
                "positions",
                "tileID",
                self = { "Global.GameMap", "GameMap" },
                layerName = "string",
                positions = "sf.Vector2i[]",
                tileID = "any"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "positions",
                positions = "sf.Vector2i[]"
            },
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        getLights = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "lights",
                lights = {
                    "GlobalCore",
                    "Light[]"
                }
            },
            Pure = true
        },
        setLights = {
            type = "function",
            parameters = {
                "self",
                "lights",
                self = { "Global.GameMap", "GameMap" },
                lights = {
                    "GlobalCore",
                    "Light[]"
                }
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
        addLight = {
            type = "function",
            parameters = {
                "self",
                "light",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                }
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
        removeLight = {
            type = "function",
            parameters = {
                "self",
                "light",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                }
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
        setLightPosition = {
            type = "function",
            parameters = {
                "self",
                "light",
                "position",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                },
                position = "sf.Vector2f"
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
        setLightColour = {
            type = "function",
            parameters = {
                "self",
                "light",
                "colour",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                },
                colour = "sf.Color"
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
        setLightRadius = {
            type = "function",
            parameters = {
                "self",
                "light",
                "radius",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                },
                radius = "float"
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
        setLightIntensity = {
            type = "function",
            parameters = {
                "self",
                "light",
                "intensity",
                self = { "Global.GameMap", "GameMap" },
                light = {
                    "GlobalCore",
                    "Light"
                },
                intensity = "float"
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
        getAmbientLight = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "ambientLight",
                ambientLight = "sf.Color"
            },
            Pure = true
        },
        setAmbientLight = {
            type = "function",
            parameters = {
                "self",
                "ambientLight",
                self = { "Global.GameMap", "GameMap" },
                ambientLight = "sf.Color"
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
        getSize = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "size",
                size = "sf.Vector2u"
            },
            Pure = true
        },
        getTopMaterial = {
            type = "function",
            parameters = {
                "self",
                "pos",
                self = { "Global.GameMap", "GameMap" },
                pos = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "topMaterial",
                topMaterial = {
                    "Engine",
                    "Material"
                }
            },
            Pure = true
        },
        findPath = {
            type = "function",
            parameters = {
                "self",
                "start",
                "goal",
                "actor",
                "excludedAnchors",
                self = { "Global.GameMap", "GameMap" },
                start = "sf.Vector2i",
                goal = "sf.Vector2i",
                actor = {
                    "Engine",
                    "Actor"
                },
                excludedAnchors = "sf.Vector2i[]"
            },
            default = {
                [1] = "self",
                [5] = {}
            },
            ["return"] = {
                "path",
                path = "sf.Vector2i[]"
            },
            Pure = true
        },
        isPathfindingPassable = {
            type = "function",
            parameters = {
                "self",
                "actor",
                "targetPosition",
                self = { "Global.GameMap", "GameMap" },
                actor = {
                    "Engine",
                    "Actor"
                },
                targetPosition = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "passable",
                passable = "bool"
            },
            Pure = true
        },
        hasPathBlockingOverlapActor = {
            type = "function",
            parameters = {
                "self",
                "actor",
                "targetPosition",
                self = { "Global.GameMap", "GameMap" },
                actor = {
                    "Engine",
                    "Actor"
                },
                targetPosition = "sf.Vector2i"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "hasActor",
                hasActor = "bool"
            },
            Pure = true
        },
        getScene = {
            type = "function",
            parameters = {
                "self",
                self = { "Global.GameMap", "GameMap" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "scene",
                scene = { "GlobalCore", "SceneBase" }
            },
            Pure = true
        },
        addCommonTip = {
            type = "function",
            parameters = {
                "self",
                "text",
                self = { "Global.GameMap", "GameMap" },
                text = "string"
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
        addDamageText = {
            type = "function",
            parameters = {
                "self",
                "text",
                "position",
                "sourceActor",
                self = { "Global.GameMap", "GameMap" },
                text = "string",
                position = "sf.Vector2f",
                sourceActor = { "Engine", "Actor" }
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
