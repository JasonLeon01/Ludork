local _METADATA = {
    Enemy = {
        moduleReturn = true,
        attrs = {
            "ID",
            "childActorComp",
            "collisionEnabled",
            "animatable",
            "animateWithoutMoving",
            "afterBattleVarChanges"
        },
        bases = {
            { "Source.MapActors.ConditionalActor", "ConditionalActor" },
            { "Source.Battler", "Battler" }
        },
        ID = {
            type = { enum = "Enums.GeneralData.Enemy", valueType = "string" },
            default = "FILL_IT_BY_YOURSELF"
        },
        childActorComp = {
            type = {
                "Source.Components.ChildActorComponent",
                "ChildActorComponent"
            },
            component = true,
            default = {
                className = "Source.MapActors.EnemyDamageText",
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
        }
    }
}

return _METADATA
