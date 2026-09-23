local _METADATA = {
    Player = {
        moduleReturn = true,
        attrs = {
            "ID",
            "tickable",
            "collisionEnabled",
            "animatable",
            "speed"
        },
        bases = {
            { "Engine", "Character" },
            { "Source.Battler", "Battler" }
        },
        ID = {
            type = "string",
            default = "FILL_IT_BY_YOURSELF",
            Meta = {
                GeneralDataVars = "Player"
            }
        },
        tickable = {
            type = "bool",
            default = true
        },
        collisionEnabled = {
            type = "bool",
            default = true
        },
        animatable = {
            type = "bool",
            default = true
        },
        speed = {
            type = "float",
            default = 96.0
        },
        onFixedTick = {
            type = "event",
            parameters = {
                "fixedDelta",
                fixedDelta = "float"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            }
        },
        getDisplayName = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.MapActors.Player", "Player" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = { "name", name = "string" },
            Pure = true
        },
        setName = {
            type = "function",
            parameters = {
                "self",
                "name",
                self = { "Source.MapActors.Player", "Player" },
                name = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = { "success", success = "bool" },
            ExecSplit = { "Success", "Invalid", Success = true, Invalid = false }
        },
        addItem = {
            type = "function",
            parameters = {
                "self",
                "itemID",
                "count",
                self = { "Source.MapActors.Player", "Player" },
                itemID = "string",
                count = "int"
            },
            default = {
                [1] = "self",
                [3] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" }
                }
            }
        },
        removeItem = {
            type = "function",
            parameters = {
                "self",
                "itemID",
                "count",
                self = { "Source.MapActors.Player", "Player" },
                itemID = "string",
                count = "int"
            },
            default = {
                [1] = "self",
                [3] = 1
            },
            ["return"] = {
                "return",
                ["return"] = "bool"
            },
            ExecSplit = {
                "success",
                "failed",
                success = { true },
                failed = { false }
            },
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" }
                }
            }
        },
        getItemCount = {
            type = "function",
            parameters = {
                "self",
                "itemID",
                self = { "Source.MapActors.Player", "Player" },
                itemID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "count",
                count = "int"
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" }
                }
            }
        },
        hasItem = {
            type = "function",
            parameters = {
                "self",
                "itemID",
                self = { "Source.MapActors.Player", "Player" },
                itemID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "itemID", "Item" }
                }
            }
        },
        addEquip = {
            type = "function",
            parameters = {
                "self",
                "equipID",
                "count",
                self = { "Source.MapActors.Player", "Player" },
                equipID = "string",
                count = "int"
            },
            default = {
                [1] = "self",
                [3] = 1
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" }
                }
            }
        },
        removeEquip = {
            type = "function",
            parameters = {
                "self",
                "equipID",
                "count",
                self = { "Source.MapActors.Player", "Player" },
                equipID = "string",
                count = "int"
            },
            default = {
                [1] = "self",
                [3] = 1
            },
            ["return"] = {
                "return",
                ["return"] = "bool"
            },
            ExecSplit = {
                "success",
                "failed",
                success = { true },
                failed = { false }
            },
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" }
                }
            }
        },
        equip = {
            type = "function",
            parameters = {
                "self",
                "equipID",
                self = { "Source.MapActors.Player", "Player" },
                equipID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" }
                }
            }
        },
        unequip = {
            type = "function",
            parameters = {
                "self",
                "slotID",
                self = { "Source.MapActors.Player", "Player" },
                slotID = "string"
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
        getEquipCount = {
            type = "function",
            parameters = {
                "self",
                "equipID",
                self = { "Source.MapActors.Player", "Player" },
                equipID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "count",
                count = "int"
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" }
                }
            }
        },
        hasEquip = {
            type = "function",
            parameters = {
                "self",
                "equipID",
                self = { "Source.MapActors.Player", "Player" },
                equipID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true,
            Meta = {
                GeneralDataVars = {
                    { "equipID", "Equip" }
                }
            }
        },
        getEquipInfo = {
            type = "function",
            parameters = {
                "self",
                "slotID",
                self = { "Source.MapActors.Player", "Player" },
                slotID = "string"
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "value",
                value = "string"
            },
            Pure = true
        },
        getForbiddenMoving = {
            type = "function",
            parameters = {
                "self",
                self = { "Source.MapActors.Player", "Player" }
            },
            default = {
                [1] = "self"
            },
            ["return"] = {
                "value",
                value = "bool"
            },
            Pure = true
        },
        setForbiddenMoving = {
            type = "function",
            parameters = {
                "self",
                "value",
                self = { "Source.MapActors.Player", "Player" },
                value = "bool"
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
        Meta = {
            GeneralDataVars = {
                { "ID", "Player" }
            }
        }
    }
}

return _METADATA
