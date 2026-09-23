local _METADATA = {
    Video = {
        attrs = {},
        PlayVideo = {
            type = "function",
            parameters = {
                "videoFileName",
                "mute",
                "skipable",
                videoFileName = "string",
                mute = "bool",
                skipable = "bool"
            },
            default = {
                [2] = false,
                [3] = true
            },
            ["return"] = {},
            ExecSplit = {
                "default",
                default = "nil"
            },
            Meta = {
                PathVars = {
                    {
                        "videoFileName",
                        "/Game/Assets/Videos"
                    }
                }
            }
        }
    }
}

return _METADATA
