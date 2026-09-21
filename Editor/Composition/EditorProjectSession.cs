using Ludork.ViewModels;
using Ludork.Services;
using System;
using System.IO;

namespace Ludork.Composition;

public sealed class EditorProjectSession : IDisposable
{
    private bool disposed;

    public EditorProjectSession(string projectPath)
    {
        ProjectPath = Path.GetFullPath(projectPath);
        GameData = new ProjectDataStore(ProjectPath);
        ProjectConfig = new ProjectConfigService(ProjectPath);
        Metadata = new LuaMetadataService(ProjectPath);
        GameVariables = new GameVariableService(ProjectPath, Metadata, GameData.Documents);
        BlueprintClasses = new BlueprintClassResolver(GameData, Metadata);
        GameConfig = new GameConfigService(ProjectPath);
        ProjectRunner = new ProjectRunnerService(ProjectPath);
        BlueprintValidation = new BlueprintValidationService(GameData, Metadata, BlueprintClasses);
        ProjectSave = new ProjectSaveService(GameData, GameVariables, BlueprintValidation, ProjectConfig);
        ReferenceIndex = new ReferenceIndexService(GameData, Metadata, BlueprintClasses);
        BlueprintCreation = new BlueprintCreationService(GameData, Metadata, BlueprintClasses);
        PreviewService = new BlueprintPreviewService(ProjectPath, GameData, BlueprintClasses, UiControlRegistry.Runtime);
        TileSelect = new TileSelectViewModel(GameData);
        ActorQueue = new ActorQueueViewModel(GameData, ProjectConfig, BlueprintClasses, PreviewService);
        FileExplorer = new FileExplorerViewModel(ProjectPath, ProjectConfig, GameData, PreviewService, ReferenceIndex);
        Actions = new EditorActionRouter();
        MapWorkspace = new MapWorkspaceViewModel(GameData, ProjectConfig, TileSelect, ReferenceIndex);
        MainViewModel = new MainViewModel(GameData, ProjectConfig, GameVariables, BlueprintClasses,
            GameConfig, ProjectSave, ReferenceIndex, TileSelect, ActorQueue, FileExplorer, Actions, MapWorkspace);
    }

    public string ProjectPath { get; }
    public ProjectDataStore GameData { get; }
    public ProjectConfigService ProjectConfig { get; }
    public LuaMetadataService Metadata { get; }
    public GameVariableService GameVariables { get; }
    public BlueprintClassResolver BlueprintClasses { get; }
    public GameConfigService GameConfig { get; }
    public ProjectRunnerService ProjectRunner { get; }
    public BlueprintValidationService BlueprintValidation { get; }
    public ProjectSaveService ProjectSave { get; }
    public ReferenceIndexService ReferenceIndex { get; }
    public UiControlRegistryService UiControlRegistry => ProjectSave.UiControlRegistry;
    public UiAssetValidationService UiAssetValidation => ProjectSave.UiAssetValidation;
    public BlueprintCreationService BlueprintCreation { get; }
    public BlueprintPreviewService PreviewService { get; }
    public EditorActionRouter Actions { get; }
    public TileSelectViewModel TileSelect { get; }
    public ActorQueueViewModel ActorQueue { get; }
    public FileExplorerViewModel FileExplorer { get; }
    public MainViewModel MainViewModel { get; }
    public MapWorkspaceViewModel MapWorkspace { get; }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        MainViewModel.Dispose();
        MapWorkspace.Dispose();
        FileExplorer.Dispose();
        ActorQueue.Dispose();
        TileSelect.Dispose();
        ProjectRunner.Dispose();
        PreviewService.Dispose();
        UiControlRegistry.Dispose();
        ReferenceIndex.Dispose();
        BlueprintClasses.Dispose();
        GameData.Dispose();
    }
}
