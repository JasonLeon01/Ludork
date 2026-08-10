local _METADATA = {
    Item = {
        attrs = {
            "ID",
            "count",
            "getSE",
        },
        bases = {
            { "Engine", "Actor" },
            { "Source.Infos.ItemInfo", "ItemInfo" },
        },
        ID = {
            type = "string",
            default = "",
            Meta = {
                GeneralDataVars = "Item",
            },
        },
        count = {
            type = "int",
            default = 1,
        },
        getSE = {
            type = "string",
            default = "",
            Meta = {
                PathVars = "Sounds",
                ConfigVars = { "Audio", "getSE" },
            },
        },
        Meta = {
            GeneralDataVars = {
                { "ID", "Item" },
            },
            PathVars = {
                { "getSE", "Sounds" },
            },
            ConfigVars = {
                { "getSE", "Audio", "getSE" },
            },
        },
    },
}

return _METADATA
