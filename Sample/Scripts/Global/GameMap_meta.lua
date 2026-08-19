local _METADATA = {
    GameMap = {
        attrs = {
            "DefaultCoverAlpha",
            "MapViewOffset",
        },
        DefaultCoverAlpha = {
            type = "int",
        },
        MapViewOffset = {
            type = "sf.Vector2f",
        },
        getPlayer = {
            type = "function",
            parameters = {},
            ["return"] = {
                "player",
                player = {
                    "Engine",
                    "Actor",
                },
            },
            Pure = true,
        },
        setPlayer = {
            type = "function",
            parameters = {
                "player",
                player = {
                    "Engine",
                    "Actor",
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getAllActors = {
            type = "function",
            parameters = {},
            ["return"] = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]",
                },
            },
            Pure = true,
        },
        getActorsByPosition = {
            type = "function",
            parameters = {
                "position",
                position = "sf.Vector2i",
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
        getActorByLayerAndPosition = {
            type = "function",
            parameters = {
                "layer",
                "position",
                layer = "string",
                position = "sf.Vector2i",
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
        getActorsByRange = {
            type = "function",
            parameters = {
                "position",
                "radius",
                position = "sf.Vector2i",
                radius = "int",
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
        getActorByTag = {
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
        isPassable = {
            type = "function",
            parameters = {
                "actor",
                "targetPosition",
                actor = {
                    "Engine",
                    "Actor",
                },
                targetPosition = "sf.Vector2i",
            },
            ["return"] = {
                "passable",
                passable = "bool",
            },
            Pure = true,
        },
        spawnActor = {
            type = "function",
            parameters = {
                "actor",
                "layer",
                "emitCreateEvent",
                actor = {
                    "Engine",
                    "Actor",
                },
                layer = "string",
                emitCreateEvent = "bool",
            },
            default = {
                [3] = true,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        createActor = {
            type = "function",
            parameters = {
                "actorClass",
                "layer",
                "kwargs",
                "emitCreateEvent",
                actorClass = {
                    "Engine",
                    "Actor",
                },
                layer = "string",
                kwargs = "any",
                emitCreateEvent = "bool",
            },
            default = {
                [4] = true,
            },
            ["return"] = {
                "actor",
                actor = {
                    "Engine",
                    "Actor",
                },
            },
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        destroyActor = {
            type = "function",
            parameters = {
                "actor",
                actor = {
                    "Engine",
                    "Actor",
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getCamera = {
            type = "function",
            parameters = {},
            ["return"] = {
                "camera",
                camera = { "GlobalCore", "Camera" },
            },
            Pure = true,
        },
        setCamera = {
            type = "function",
            parameters = {
                "camera",
                camera = { "GlobalCore", "Camera" },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getTilemap = {
            type = "function",
            parameters = {},
            ["return"] = {
                "tilemap",
                tilemap = { "Engine", "Tilemap" },
            },
            Pure = true,
        },
        getTerrainTile = {
            type = "function",
            parameters = {
                "layerName",
                "position",
                layerName = "string",
                position = "sf.Vector2i",
            },
            ["return"] = {
                "tileID",
                tileID = "any",
            },
            Pure = true,
        },
        getTerrainTilePositions = {
            type = "function",
            parameters = {
                "layerName",
                "tileID",
                layerName = "string",
                tileID = "any",
            },
            ["return"] = {
                "positions",
                positions = "sf.Vector2i[]",
            },
            Pure = true,
        },
        setTerrainTile = {
            type = "function",
            parameters = {
                "layerName",
                "position",
                "tileID",
                layerName = "string",
                position = "sf.Vector2i",
                tileID = "any",
            },
            ["return"] = {
                "success",
                success = "bool",
            },
            Pure = true,
        },
        setTerrainTiles = {
            type = "function",
            parameters = {
                "layerName",
                "positions",
                "tileID",
                layerName = "string",
                positions = "sf.Vector2i[]",
                tileID = "any",
            },
            ["return"] = {
                "positions",
                positions = "sf.Vector2i[]",
            },
            Pure = true,
        },
        getLights = {
            type = "function",
            parameters = {},
            ["return"] = {
                "lights",
                lights = {
                    "GlobalCore",
                    "Light[]",
                },
            },
            Pure = true,
        },
        setLights = {
            type = "function",
            parameters = {
                "lights",
                lights = {
                    "GlobalCore",
                    "Light[]",
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        addLight = {
            type = "function",
            parameters = {
                "light",
                light = {
                    "GlobalCore",
                    "Light",
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        removeLight = {
            type = "function",
            parameters = {
                "light",
                light = {
                    "GlobalCore",
                    "Light",
                },
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        setLightPosition = {
            type = "function",
            parameters = {
                "light",
                "position",
                light = {
                    "GlobalCore",
                    "Light",
                },
                position = "sf.Vector2f",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        setLightColour = {
            type = "function",
            parameters = {
                "light",
                "colour",
                light = {
                    "GlobalCore",
                    "Light",
                },
                colour = "sf.Color",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        setLightRadius = {
            type = "function",
            parameters = {
                "light",
                "radius",
                light = {
                    "GlobalCore",
                    "Light",
                },
                radius = "float",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        setLightIntensity = {
            type = "function",
            parameters = {
                "light",
                "intensity",
                light = {
                    "GlobalCore",
                    "Light",
                },
                intensity = "float",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getAmbientLight = {
            type = "function",
            parameters = {},
            ["return"] = {
                "ambientLight",
                ambientLight = "sf.Color",
            },
            Pure = true,
        },
        setAmbientLight = {
            type = "function",
            parameters = {
                "ambientLight",
                ambientLight = "sf.Color",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        getSize = {
            type = "function",
            parameters = {},
            ["return"] = {
                "size",
                size = "sf.Vector2u",
            },
            Pure = true,
        },
        getTopMaterial = {
            type = "function",
            parameters = {
                "pos",
                pos = "sf.Vector2i",
            },
            ["return"] = {
                "topMaterial",
                topMaterial = {
                    "Engine",
                    "Material",
                },
            },
            Pure = true,
        },
        findPath = {
            type = "function",
            parameters = {
                "start",
                "goal",
                "actor",
                "excludedAnchors",
                start = "sf.Vector2i",
                goal = "sf.Vector2i",
                actor = {
                    "Engine",
                    "Actor",
                },
                excludedAnchors = "sf.Vector2i[]",
            },
            default = {
                [4] = {},
            },
            ["return"] = {
                "path",
                path = "sf.Vector2i[]",
            },
            Pure = true,
        },
        isPathfindingPassable = {
            type = "function",
            parameters = {
                "actor",
                "targetPosition",
                actor = {
                    "Engine",
                    "Actor",
                },
                targetPosition = "sf.Vector2i",
            },
            ["return"] = {
                "passable",
                passable = "bool",
            },
            Pure = true,
        },
        hasPathBlockingOverlapActor = {
            type = "function",
            parameters = {
                "actor",
                "targetPosition",
                actor = {
                    "Engine",
                    "Actor",
                },
                targetPosition = "sf.Vector2i",
            },
            ["return"] = {
                "hasActor",
                hasActor = "bool",
            },
            Pure = true,
        },
        getScene = {
            type = "function",
            parameters = {},
            ["return"] = {
                "scene",
                scene = { "GlobalCore", "SceneBase" },
            },
            Pure = true,
        },
        addCommonTip = {
            type = "function",
            parameters = {
                "text",
                text = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        addDamageText = {
            type = "function",
            parameters = {
                "text",
                "position",
                text = "string",
                position = "sf.Vector2f",
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
