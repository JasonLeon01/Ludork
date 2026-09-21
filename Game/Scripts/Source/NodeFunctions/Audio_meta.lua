local _METADATA = {
    Audio = {
        attrs = {},
        EditSoundFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
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
                        "attenuation"
                    }
                }
            }
        },
        EditMusicFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
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
                        "loopPoint"
                    }
                }
            }
        },
        SetEffect = {
            type = "function",
            parameters = {
                "audioType",
                "effect",
                audioType = "string",
                effect = "string"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                DropBox = {
                    audioType = {
                        "Sound",
                        "Voice",
                        "Music"
                    },
                    effect = {
                        "nil",
                        "Echo",
                        "Distortion",
                        "Underwater",
                        "BehindWall"
                    }
                }
            }
        },
        PlaySound = {
            type = "function",
            parameters = {
                "soundFileName",
                "applyFilter",
                soundFileName = "string",
                applyFilter = "bool"
            },
            default = {
                [2] = false
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                PathVars = {
                    {
                        "soundFileName",
                        "/Game/Assets/Sounds"
                    }
                }
            }
        },
        PlayMusic = {
            type = "function",
            parameters = {
                "musicFileName",
                "applyFilter",
                musicFileName = "string",
                applyFilter = "bool"
            },
            default = {
                [2] = false
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                PathVars = {
                    {
                        "musicFileName",
                        "/Game/Assets/Musics"
                    }
                }
            }
        },
        SetBgmFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
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
                        "loopPoint"
                    }
                }
            }
        },
        SetBgsFilter = {
            type = "function",
            parameters = {
                "attr",
                "value",
                attr = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
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
                        "attenuation"
                    }
                }
            }
        }
    }
}

return _METADATA
