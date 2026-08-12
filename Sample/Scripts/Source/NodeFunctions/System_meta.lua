local _METADATA = {
    System = {
        attrs = {},
        EditSoundFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    attr = {
                        "loop",
                        "offset",
                        "pitch",
                        "pan",
                        "volume",
                        "spatial",
                        "position",
                        "direction",
                        "cone",
                        "velocity",
                        "dopplerFactor",
                        "directionalAttenuationFactor",
                        "relativeToListener",
                        "minDistance",
                        "maxDistance",
                        "minGain",
                        "maxGain",
                        "attenuation",
                    },
                },
            },
        },
        EditMusicFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    attr = {
                        "loop",
                        "offset",
                        "pitch",
                        "pan",
                        "volume",
                        "spatial",
                        "position",
                        "direction",
                        "cone",
                        "velocity",
                        "dopplerFactor",
                        "directionalAttenuationFactor",
                        "relativeToListener",
                        "minDistance",
                        "maxDistance",
                        "minGain",
                        "maxGain",
                        "attenuation",
                        "loopPoint",
                    },
                },
            },
        },
        PlaySound = {
            type = "function",
            parameters = {
                "soundFileName",
                "applyFilter",
                soundFileName = "string",
                applyFilter = "bool",
            },
            default = {
                [2] = false,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                PathVars = {
                    {
                        "soundFileName",
                        "Sounds",
                    },
                },
            },
        },
        PlayMusic = {
            type = "function",
            parameters = {
                "musicFileName",
                "applyFilter",
                musicFileName = "string",
                applyFilter = "bool",
            },
            default = {
                [2] = false,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                PathVars = {
                    {
                        "musicFileName",
                        "Musics",
                    },
                },
            },
        },
        PlayVideo = {
            type = "function",
            parameters = {
                "videoFileName",
                "mute",
                "skipable",
                videoFileName = "string",
                mute = "bool",
                skipable = "bool",
            },
            default = {
                [2] = false,
                [3] = true,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                PathVars = {
                    {
                        "videoFileName",
                        "Videos",
                    },
                },
            },
        },
        FreezeTransitionBackground = {
            type = "function",
            parameters = {
            },
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "Frozen",
                Frozen = {
                    true,
                },
            },
        },
        RequestTransition = {
            type = "function",
            parameters = {
                "transitionName",
                "transitionTime",
                transitionName = "string",
                transitionTime = "float",
            },
            default = {
                [1] = "",
                [2] = 1.0,
            },
            ["return"] = {
                "return",
                ["return"] = "function",
            },
            Latent = {
                "Finished",
                Finished = {
                    true,
                },
            },
            Meta = {
                PathVars = {
                    {
                        "transitionName",
                        "Transitions",
                    },
                },
            },
        },
        SetBgmFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    attr = {
                        "loop",
                        "offset",
                        "pitch",
                        "pan",
                        "volume",
                        "spatial",
                        "position",
                        "direction",
                        "cone",
                        "velocity",
                        "dopplerFactor",
                        "directionalAttenuationFactor",
                        "relativeToListener",
                        "minDistance",
                        "maxDistance",
                        "minGain",
                        "maxGain",
                        "attenuation",
                        "loopPoint",
                    },
                },
            },
        },
        SetBgsFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    attr = {
                        "loop",
                        "offset",
                        "pitch",
                        "pan",
                        "volume",
                        "spatial",
                        "position",
                        "direction",
                        "cone",
                        "velocity",
                        "dopplerFactor",
                        "directionalAttenuationFactor",
                        "relativeToListener",
                        "minDistance",
                        "maxDistance",
                        "minGain",
                        "maxGain",
                        "attenuation",
                    },
                },
            },
        },
        SetEffect = {
            type = "function",
            parameters = {
                "audioType",
                "effect",
                audioType = "string",
                effect = "string",
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    audioType = {
                        "Sound",
                        "Voice",
                        "Music",
                    },
                    effect = {
                        "nil",
                        "Echo",
                        "Distortion",
                        "Underwater",
                        "BehindWall",
                    },
                },
            },
        },
        FlashScreen = {
            type = "function",
            parameters = {
                "red",
                "green",
                "blue",
                "alpha",
                "duration",
                red = "int",
                green = "int",
                blue = "int",
                alpha = "int",
                duration = "float",
            },
            default = {
                [1] = 255,
                [2] = 255,
                [3] = 255,
                [4] = 255,
                [5] = 0.5,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        StopFlashScreen = {
            type = "function",
            parameters = {
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ChangeScreenTone = {
            type = "function",
            parameters = {
                "red",
                "green",
                "blue",
                "gray",
                "duration",
                red = "int",
                green = "int",
                blue = "int",
                gray = "int",
                duration = "float",
            },
            default = {
                [1] = 0,
                [2] = 0,
                [3] = 0,
                [4] = 0,
                [5] = 0.0,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ClearScreenTone = {
            type = "function",
            parameters = {
                "duration",
                duration = "float",
            },
            default = {
                [1] = 0.0,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        ScreenShake = {
            type = "function",
            parameters = {
                "power",
                "speed",
                "duration",
                power = "float",
                speed = "float",
                duration = "float",
            },
            default = {
                [1] = 4.0,
                [2] = 10.0,
                [3] = 0.5,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        StopScreenShake = {
            type = "function",
            parameters = {
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
        },
        SetWeather = {
            type = "function",
            parameters = {
                "weatherType",
                "power",
                "maxCount",
                weatherType = "string",
                power = "int",
                maxCount = "int",
            },
            default = {
                [2] = 40,
                [3] = 80,
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil",
            },
            Meta = {
                DropBox = {
                    weatherType = {
                        "LOC(\"WEATHER_TYPE_NONE\")",
                        "LOC(\"WEATHER_TYPE_RAIN\")",
                        "LOC(\"WEATHER_TYPE_STORM\")",
                        "LOC(\"WEATHER_TYPE_SNOW\")",
                    },
                },
            },
        },
        ClearWeather = {
            type = "function",
            parameters = {
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
