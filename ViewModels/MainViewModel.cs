using System.Collections.Generic;
using CommunityToolkit.Mvvm.Input;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    private MapListItemViewModel? selectedMap;
    private LayerTabViewModel? selectedLayerTab;
    private JsonObject? copiedLayer;
    private string? copiedLayerName;
    private string selectedLanguage = LocaleService.CurrentLanguage;
    private bool disposed;

    public MainViewModel(string projectPath)
    {
        GameData = new GameDataService(projectPath);
        TileSelect = new TileSelectViewModel(GameData);
        TileSelect.TilesetSelected += onTilesetSelected;
        ProjectConfig = new ProjectConfigService(projectPath);
        Metadata = new LuaMetadataService(projectPath);
        GameVariables = new GameVariableService(projectPath, Metadata);
        BlueprintClasses = new BlueprintClassResolver(GameData, Metadata);
        GameConfig = new GameConfigService(projectPath);
        BlueprintValidation = new BlueprintValidationService(GameData, Metadata, BlueprintClasses);
        ProjectSave = new ProjectSaveService(
            GameData,
            GameConfig,
            GameVariables,
            BlueprintValidation);
        ReferenceIndex = new ReferenceIndexService(GameData, Metadata, BlueprintClasses);
        UiControlRegistry = ProjectSave.UiControlRegistry;
        UiAssetValidation = ProjectSave.UiAssetValidation;
        BlueprintCreation = new BlueprintCreationService(GameData, Metadata, BlueprintClasses);
        PreviewService = new BlueprintPreviewService(projectPath, GameData, BlueprintClasses);
        IconService = new FileIconService();
        ActorQueue = new ActorQueueViewModel(
            GameData,
            ProjectConfig,
            BlueprintClasses,
            PreviewService,
            IconService);
        FileExplorerPanel = new FileExplorerViewModel(
            projectPath,
            ProjectConfig,
            GameData,
            PreviewService,
            IconService,
            ReferenceIndex);
        Actions = new EditorActionRouter(projectPath);
        SaveCommand = new RelayCommand(() => SaveRequested?.Invoke(this, EventArgs.Empty), () => IsModified);
        NewProjectCommand = new RelayCommand(() => NewProjectRequested?.Invoke(this, EventArgs.Empty));
        OpenProjectCommand = new RelayCommand(() => OpenProjectRequested?.Invoke(this, EventArgs.Empty));
        ExitCommand = new RelayCommand(() => ExitRequested?.Invoke(this, EventArgs.Empty));
        TileModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 0));
        LightModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 1));
        ActorModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 2));
        HelpCommand = new RelayCommand(Actions.OpenHelp);
        NewBlueprintCommand = new RelayCommand(() => Actions.NewBlueprint());
        NewAnimationCommand = new RelayCommand(Actions.NewAnimation);
        NewCurveCommand = new RelayCommand(Actions.NewCurve);
        NewTextConfigCommand = new RelayCommand(() => Actions.NewTextConfig());
        NewUiAssetCommand = new RelayCommand(() => Actions.NewUiAsset());
        GameConfigCommand = new RelayCommand(Actions.OpenGameConfig);
        SystemConfigCommand = new RelayCommand(Actions.OpenSystemConfig);
        AnimationOverviewCommand = new RelayCommand(Actions.OpenAnimationOverview);
        TilesetsDataCommand = new RelayCommand(() => Actions.OpenTilesets());
        CommonFunctionsCommand = new RelayCommand(() => Actions.OpenCommonFunctions());
        GameVariablesCommand = new RelayCommand(Actions.OpenGameVariables);
        GeneralDataCommand = new RelayCommand(() => Actions.OpenGeneralData());
        UndoCommand = new RelayCommand(executeUndo, () => GameData.CanUndo);
        RedoCommand = new RelayCommand(executeRedo, () => GameData.CanRedo);
        ChangeLanguageCommand = new RelayCommand<string>(changeLanguage);
        FileExplorerPanel.FileClicked += onExplorerFileClicked;
        FileExplorerPanel.FileOpened += onExplorerFileOpened;
        FileExplorerPanel.FilesChanged += onExplorerFilesChanged;
        GameData.ModifiedChanged += onModifiedChanged;
        GameConfig.Changed += onModifiedChanged;
        GameVariables.Changed += onModifiedChanged;
        GameVariables.Saved += onGameVariablesSaved;
        GameData.UndoRedoStateChanged += onUndoRedoStateChanged;
        GameData.DataRestored += onDataRestored;
        rebuildMapTree();
        SelectedMap = Maps.FirstOrDefault();
    }

    public event EventHandler? SelectedMapChanged;
    public event EventHandler? SaveRequested;
    public event EventHandler<SaveResult>? SaveCompleted;
    public event EventHandler<HistoryCompletedEventArgs>? HistoryCompleted;
    public event EventHandler? NewProjectRequested;
    public event EventHandler? OpenProjectRequested;
    public event EventHandler? ExitRequested;
    public event EventHandler<int>? PreviewModeRequested;
    public event EventHandler<string>? FileOpenFailed;
    public event EventHandler? ActorOutlinerChanged;
    public event EventHandler? LayerDisplayStateChanged;

    public GameDataService GameData { get; }
    public ProjectConfigService ProjectConfig { get; }
    public LuaMetadataService Metadata { get; }
    public GameVariableService GameVariables { get; }
    public BlueprintClassResolver BlueprintClasses { get; }
    public GameConfigService GameConfig { get; }
    public BlueprintValidationService BlueprintValidation { get; }
    public ProjectSaveService ProjectSave { get; }
    public ReferenceIndexService ReferenceIndex { get; }
    public UiControlRegistryService UiControlRegistry { get; }
    public UiAssetValidationService UiAssetValidation { get; }
    public BlueprintCreationService BlueprintCreation { get; }
    public BlueprintPreviewService PreviewService { get; }
    public FileIconService IconService { get; }
    public ActorQueueViewModel ActorQueue { get; }
    public FileExplorerViewModel FileExplorerPanel { get; }
    public EditorActionRouter Actions { get; }
    public TileSelectViewModel TileSelect { get; }
    public ObservableCollection<MapListItemViewModel> Maps { get; } = [];
    public ObservableCollection<LayerTabViewModel> LayerTabs { get; } = [];
    public ObservableCollection<ActorOutlinerItemViewModel> ActorOutlinerItems { get; } = [];
    public IRelayCommand SaveCommand { get; }
    public IRelayCommand NewProjectCommand { get; }
    public IRelayCommand OpenProjectCommand { get; }
    public IRelayCommand ExitCommand { get; }
    public IRelayCommand TileModeCommand { get; }
    public IRelayCommand LightModeCommand { get; }
    public IRelayCommand ActorModeCommand { get; }
    public IRelayCommand HelpCommand { get; }
    public IRelayCommand NewBlueprintCommand { get; }
    public IRelayCommand NewAnimationCommand { get; }
    public IRelayCommand NewCurveCommand { get; }
    public IRelayCommand NewTextConfigCommand { get; }
    public IRelayCommand NewUiAssetCommand { get; }
    public IRelayCommand GameConfigCommand { get; }
    public IRelayCommand SystemConfigCommand { get; }
    public IRelayCommand AnimationOverviewCommand { get; }
    public IRelayCommand TilesetsDataCommand { get; }
    public IRelayCommand CommonFunctionsCommand { get; }
    public IRelayCommand GameVariablesCommand { get; }
    public IRelayCommand GeneralDataCommand { get; }
    public IRelayCommand UndoCommand { get; }
    public IRelayCommand RedoCommand { get; }
    public IRelayCommand<string> ChangeLanguageCommand { get; }
    public string SelectedLanguage
    {
        get => selectedLanguage;
        private set
        {
            if (!SetProperty(ref selectedLanguage, value))
                return;
            OnPropertyChanged(nameof(IsEnglishLanguage));
            OnPropertyChanged(nameof(IsChineseLanguage));
        }
    }

    public bool IsEnglishLanguage => string.Equals(SelectedLanguage, "en_GB", StringComparison.Ordinal);
    public bool IsChineseLanguage => string.Equals(SelectedLanguage, "zh_CN", StringComparison.Ordinal);
    public bool IndividualWindow
    {
        get => ProjectConfig.IndividualWindow;
        set
        {
            if (ProjectConfig.IndividualWindow == value)
                return;
            ProjectConfig.IndividualWindow = value;
            OnPropertyChanged();
        }
    }
    public bool CanConfigureIndividualWindow => ProjectConfig.CanConfigureIndividualWindow;
    public event EventHandler? LanguageChangeRequested;
    public bool IsModified => GameData.IsModified || GameConfig.IsModified || GameVariables.IsModified;
    public string WindowTitle => GameData.getGameTitle() + (IsModified ? " *" : string.Empty);

    public MapListItemViewModel? SelectedMap
    {
        get => selectedMap;
        set
        {
            if (!SetProperty(ref selectedMap, value))
                return;
            refreshLayerTabs();
            IReadOnlyCollection<string> pinnedMaps = value is { IsWorldChild: true }
                ? new[] { value.Key }
                : Array.Empty<string>();
            GameData.TrimWorldChildCache(pinnedMaps);
            SelectedMapChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public LayerTabViewModel? SelectedLayerTab
    {
        get => selectedLayerTab;
        set
        {
            if (!SetProperty(ref selectedLayerTab, value))
                return;
            bool hasLayer = value is not null && !value.IsOverview && SelectedMap is not null;
            TileSelect.IsLayerSelected = hasLayer;
            if (hasLayer)
                TileSelect.setCurrentTilesetKey(GameData.getLayerTilesetKey(SelectedMap!.Key, value!.Name));
            LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public bool IsSelectedLayerEditable => SelectedLayerTab is
        {
            IsOverview: false,
            LayerVisible: true,
        };

    public bool canEditLayer(string mapKey, string layerName)
    {
        if (GameData.MapData.GetValueOrDefault(mapKey)?["layers"]?[layerName] is not JsonObject layer)
            return false;
        return layer["visible"]?.GetValue<bool?>() ?? true;
    }

    public bool moveLayer(LayerTabViewModel moving, LayerTabViewModel target)
    {
        if (SelectedMap is null || moving.IsOverview || target.IsOverview || moving == target)
            return false;
        if (!GameData.reorderLayers(SelectedMap.Key, moving.Name, target.Name))
            return false;
        int fromIndex = LayerTabs.IndexOf(moving);
        int targetIndex = LayerTabs.IndexOf(target);
        LayerTabs.Move(fromIndex, targetIndex);
        SelectedLayerTab = moving;
        refreshActorOutliner();
        return true;
    }

    public bool addLayer(string name, string? insertAfterLayer)
    {
        if (SelectedMap is null || !GameData.addEmptyLayer(SelectedMap.Key, name, insertAfterLayer))
            return false;
        refreshLayerTabs(name);
        return true;
    }

    public bool renameLayer(string oldName, string newName)
    {
        if (SelectedMap is null || !GameData.renameLayer(SelectedMap.Key, oldName, newName))
            return false;
        refreshLayerTabs(newName);
        return true;
    }

    public bool deleteLayer(string layerName)
    {
        if (SelectedMap is null || !GameData.removeLayer(SelectedMap.Key, layerName))
            return false;
        refreshLayerTabs();
        return true;
    }

    public bool copyLayer(string layerName)
    {
        if (SelectedMap is null || GameData.copyLayer(SelectedMap.Key, layerName) is not { } copy)
            return false;
        copiedLayer = copy;
        copiedLayerName = layerName;
        OnPropertyChanged(nameof(CanPasteLayer));
        return true;
    }

    public bool pasteLayer(string insertAfterLayer)
    {
        if (SelectedMap is null || copiedLayer is null || string.IsNullOrWhiteSpace(copiedLayerName))
            return false;
        string name = getUniqueLayerName($"{copiedLayerName}_copy");
        if (!GameData.pasteLayer(SelectedMap.Key, name, copiedLayer, insertAfterLayer))
            return false;
        refreshLayerTabs(name);
        return true;
    }

    public bool CanPasteLayer => copiedLayer is not null;

    public bool setLayerVisible(LayerTabViewModel layer, bool visible)
    {
        if (SelectedMap is null || layer.IsOverview || layer.LayerVisible == visible)
            return false;
        if (!GameData.SetLayerVisible(SelectedMap.Key, layer.Name, visible))
            return false;
        layer.LayerVisible = visible;
        updateLayerEditability();
        return true;
    }

    public string getLayerShaderPath(string layerName)
    {
        return SelectedMap is null ? string.Empty : GameData.getLayerShaderPath(SelectedMap.Key, layerName);
    }

    public bool setLayerShaderPath(string layerName, string shaderPath)
    {
        return SelectedMap is not null && GameData.setLayerShaderPath(SelectedMap.Key, layerName, shaderPath);
    }

    public bool layerNameExists(string name, string? except = null)
    {
        return LayerTabs.Any(item => !item.IsOverview && item.Name == name && item.Name != except);
    }

    public void refreshActorOutliner()
    {
        ActorOutlinerItems.Clear();
        JsonObject? actorGroups = SelectedMapData?["actors"] as JsonObject;
        foreach (LayerTabViewModel layer in LayerTabs)
        {
            if (layer.IsOverview)
                continue;
            List<ActorOutlinerItemViewModel> actors = [];
            if (actorGroups?[layer.Name] is JsonArray layerActors)
            {
                for (int index = 0; index < layerActors.Count; index += 1)
                {
                    if (layerActors[index] is not JsonObject actor)
                        continue;
                    string tag = getString(actor["tag"]);
                    string reference = getString(actor["bp"]);
                    string name = !string.IsNullOrWhiteSpace(tag)
                        ? tag
                        : !string.IsNullOrWhiteSpace(reference)
                            ? reference
                            : $"#{index + 1}";
                    actors.Add(new ActorOutlinerItemViewModel(
                        name,
                        reference,
                        layer.Name,
                        index,
                        []));
                }
            }
            ActorOutlinerItems.Add(new ActorOutlinerItemViewModel(
                layer.Name,
                layer.Name,
                layer.Name,
                null,
                actors));
        }
        ActorOutlinerChanged?.Invoke(this, EventArgs.Empty);
    }

    public SaveResult SaveChanges(bool notify = true)
    {
        SaveResult result = ProjectSave.TrySave().Result;
        if (notify)
            SaveCompleted?.Invoke(this, result);
        return result;
    }

    public IReadOnlyList<string> UndoChanges()
    {
        IReadOnlyList<string> differences = GameData.Undo();
        HistoryCompleted?.Invoke(this, new HistoryCompletedEventArgs("Undo", differences));
        return differences;
    }

    public IReadOnlyList<string> RedoChanges()
    {
        IReadOnlyList<string> differences = GameData.Redo();
        HistoryCompleted?.Invoke(this, new HistoryCompletedEventArgs("Redo", differences));
        return differences;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        GameConfig.Changed -= onModifiedChanged;
        GameVariables.Changed -= onModifiedChanged;
        GameVariables.Saved -= onGameVariablesSaved;
        TileSelect.TilesetSelected -= onTilesetSelected;
        TileSelect.Dispose();
        ActorQueue.Dispose();
        FileExplorerPanel.Dispose();
        PreviewService.Dispose();
        ReferenceIndex.Dispose();
        BlueprintClasses.Dispose();
        GameData.Dispose();
    }

    private void executeUndo() => UndoChanges();

    private void executeRedo() => RedoChanges();

    private void refreshLayerTabs(string? preferredLayerName = null)
    {
        string? previousName = preferredLayerName ?? (SelectedLayerTab is { IsOverview: false } ? SelectedLayerTab.Name : null);
        LayerTabs.Clear();
        if (SelectedMap is not { IsMap: true })
        {
            SelectedLayerTab = null;
            refreshActorOutliner();
            updateLayerEditability();
            return;
        }
        LayerTabs.Add(new LayerTabViewModel(LocaleService.Get("OVERVIEW"), true, true));
        if (SelectedMap is not null)
        {
            foreach (string name in GameData.getLayerNames(SelectedMap.Key))
            {
                bool visible = SelectedMapData?["layers"]?[name]?["visible"]?.GetValue<bool?>() ?? true;
                LayerTabs.Add(new LayerTabViewModel(
                    name,
                    false,
                    visible));
            }
        }
        SelectedLayerTab = previousName is null
            ? LayerTabs[0]
            : LayerTabs.FirstOrDefault(item => !item.IsOverview && item.Name == previousName) ?? LayerTabs[0];
        refreshActorOutliner();
        updateLayerEditability();
    }

    private void updateLayerEditability()
    {
        LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private static string getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text)
            ? text ?? string.Empty
            : string.Empty;
    }

    private string getUniqueLayerName(string baseName)
    {
        string candidate = baseName;
        int suffix = 2;
        while (layerNameExists(candidate))
            candidate = $"{baseName}_{suffix++}";
        return candidate;
    }

    private void onModifiedChanged(object? sender, EventArgs args)
    {
        OnPropertyChanged(nameof(IsModified));
        OnPropertyChanged(nameof(WindowTitle));
        SaveCommand.NotifyCanExecuteChanged();
    }

    private void onTilesetSelected(object? sender, string tilesetKey)
    {
        if (SelectedMap is null || SelectedLayerTab is not { IsOverview: false } layer)
            return;
        if (GameData.setLayerTilesetKey(SelectedMap.Key, layer.Name, tilesetKey))
            LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onGameVariablesSaved(object? sender, EventArgs args)
    {
        FileExplorerPanel.Refresh();
        onModifiedChanged(sender, args);
    }

    private void onUndoRedoStateChanged(object? sender, EventArgs args)
    {
        UndoCommand.NotifyCanExecuteChanged();
        RedoCommand.NotifyCanExecuteChanged();
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        ActorQueue.PurgeStale();
        refreshMaps();
        TileSelect.RefreshData();
        SelectedMapChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onExplorerFileClicked(object? sender, string path)
    {
        DataFileInfo? info = GameData.TryLoadDataFile(path);
        if (info?.Type != "blueprint" || info.Key is null)
            return;
        string reference = "Data.Blueprints." + info.Key.Replace('/', '.');
        if (BlueprintClasses.IsDerivedFrom(reference, "Engine.Actor"))
            ActorQueue.AddOrPromote(reference);
    }

    private void onExplorerFilesChanged(
        object? sender,
        FileExplorerFilesChangedEventArgs args)
    {
        remapExplorerMoves(args.Moved);
        ReferenceIndex.MarkDirty();
        ActorQueue.PurgeStale();
        refreshMaps();
        TileSelect.RefreshData();
    }

    private void remapExplorerMoves(
        IReadOnlyList<(string OldPath, string NewPath)> moved)
    {
        if (moved.Count == 0)
            return;
        Dictionary<string, string> replacements = new Dictionary<string, string>(StringComparer.Ordinal);
        IReadOnlyList<string> blueprintReferences = ActorQueue.BlueprintReferences;
        foreach ((string oldPath, string newPath) in moved)
        {
            bool directory = Directory.Exists(newPath);
            if (!directory && !System.IO.File.Exists(newPath))
                continue;
            if (tryGetBlueprintReferencePath(oldPath, directory, out string oldReference)
                && tryGetBlueprintReferencePath(newPath, directory, out string newReference))
            {
                foreach (string reference in blueprintReferences)
                {
                    if (!directory && string.Equals(reference, oldReference, StringComparison.Ordinal))
                        replacements[reference] = newReference;
                    else if (directory && reference.StartsWith(oldReference + ".", StringComparison.Ordinal))
                        replacements[reference] = newReference + reference[oldReference.Length..];
                }
            }
        }
        if (replacements.Count != 0)
            ActorQueue.RemapReferences(replacements);
    }

    private bool tryGetBlueprintReferencePath(
        string path,
        bool directory,
        out string reference)
    {
        string root = Path.GetFullPath(Path.Combine(GameData.ProjectPath, "Data", "Blueprints"));
        string fullPath = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(root, fullPath);
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
        {
            reference = string.Empty;
            return false;
        }
        if (!directory && !string.Equals(
                Path.GetExtension(relative),
                DataConfig.DataFileExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            reference = string.Empty;
            return false;
        }
        string key = directory ? relative : Path.ChangeExtension(relative, null)!;
        reference = "Data.Blueprints." + key
            .Replace(Path.DirectorySeparatorChar, '.')
            .Replace(Path.AltDirectorySeparatorChar, '.')
            .Trim('.');
        return reference.Length > "Data.Blueprints.".Length;
    }

    private void onExplorerFileOpened(object? sender, string path)
    {
        DataFileInfo? info = GameData.TryLoadDataFile(path);
        if (info?.Type == "invalidTextConfig")
        {
            FileOpenFailed?.Invoke(
                this,
                LocaleService.Get("INVALID_TEXT_CONFIG_TYPE")
                    .Replace("{path}", path));
            return;
        }
        if (info is null
            || string.IsNullOrWhiteSpace(info.Key)
            || !EditorDataOpenCatalog.TryResolve(info.Type, out EditorDataOpenTarget target))
        {
            openWithSystem(path);
            return;
        }
        switch (target)
        {
            case EditorDataOpenTarget.SystemConfig:
                Actions.OpenSystemConfig();
                break;
            case EditorDataOpenTarget.Tilesets:
                Actions.OpenTilesets(info.Key);
                break;
            case EditorDataOpenTarget.AutoTiles:
                Actions.OpenAutoTiles(info.Key);
                break;
            case EditorDataOpenTarget.Map:
                SelectedMap = findMapItem(info.Key);
                break;
            case EditorDataOpenTarget.CommonFunctions:
                Actions.OpenCommonFunctions(info.Key);
                break;
            case EditorDataOpenTarget.Blueprint:
                Actions.OpenBlueprint(info.Key);
                break;
            case EditorDataOpenTarget.Animation:
                Actions.OpenAnimation(info.Key);
                break;
            case EditorDataOpenTarget.Curve:
                Actions.OpenCurve(info.Key);
                break;
            case EditorDataOpenTarget.TextConfig:
                Actions.OpenTextConfig(info.Key);
                break;
            case EditorDataOpenTarget.UiAsset:
                Actions.OpenUiAsset(UiAssetSchema.ToLogicalAssetKey(info.Key));
                break;
            case EditorDataOpenTarget.GeneralData:
                Actions.OpenGeneralData(info.Key);
                break;
        }
    }

    private void openWithSystem(string path)
    {
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Win32Exception exception)
        {
            FileOpenFailed?.Invoke(this, exception.Message);
        }
        catch (InvalidOperationException exception)
        {
            FileOpenFailed?.Invoke(this, exception.Message);
        }
    }

    private void changeLanguage(string? language)
    {
        if (string.IsNullOrWhiteSpace(language)
            || string.Equals(SelectedLanguage, language, StringComparison.Ordinal)
            || !LocaleService.SetLanguage(language))
            return;
        SelectedLanguage = language;
        refreshMaps();
        LanguageChangeRequested?.Invoke(this, EventArgs.Empty);
    }

    public string File => LocaleService.Get("FILE");
    public string NewProject => LocaleService.Get("NEW_PROJECT");
    public string OpenProject => LocaleService.Get("OPEN_PROJECT");
    public string Save => LocaleService.Get("SAVE");
    public string PackProject => LocaleService.Get("PACK_PROJECT");
    public string Exit => LocaleService.Get("EXIT");
    public string Plugins => LocaleService.Get("PLUGINS");
    public string ImportPlugin => LocaleService.Get("IMPORT_PLUGIN");
    public string ManagePlugins => LocaleService.Get("MANAGE_PLUGINS");
    public string Edit => LocaleService.Get("EDIT");
    public string DevelopmentToolsSettings => LocaleService.Get("DEVELOPMENT_TOOLS_SETTINGS");
    public string IndividualWindowLabel => LocaleService.Get("IndividualWindow");
    public string Undo => LocaleService.Get("UNDO");
    public string Redo => LocaleService.Get("REDO");
    public string Game => LocaleService.Get("GAME");
    public string GameConfigLabel => LocaleService.Get("GAME_CONFIG");
    public string PerformanceMonitor => LocaleService.Get("PERFORMANCE_MONITOR");
    public string ReloadModule => LocaleService.Get("RELOAD_MODULE");
    public string NewBlueprint => LocaleService.Get("NEW_BLUEPRINT");
    public string NewAnimation => LocaleService.Get("NEW_ANIMATION");
    public string NewCurve => LocaleService.Get("NEW_CURVE");
    public string NewTextConfig => LocaleService.Get("NEW_TEXT_CONFIG");
    public string NewUiAsset => LocaleService.Get("NEW_UI_ASSET");
    public string Database => LocaleService.Get("DATABASE");
    public string SystemConfig => LocaleService.Get("SYSTEM_CONFIG");
    public string AnimationOverview => LocaleService.Get("ANIMATION_OVERVIEW");
    public string TilesetsData => LocaleService.Get("TILESETS_DATA");
    public string CommonFunctions => LocaleService.Get("COMMON_FUNCTIONS");
    public string GameVariablesLabel => LocaleService.Get("GAME_VARIABLES");
    public string GeneralData => LocaleService.Get("GENERAL_DATA");
    public string Help => LocaleService.Get("HELP");
    public string HelpExplanation => LocaleService.Get("HELP_EXPLANATION");
    public string HelpLanguage => LocaleService.Get("HELP_LANGUAGE");
    public string About => LocaleService.Get("ABOUT_MENU");
    public string MapList => LocaleService.Get("MAP_LIST");
    public string WorldOutliner => LocaleService.Get("WORLD_OUTLINER");
    public string ActorLibrary => LocaleService.Get("ACTOR_LIBRARY");
    public string FileExplorer => LocaleService.Get("FILE_EXPLORER");
    public string Console => LocaleService.Get("CONSOLE");
}

public sealed class HistoryCompletedEventArgs(string action, IReadOnlyList<string> differences) : EventArgs
{
    public string Action { get; } = action;
    public IReadOnlyList<string> Differences { get; } = differences;
}
public sealed class LayerTabViewModel : ViewModelBase
{
    private bool layerVisible;

    public LayerTabViewModel(
        string name,
        bool isOverview,
        bool layerVisible)
    {
        Name = name;
        IsOverview = isOverview;
        this.layerVisible = layerVisible;
    }

    public string Name { get; }
    public bool IsOverview { get; }
    public bool ShowLayerActions => !IsOverview;
    public bool LayerVisible
    {
        get => layerVisible;
        set
        {
            if (!SetProperty(ref layerVisible, value))
                return;
            OnPropertyChanged(nameof(VisibilityTooltip));
        }
    }
    public string VisibilityTooltip => LocaleService.Get(LayerVisible ? "HIDE_LAYER" : "SHOW_LAYER");
}

public sealed class ActorOutlinerItemViewModel(
    string name,
    string description,
    string layerName,
    int? actorIndex,
    IReadOnlyList<ActorOutlinerItemViewModel> children)
{
    public string Name { get; } = name;
    public string Description { get; } = description;
    public string LayerName { get; } = layerName;
    public int? ActorIndex { get; } = actorIndex;
    public IReadOnlyList<ActorOutlinerItemViewModel> Children { get; } = children;
}
