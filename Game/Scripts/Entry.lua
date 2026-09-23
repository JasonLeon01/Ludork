local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local MainConfig = require("Source.Configs.Main")
local Locale = require("Source.Locale.Core")
local LiveDebug = require("Internal.LiveDebug")

local SceneManager = GlobalCore.SceneManager
local NodeGraphFunctions = GlobalFunctions.NodeGraph
local GlobalSystem = GlobalCore.System

local APP_NAME = "LudorkSample"

-- Entry point.
local function entry()
    local Data = require("Source.Data")
    local SceneInit = require("Source.Scenes.SceneInit")
    local SourceSystem = require("Source.System")

    Engine.setAppName(APP_NAME)
    Logging.info("Starting %s", APP_NAME)
    Locale.Init()
    local iniFilePath, iniFile = MainConfig.LoadOrCreate()
    Locale.SetLanguage(Locale.ResolveLanguage(iniFile:get("Main", "language", "")))
    NodeGraphFunctions.initLatent()
    GlobalSystem.init(iniFile, iniFilePath)
    Locale.SetLanguage(Locale.ResolveLanguage(GlobalSystem.getLanguage()))
    Data.InitializeRuntime()
    SourceSystem.InstallRuntimeProviders()
    LiveDebug.Install()
    SceneManager.setScene(SceneInit.new())
    SourceSystem.Init()
    GlobalSystem.run()
    LiveDebug.Uninstall()
    Logging.info("Game exited successfully.")
end

entry()
