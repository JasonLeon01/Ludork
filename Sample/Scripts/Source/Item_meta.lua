local _METADATA = {
    Item = {
        attrs = {
            "ID",
            "count",
            "getSE"
        },
        bases = {
            { "Engine", "Actor" }
        },
        ID = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "Item"
            }
        },
        count = {
            type = "int",
            default = 1
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
                { "ID", "Item" }
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
