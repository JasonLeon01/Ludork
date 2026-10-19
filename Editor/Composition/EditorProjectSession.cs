using Ludork.ViewModels;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.Composition;

public sealed class EditorProjectSession : IDisposable
{
    private bool disposed;

    public EditorProjectSession(ProjectDataStore gameData)
    {
        ProjectPath = gameData.ProjectPath;
        GameData = gameData;
        ProjectConfig = new ProjectConfigService(ProjectPath);
        Metadata = GameData.Metadata;
        GameVariables = new GameVariableService(ProjectPath, Metadata, GameData.Documents);
        BlueprintClasses = GameData.BlueprintClasses;
        GameConfig = new GameConfigService(ProjectPath);
        ProjectRunner = new ProjectRunnerService(ProjectPath);
        BlueprintValidation = new BlueprintValidationService(GameData, Metadata, BlueprintClasses);
        ProjectSave = new ProjectSaveService(GameData, GameVariables, ProjectConfig);
        ReferenceIndex = GameData.ReferenceIndex;
        ReferenceIndex.EnableBackgroundUpdates();
        BlueprintCreation = new BlueprintCreationService(GameData, Metadata, BlueprintClasses);
        PreviewService = new BlueprintPreviewService(ProjectPath, GameData, BlueprintClasses, UiControlRegistry.Runtime, ProjectConfig);
        TileSelect = new TileSelectViewModel(GameData);
        ActorQueue = new ActorQueueViewModel(GameData, ProjectConfig, BlueprintClasses, PreviewService);
        FileExplorer = new FileExplorerViewModel(ProjectPath, ProjectConfig, GameData, PreviewService, ReferenceIndex);
        Actions = new EditorActionRouter();
        MapWorkspace = new MapWorkspaceViewModel(GameData, ProjectConfig, TileSelect, ReferenceIndex);
        MainViewModel = new MainViewModel(GameData, ProjectConfig, GameVariables, BlueprintClasses, PreviewService,
            GameConfig, ProjectSave, ReferenceIndex, TileSelect, ActorQueue, FileExplorer, Actions, MapWorkspace);
        ExternalChanges = new ProjectExternalChangeService(GameData, ProjectSave);
        ProjectSave.ExternalChanges = ExternalChanges;
        ExternalChanges.Synchronized += onExternalFilesSynchronized;
        FileExplorer.FilesChanged += onFilesChanged;
    }

    public string ProjectPath { get; }
    public ProjectDataStore GameData { get; }
    public EditorEditOperationService EditOperations => GameData.EditOperations;
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
    public ProjectExternalChangeService ExternalChanges { get; }

    private void onExternalFilesSynchronized(object? sender, ExternalFilesSynchronizedEventArgs args)
        => MainViewModel.RefreshExternalFiles(args.Paths);

    private void onFilesChanged(object? sender, FileExplorerFilesChangedEventArgs args)
        => ExternalChanges.AcceptFileOperations(args.Added.Concat(args.Deleted)
            .Concat(args.Moved.SelectMany(move => new[] { move.OldPath, move.NewPath }))
            .Where(path => !GameData.Documents.ModifiedDocuments.Any(document =>
                ProjectDiskSnapshot.IsWithin(document.Path, path)
                || ProjectDiskSnapshot.IsWithin(document.SavedPath, path))));
    public MapWorkspaceViewModel MapWorkspace { get; }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        FileExplorer.FilesChanged -= onFilesChanged;
        ExternalChanges.Synchronized -= onExternalFilesSynchronized;
        ExternalChanges.Dispose();
        MainViewModel.Dispose();
        MapWorkspace.Dispose();
        FileExplorer.Dispose();
        ActorQueue.Dispose();
        TileSelect.Dispose();
        ProjectRunner.Dispose();
        PreviewService.Dispose();
        UiControlRegistry.Dispose();
        GameData.Dispose();
    }
}
