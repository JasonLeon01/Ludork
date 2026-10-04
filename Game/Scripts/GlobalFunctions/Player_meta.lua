local _METADATA = {
    Player = {
        attrs = {},
        GetPlayer = {
            type = "function",
            parameters = {},
            ["return"] = {
                "player",
                player = {
                    "Source.MapActors.Player",
                    "Player"
                }
            },
            Pure = true
        },
        GetPlayerFrontPosition = {
            type = "function",
            parameters = {},
            ["return"] = {
                "position",
                position = "sf.Vector2i"
            },
            Pure = true
        },
        AddItem = {
            type = "function",
            parameters = {
                "itemID",
                "count",
                itemID = { enum = "Enums.GeneralData.Item", valueType = "string" },
                count = "int"
            },
            default = {
                [2] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RemoveItem = {
            type = "function",
            parameters = {
                "itemID",
                "count",
                itemID = { enum = "Enums.GeneralData.Item", valueType = "string" },
                count = "int"
            },
            default = {
                [2] = 1
            },
            ["return"] = {
                "return",
                ["return"] = "int"
            },
            ExecSplit = {
                "Success",
                "Failed",
                Success = {
                    0
                },
                Failed = {
                    1
                }
            }
        },
        HasItem = {
            type = "function",
            parameters = {
                "itemID",
                itemID = { enum = "Enums.GeneralData.Item", valueType = "string" }
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        GetItemCount = {
            type = "function",
            parameters = {
                "itemID",
                itemID = { enum = "Enums.GeneralData.Item", valueType = "string" }
            },
            ["return"] = {
                "count",
                count = "int"
            },
            Pure = true
        },
        AddEquip = {
            type = "function",
            parameters = {
                "equipID",
                "count",
                equipID = { enum = "Enums.GeneralData.Equip", valueType = "string" },
                count = "int"
            },
            default = {
                [2] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        RemoveEquip = {
            type = "function",
            parameters = {
                "equipID",
                "count",
                equipID = { enum = "Enums.GeneralData.Equip", valueType = "string" },
                count = "int"
            },
            default = {
                [2] = 1
            },
            ["return"] = {
                "return",
                ["return"] = "int"
            },
            ExecSplit = {
                "Success",
                "Failed",
                Success = {
                    0
                },
                Failed = {
                    1
                }
            }
        },
        HasEquip = {
            type = "function",
            parameters = {
                "equipID",
                equipID = { enum = "Enums.GeneralData.Equip", valueType = "string" }
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        EquipItem = {
            type = "function",
            parameters = {
                "equipID",
                equipID = { enum = "Enums.GeneralData.Equip", valueType = "string" }
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        UnequipSlot = {
            type = "function",
            parameters = {
                "slotID",
                slotID = "string"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        GetEquipInSlot = {
            type = "function",
            parameters = {
                "slotID",
                slotID = "string"
            },
            ["return"] = {
                "equipID",
                equipID = "string"
            },
            Pure = true
        },
        GetPlayerAttr = {
            type = "function",
            parameters = {
                "attrName",
                attrName = "string"
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        SetPlayerAttr = {
            type = "function",
            parameters = {
                "attrName",
                "value",
                attrName = "string",
                value = "any"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        GetPlayerAttrRef = {
            type = "function",
            parameters = {
                "attrName",
                attrName = "string"
            },
            ["return"] = {
                "value",
                value = "any"
            },
            Pure = true
        },
        HealPlayer = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        DamagePlayer = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddHP = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddGold = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddATK = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddDEF = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        AddEXP = {
            type = "function",
            parameters = {
                "amount",
                amount = "int"
            },
            default = {
                [1] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        MeetPlayer = {
            type = "function",
            parameters = {
                "actors",
                actors = {
                    "Engine",
                    "Actor[]"
                }
            },
            ["return"] = {
                "playerInfo",
                playerInfo = {
                    union = {
                        { "Source.MapActors.Player", "Player" },
                        "nil"
                    }
                }
            },
            Pure = true
        }
    }
}

return _METADATA
