local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local MainConfig = require("Source.Configs.Main")
local Locale = require("Source.Locale.Core")

local NodeGraphFunctions = GlobalFunctions.NodeGraph
local GlobalSystem = GlobalCore.System

local APP_NAME = "Ludork Sample"
SAVE_AS_LDC = false

-- Entry point.
local function entry()
    local SceneInit = require("Source.Scenes.SceneInit")
    local SourceSystem = require("Source.System")

    Engine.setAppName(APP_NAME)
    Logging.info("Starting %s", APP_NAME)
    local iniFilePath, iniFile = MainConfig.loadOrCreate()
    Locale.setLanguage(Locale.ResolveLanguage(iniFile:get("Main", "language", "")))
    NodeGraphFunctions.initLatent()
    GlobalSystem.init(iniFile, iniFilePath)
    Locale.setLanguage(Locale.ResolveLanguage(GlobalSystem.getLanguage()))
    GlobalSystem.setScene(SceneInit.new())
    SourceSystem.init()
    GlobalSystem.run()
    Logging.info("Game exited successfully.")
end

entry()
