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

public sealed class MainViewModel : ViewModelBase, IDisposable
{
    private string selectedLanguage = LocaleService.CurrentLanguage;
    private bool disposed;
    private EditorDocument? activeDocument;
    private bool canEdit = true;
    private readonly IRelayCommand[] editingCommands;

    public MainViewModel(
        ProjectDataStore gameData,
        ProjectConfigService projectConfig,
        GameVariableService gameVariables,
        BlueprintClassResolver blueprintClasses,
        GameConfigService gameConfig,
        ProjectSaveService projectSave,
        ReferenceIndexService referenceIndex,
        TileSelectViewModel tileSelect,
        ActorQueueViewModel actorQueue,
        FileExplorerViewModel fileExplorerPanel,
        EditorActionRouter actions,
        MapWorkspaceViewModel mapWorkspace)
    {
        GameData = gameData;
        ProjectConfig = projectConfig;
        GameVariables = gameVariables;
        BlueprintClasses = blueprintClasses;
        GameConfig = gameConfig;
        ProjectSave = projectSave;
        ReferenceIndex = referenceIndex;
        TileSelect = tileSelect;
        ActorQueue = actorQueue;
        FileExplorerPanel = fileExplorerPanel;
        Actions = actions;
        MapWorkspace = mapWorkspace;
        MapWorkspace.PropertyChanged += onMapWorkspacePropertyChanged;
        MapWorkspace.SelectedMapChanged += onUndoRedoStateChanged;
        SaveCommand = new RelayCommand(() => SaveRequested?.Invoke(this, EventArgs.Empty), () => CanEdit && IsModified);
        NewProjectCommand = new RelayCommand(() => NewProjectRequested?.Invoke(this, EventArgs.Empty));
        OpenProjectCommand = new RelayCommand(() => OpenProjectRequested?.Invoke(this, EventArgs.Empty));
        ExitCommand = new RelayCommand(() => ExitRequested?.Invoke(this, EventArgs.Empty));
        TileModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 0), () => MapWorkspace.CanUseMapTools);
        LightModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 1), () => CanEdit);
        ActorModeCommand = new RelayCommand(() => PreviewModeRequested?.Invoke(this, 2), () => MapWorkspace.CanUseMapTools);
        HelpCommand = new RelayCommand(Actions.OpenHelp);
        NewBlueprintCommand = new RelayCommand(() => Actions.NewBlueprint(), () => CanEdit);
        NewAnimationCommand = new RelayCommand(Actions.NewAnimation, () => CanEdit);
        NewParticleCommand = new RelayCommand(() => Actions.NewParticle(), () => CanEdit);
        NewCurveCommand = new RelayCommand(Actions.NewCurve, () => CanEdit);
        NewTextConfigCommand = new RelayCommand(() => Actions.NewTextConfig(), () => CanEdit);
        NewUiAssetCommand = new RelayCommand(() => Actions.NewUiAsset(), () => CanEdit);
        GameConfigCommand = new RelayCommand(Actions.OpenGameConfig, () => CanEdit);
        SystemConfigCommand = new RelayCommand(Actions.OpenSystemConfig, () => CanEdit);
        AnimationOverviewCommand = new RelayCommand(Actions.OpenAnimationOverview, () => CanEdit);
        ParticleOverviewCommand = new RelayCommand(Actions.OpenParticleOverview, () => CanEdit);
        TilesetsDataCommand = new RelayCommand(() => Actions.OpenTilesets(), () => CanEdit);
        CommonFunctionsCommand = new RelayCommand(() => Actions.OpenCommonFunctions(), () => CanEdit);
        GameVariablesCommand = new RelayCommand(Actions.OpenGameVariables, () => CanEdit);
        GeneralDataCommand = new RelayCommand(() => Actions.OpenGeneralData(), () => CanEdit);
        UndoCommand = new RelayCommand(executeUndo, () => CanEdit && ActiveDocument?.CanAttemptUndo == true);
        RedoCommand = new RelayCommand(executeRedo, () => CanEdit && ActiveDocument?.CanRedo == true);
        editingCommands =
        [
            SaveCommand, UndoCommand, RedoCommand, TileModeCommand, LightModeCommand, ActorModeCommand,
            NewBlueprintCommand, NewAnimationCommand, NewParticleCommand, NewCurveCommand, NewTextConfigCommand, NewUiAssetCommand,
            GameConfigCommand, SystemConfigCommand, AnimationOverviewCommand, ParticleOverviewCommand, TilesetsDataCommand,
            CommonFunctionsCommand, GameVariablesCommand, GeneralDataCommand,
        ];
        ChangeLanguageCommand = new RelayCommand<string>(changeLanguage);
        FileExplorerPanel.FileClicked += onExplorerFileClicked;
        FileExplorerPanel.FileOpened += onExplorerFileOpened;
        FileExplorerPanel.FilesChanged += onExplorerFilesChanged;
        GameData.ModifiedChanged += onModifiedChanged;
        ProjectSave.PendingInputsChanged += onModifiedChanged;
        GameConfig.Changed += onModifiedChanged;
        GameVariables.Changed += onModifiedChanged;
        GameVariables.Saved += onGameVariablesSaved;
        GameData.Documents.Changed += onDocumentsChanged;
        GameData.DataRestored += onDataRestored;
    }

    public event EventHandler? SaveRequested;
    public event EventHandler<SaveResult>? SaveCompleted;
    public event EventHandler<HistoryCompletedEventArgs>? HistoryCompleted;
    public event EventHandler? NewProjectRequested;
    public event EventHandler? OpenProjectRequested;
    public event EventHandler? ExitRequested;
    public event EventHandler<int>? PreviewModeRequested;
    public event EventHandler<string>? FileOpenFailed;

    public MapWorkspaceViewModel MapWorkspace { get; }
    public ProjectDataStore GameData { get; }
    public ProjectConfigService ProjectConfig { get; }
    public GameVariableService GameVariables { get; }
    public BlueprintClassResolver BlueprintClasses { get; }
    public GameConfigService GameConfig { get; }
    public ProjectSaveService ProjectSave { get; }
    public ReferenceIndexService ReferenceIndex { get; }
    public ActorQueueViewModel ActorQueue { get; }
    public FileExplorerViewModel FileExplorerPanel { get; }
    public EditorActionRouter Actions { get; }
    public TileSelectViewModel TileSelect { get; }
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
    public IRelayCommand NewParticleCommand { get; }
    public IRelayCommand NewCurveCommand { get; }
    public IRelayCommand NewTextConfigCommand { get; }
    public IRelayCommand NewUiAssetCommand { get; }
    public IRelayCommand GameConfigCommand { get; }
    public IRelayCommand SystemConfigCommand { get; }
    public IRelayCommand AnimationOverviewCommand { get; }
    public IRelayCommand ParticleOverviewCommand { get; }
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
            MapWorkspace.RefreshConfiguration();
        }
    }
    public bool CanEdit
    {
        get => canEdit;
        set
        {
            if (!SetProperty(ref canEdit, value))
                return;
            OnPropertyChanged(nameof(CanConfigureIndividualWindow));
            MapWorkspace.CanEdit = value;
            foreach (IRelayCommand command in editingCommands)
                command.NotifyCanExecuteChanged();
        }
    }
    public bool CanConfigureIndividualWindow => CanEdit && ProjectConfig.CanConfigureIndividualWindow;
    public event EventHandler? LanguageChangeRequested;
    public bool IsModified => GameData.IsModified || GameData.Documents.IsModified || ProjectSave.HasPendingInputErrors;
    public EditorDocument? ActiveDocument => activeDocument is { Exists: true } ? activeDocument : null;
    public void SetActiveDocument(EditorDocument? document)
    {
        if (ReferenceEquals(activeDocument, document))
            return;
        activeDocument = document;
        GameData.BreakHistoryGesture();
        onUndoRedoStateChanged(this, EventArgs.Empty);
    }
    public string WindowTitle => GameData.Configs.getGameTitle() + (IsModified ? " *" : string.Empty);

    public SaveResult SaveChanges(bool notify = true)
    {
        SaveResult result = ProjectSave.TrySave().Result;
        if (notify)
            SaveCompleted?.Invoke(this, result);
        return result;
    }

    public HistoryResult UndoChanges()
    {
        if (!CanEdit)
            return new HistoryResult(false);
        EditorDocument? document = ActiveDocument;
        HistoryResult result = document is null ? new HistoryResult(false) : GameData.Undo(document.Section, document.Key);
        HistoryCompleted?.Invoke(this, new HistoryCompletedEventArgs("Undo", result));
        return result;
    }

    public HistoryResult RedoChanges()
    {
        if (!CanEdit)
            return new HistoryResult(false);
        EditorDocument? document = ActiveDocument;
        HistoryResult result = document is null ? new HistoryResult(false) : GameData.Redo(document.Section, document.Key);
        HistoryCompleted?.Invoke(this, new HistoryCompletedEventArgs("Redo", result));
        return result;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        ProjectSave.PendingInputsChanged -= onModifiedChanged;
        GameData.ModifiedChanged -= onModifiedChanged;
        GameData.Documents.Changed -= onDocumentsChanged;
        GameData.DataRestored -= onDataRestored;
        GameConfig.Changed -= onModifiedChanged;
        GameVariables.Changed -= onModifiedChanged;
        GameVariables.Saved -= onGameVariablesSaved;
        MapWorkspace.PropertyChanged -= onMapWorkspacePropertyChanged;
        MapWorkspace.SelectedMapChanged -= onUndoRedoStateChanged;
        FileExplorerPanel.FileClicked -= onExplorerFileClicked;
        FileExplorerPanel.FileOpened -= onExplorerFileOpened;
        FileExplorerPanel.FilesChanged -= onExplorerFilesChanged;
    }

    private void onMapWorkspacePropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(MapWorkspaceViewModel.CanUseMapTools))
        {
            TileModeCommand.NotifyCanExecuteChanged();
            ActorModeCommand.NotifyCanExecuteChanged();
        }
    }

    private void executeUndo() => UndoChanges();

    private void executeRedo() => RedoChanges();

    private void onModifiedChanged(object? sender, EventArgs args)
    {
        OnPropertyChanged(nameof(IsModified));
        OnPropertyChanged(nameof(WindowTitle));
        SaveCommand.NotifyCanExecuteChanged();
    }

    private void onGameVariablesSaved(object? sender, EventArgs args)
    {
        FileExplorerPanel.RequestRefresh();
        onModifiedChanged(sender, args);
    }

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        onModifiedChanged(sender, args);
        onUndoRedoStateChanged(sender, args);
    }

    private void onUndoRedoStateChanged(object? sender, EventArgs args)
    {
        UndoCommand.NotifyCanExecuteChanged();
        RedoCommand.NotifyCanExecuteChanged();
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        ActorQueue.PurgeStale();
        MapWorkspace.refreshMaps();
        TileSelect.RefreshData();
    }

    private void onExplorerFileClicked(object? sender, FileExplorerFileEventArgs args)
    {
        string path = args.Path;
        DataFileInfo? info = args.Info;
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
        MapWorkspace.refreshMaps();
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
            if (!directory && !System.IO.File.Exists(newPath) && GameData.GetDocumentByPath(newPath)?.Exists != true)
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

    private void onExplorerFileOpened(object? sender, FileExplorerFileEventArgs args)
    {
        if (!CanEdit)
            return;
        string path = args.Path;
        DataFileInfo? info = args.Info;
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
                MapWorkspace.SelectedMap = MapWorkspace.findMapItem(info.Key);
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
            case EditorDataOpenTarget.Particle:
                Actions.OpenParticle(info.Key);
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
        MapWorkspace.refreshMaps();
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
    public string NewParticle => LocaleService.Get("NEW_PARTICLE");
    public string NewCurve => LocaleService.Get("NEW_CURVE");
    public string NewTextConfig => LocaleService.Get("NEW_TEXT_CONFIG");
    public string NewUiAsset => LocaleService.Get("NEW_UI_ASSET");
    public string Database => LocaleService.Get("DATABASE");
    public string SystemConfig => LocaleService.Get("SYSTEM_CONFIG");
    public string AnimationOverview => LocaleService.Get("ANIMATION_OVERVIEW");
    public string ParticleOverview => LocaleService.Get("PARTICLE_OVERVIEW");
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
