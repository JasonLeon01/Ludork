local _METADATA = {
    Equip = {
        attrs = {
            "ID",
            "getSE"
        },
        bases = {
            { "Engine", "Actor" }
        },
        ID = {
            type = "string",
            default = "FILL_IT_BY_YOURSELF",
            Meta = {
                GeneralDataVars = "Equip"
            }
        },
        getSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "/Game/Assets/Sounds",
                ConfigVars = { "Audio", "getSE" }
            }
        },
        Meta = {
            GeneralDataVars = {
                { "ID", "Equip" }
            },
            PathVars = {
                { "getSE", "/Game/Assets/Sounds" }
            },
            ConfigVars = {
                { "getSE", "Audio", "getSE" }
            }
        }
    }
}

return _METADATA
