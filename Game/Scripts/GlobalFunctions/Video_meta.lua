local _METADATA = {
    Video = {
        attrs = {},
        PlayVideo = {
            type = "function",
            parameters = {
                "videoFileName",
                "mute",
                "skipable",
                "subtitleFileName",
                videoFileName = "string",
                mute = "bool",
                skipable = "bool",
                subtitleFileName = "string"
            },
            default = {
                [2] = false,
                [3] = true,
                [4] = ""
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
                    },
                    {
                        "subtitleFileName",
                        "/Game/Assets/Subtitles"
                    }
                }
            }
        }
    }
}

return _METADATA
