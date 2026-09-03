local _METADATA = {
    Enemy = {
        attrs = {
            "ID",
            "childActorComp",
            "collisionEnabled",
            "animatable",
            "animateWithoutMoving",
            "afterBattleVarChanges"
        },
        bases = {
            { "Engine", "Actor" },
            { "Source.Battler", "Battler" }
        },
        ID = {
            type = "string",
            default = "FILL_IT_BY_YOURSELF",
            Meta = {
                GeneralDataVars = "Enemy"
            }
        },
        childActorComp = {
            type = {
                "Source.Components.ChildActorComponent",
                "ChildActorComponent"
            },
            component = true,
            default = {
                className = "Source.EnemyDamageText.EnemyDamageText",
                relativePosition = { 0.0, 0.0 }
            }
        },
        collisionEnabled = {
            type = "bool",
            default = true
        },
        animatable = {
            type = "bool",
            default = true
        },
        animateWithoutMoving = {
            type = "bool",
            default = true
        },
        afterBattleVarChanges = {
            type = "Dict[string, Tuple[string, any]]",
            default = {},
            Meta = {
                DictKeyMeta = {
                    InstVar = {
                        types = {
                            "int",
                            "float"
                        }
                    }
                },
                ItemMeta = {
                    TupleMeta = {
                        [1] = {
                            DropBox = {
                                "=",
                                "+",
                                "-",
                                "*",
                                "/",
                                "//",
                                "%",
                                "**"
                            }
                        },
                        [2] = {
                            InstVarValue = "$dictKey"
                        }
                    }
                }
            }
        },
        onCollision = {
            type = "event",
            parameters = {
                "other",
                other = { "Engine", "Actor[]" }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        onDefeat = {
            type = "event",
            parameters = {},
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        Meta = {
            GeneralDataVars = {
                { "ID", "Enemy" }
            }
        }
    }
}

return _METADATA
