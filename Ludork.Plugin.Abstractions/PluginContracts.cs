using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugin.Abstractions;

public interface IEditorPlugin
{
    void Register(IPluginRegistrar registrar);
}

public interface IPluginRegistrar
{
    string PluginDirectory { get; }
    string PluginDataDirectory =>
        throw new NotSupportedException(
            "This plugin registrar does not provide an external data directory.");
    string EditorLanguage { get; }
    void RegisterMenuCommand(PluginMenuCommand command);
    void RegisterMapContextMenuCommand(PluginMapContextMenuCommand command);
    void RegisterTextHintProvider(ITextHintProvider provider);
    void RegisterBeforeRunHook(IProjectOperationHook hook);
    void RegisterBeforePackHook(IProjectOperationHook hook);
}

public interface ITextHintProvider
{
    string? Resolve(TextHintContext context);
}

public interface IProjectOperationHook
{
    Task<PluginResult> ExecuteAsync(ProjectOperationContext context);
}

public interface IProjectPackaging
{
    bool UseLuac { get; }
    void CompileLuaDirectory(string relativePath);
    void ExcludeFile(string relativePath);
}

public interface IPluginOutput
{
    void WriteLine(string text);
}

public interface ITextHintRefresh
{
    void Invalidate();
}

public interface IPluginUserInterface
{
    Task ShowMessageAsync(
        string title,
        string message,
        PluginMessageKind kind,
        CancellationToken cancellationToken);
}

public interface IPluginSecretStore
{
    Task<bool> ContainsAsync(string name, CancellationToken cancellationToken);
    Task<string?> ReadAsync(string name, CancellationToken cancellationToken);
    Task WriteAsync(string name, string value, CancellationToken cancellationToken);
    Task DeleteAsync(string name, CancellationToken cancellationToken);
}

public interface IMapEditorHost
{
    string ProjectPath { get; }
    string? SuggestedMapKey { get; }
    IReadOnlyList<PluginMapSummary> ListMaps();
    PluginMapSnapshot ReadMap(string mapKey);
    PluginTilesetSnapshot ReadTileset(string tilesetKey);

    Task<PluginMapWriteResult> ReplaceLayerAndSaveAsync(
        PluginMapLayerWriteRequest request,
        CancellationToken cancellationToken);
}

public interface IBlueprintAssistantProvider
{
    Task<BlueprintAssistantSettings> LoadSettingsAsync(
        BlueprintAssistantProviderContext context,
        CancellationToken cancellationToken);

    Task<PluginResult> SaveSettingsAsync(
        BlueprintAssistantProviderContext context,
        BlueprintAssistantSettingsUpdate update,
        CancellationToken cancellationToken);

    Task<PluginResult> TestConnectionAsync(
        BlueprintAssistantProviderContext context,
        CancellationToken cancellationToken);

    IAsyncEnumerable<BlueprintAssistantEvent> RunTurnAsync(
        BlueprintAssistantTurnContext context);
}

public interface IBlueprintAssistantWorkspace
{
    Task<BlueprintAssistantToolResult> ListBlueprintsAsync(
        CancellationToken cancellationToken);

    Task<BlueprintAssistantToolResult> ReadBlueprintAsync(
        string blueprintKey,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantToolResult> GetApiCatalogAsync(
        string query,
        int maxResults,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantToolResult> SearchProjectAsync(
        string query,
        int maxResults,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantToolResult> ReadProjectFileAsync(
        string relativePath,
        int startLine,
        int lineCount,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantToolResult> ValidateCandidateAsync(
        string candidateJson,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantProposalResult> ProposePatchAsync(
        string patchJson,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantProposalResult> ProposeReplacementAsync(
        string replacementJson,
        CancellationToken cancellationToken);
}

public interface IBlueprintAssistantHost
{
    string ProjectPath { get; }
    string? SuggestedBlueprintKey { get; }
    IReadOnlyList<string> ListBlueprints();
    IBlueprintAssistantSession CreateSession(string blueprintKey);
}

public interface IBlueprintAssistantSession
{
    string BlueprintKey { get; }
    string BaseRevision { get; }
    IBlueprintAssistantWorkspace Workspace { get; }

    Task<BlueprintAssistantApplyResult> ApplyProposalAsync(
        string proposalId,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantCandidateResult> GetProposalCandidateAsync(
        string proposalId,
        CancellationToken cancellationToken);

    Task<BlueprintAssistantApplyResult> ApplyCandidateAsync(
        string baseRevision,
        string candidateJson,
        CancellationToken cancellationToken);

    Task<PluginResult> DiscardProposalAsync(
        string proposalId,
        CancellationToken cancellationToken);
}

public enum PluginMenuLocation
{
    File,
    Edit,
    Game,
    Database,
    Help,
    Plugins,
}

public enum ProjectOperationKind
{
    Run,
    Pack,
}

public enum PluginMessageKind
{
    Information,
    Warning,
    Error,
}

public sealed record PluginResult(bool Success, string Error)
{
    public static PluginResult Completed()
    {
        return new PluginResult(true, string.Empty);
    }

    public static PluginResult Failed(string error)
    {
        return new PluginResult(false, error);
    }
}

public sealed record TextHintContext(
    string ProjectPath,
    string EditorLanguage,
    string Text);

public sealed record ProjectOperationContext(
    string ProjectPath,
    string EditorLanguage,
    ProjectOperationKind Operation,
    IProjectPackaging? Packaging,
    IPluginOutput Output,
    ITextHintRefresh TextHints,
    CancellationToken CancellationToken);

public sealed record PluginMenuContext(
    string? ProjectPath,
    string EditorLanguage,
    string PluginDirectory,
    string PluginDataDirectory,
    IPluginUserInterface UserInterface,
    ITextHintRefresh TextHints,
    IPluginSecretStore SecretStore,
    IBlueprintAssistantHost? BlueprintAssistantHost,
    CancellationToken CancellationToken)
{
    public PluginMenuContext(
        string? projectPath,
        string editorLanguage,
        string pluginDirectory,
        IPluginUserInterface userInterface,
        ITextHintRefresh textHints,
        CancellationToken cancellationToken)
        : this(
            projectPath,
            editorLanguage,
            pluginDirectory,
            pluginDirectory,
            userInterface,
            textHints,
            UnavailablePluginSecretStore.Instance,
            null,
            cancellationToken)
    {
    }

    public void Deconstruct(
        out string? projectPath,
        out string editorLanguage,
        out string pluginDirectory,
        out IPluginUserInterface userInterface,
        out ITextHintRefresh textHints,
        out CancellationToken cancellationToken)
    {
        projectPath = ProjectPath;
        editorLanguage = EditorLanguage;
        pluginDirectory = PluginDirectory;
        userInterface = UserInterface;
        textHints = TextHints;
        cancellationToken = CancellationToken;
    }

    private sealed class UnavailablePluginSecretStore : IPluginSecretStore
    {
        public static UnavailablePluginSecretStore Instance { get; } = new();

        public Task<bool> ContainsAsync(
            string name,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromResult(false);
        }

        public Task<string?> ReadAsync(
            string name,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromResult<string?>(null);
        }

        public Task WriteAsync(
            string name,
            string value,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromException(
                new NotSupportedException(
                    "This plugin menu context does not provide a secret store."));
        }

        public Task DeleteAsync(
            string name,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            return Task.FromException(
                new NotSupportedException(
                    "This plugin menu context does not provide a secret store."));
        }
    }
}

public delegate Task<PluginResult> PluginMenuHandler(PluginMenuContext context);

public sealed record PluginMenuCommand(
    string Id,
    PluginMenuLocation Location,
    int Order,
    string Label,
    PluginMenuHandler Handler);

public sealed record PluginMapSummary(
    string Key,
    string DisplayName);

public sealed record PluginMapSnapshot(
    string Key,
    string DisplayName,
    int Width,
    int Height,
    string Revision,
    IReadOnlyList<PluginMapLayerSnapshot> Layers);

public sealed record PluginMapLayerSnapshot(
    string Name,
    string TilesetKey,
    IReadOnlyList<IReadOnlyList<int?>> Tiles,
    IReadOnlyList<IReadOnlyList<string?>> AutoTiles)
{
    public IReadOnlyList<PluginMapActorSnapshot> Actors { get; init; } =
        Array.Empty<PluginMapActorSnapshot>();
}

public sealed record PluginMapActorRectSnapshot(
    int X,
    int Y,
    int Width,
    int Height);

public sealed record PluginMapActorSnapshot(
    int X,
    int Y,
    string ImagePath,
    PluginMapActorRectSnapshot? SourceRect,
    bool IsCharacter,
    int Direction,
    double TranslationX,
    double TranslationY,
    double ScaleX,
    double ScaleY,
    double OriginX,
    double OriginY,
    double Rotation,
    double Hue);

public sealed record PluginTilesetSnapshot(
    string Key,
    string ImagePath,
    int TileWidth,
    int TileHeight,
    int TileCount,
    IReadOnlyList<bool> Passable);

public sealed record PluginMapLayerWriteRequest(
    string MapKey,
    string LayerName,
    string BaseRevision,
    string ExpectedTilesetKey,
    IReadOnlyList<IReadOnlyList<int?>> Tiles,
    IReadOnlyList<IReadOnlyList<string?>> AutoTiles);

public sealed record PluginMapWriteResult(
    bool Success,
    bool Conflict,
    string Error,
    string CurrentRevision)
{
    public static PluginMapWriteResult Completed(string currentRevision)
    {
        return new PluginMapWriteResult(
            true,
            false,
            string.Empty,
            currentRevision);
    }

    public static PluginMapWriteResult Conflicted(string currentRevision)
    {
        return new PluginMapWriteResult(
            false,
            true,
            string.Empty,
            currentRevision);
    }

    public static PluginMapWriteResult Failed(
        string error,
        string currentRevision)
    {
        return new PluginMapWriteResult(
            false,
            false,
            error,
            currentRevision);
    }
}

public sealed record PluginMapContextMenuContext(
    string ProjectPath,
    string MapKey,
    string EditorLanguage,
    string PluginDirectory,
    string PluginDataDirectory,
    IPluginUserInterface UserInterface,
    ITextHintRefresh TextHints,
    IPluginSecretStore SecretStore,
    IMapEditorHost MapEditorHost,
    CancellationToken CancellationToken);

public delegate Task<PluginResult> PluginMapContextMenuHandler(
    PluginMapContextMenuContext context);

public sealed record PluginMapContextMenuCommand(
    string Id,
    int Order,
    string Label,
    PluginMapContextMenuHandler Handler);

public sealed record BlueprintAssistantProviderContext(
    string PluginId,
    string PluginDirectory,
    string PluginDataDirectory,
    string EditorLanguage,
    IPluginSecretStore SecretStore);

public sealed record BlueprintAssistantSettings(
    string Provider,
    string Model,
    string Endpoint,
    string Organization,
    string Project,
    bool HasApiKey,
    string MaskedApiKey)
{
    public BlueprintAssistantSettings(
        string provider,
        string model,
        string endpoint,
        string organization,
        string project,
        bool hasApiKey)
        : this(
            provider,
            model,
            endpoint,
            organization,
            project,
            hasApiKey,
            string.Empty)
    {
    }
}

public sealed record BlueprintAssistantSettingsUpdate(
    string Provider,
    string Model,
    string Endpoint,
    string Organization,
    string Project,
    string? NewApiKey,
    bool RemoveApiKey);

public enum BlueprintAssistantMessageRole
{
    User,
    Assistant,
}

public sealed record BlueprintAssistantMessage(
    BlueprintAssistantMessageRole Role,
    string Content,
    DateTimeOffset CreatedAt);

public sealed record BlueprintAssistantTurnContext(
    BlueprintAssistantProviderContext ProviderContext,
    string ConversationId,
    string ProjectPath,
    string BlueprintKey,
    string BaseRevision,
    IReadOnlyList<BlueprintAssistantMessage> Messages,
    IBlueprintAssistantWorkspace Workspace,
    CancellationToken CancellationToken);

public enum BlueprintAssistantEventKind
{
    TextDelta,
    Status,
    ToolStarted,
    ToolCompleted,
    ProposalReady,
    Completed,
}

public sealed record BlueprintAssistantEvent(
    BlueprintAssistantEventKind Kind,
    string Text,
    string ToolName,
    BlueprintAssistantProposal? Proposal)
{
    public static BlueprintAssistantEvent TextDelta(string text)
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.TextDelta,
            text,
            string.Empty,
            null);
    }

    public static BlueprintAssistantEvent Status(string text)
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.Status,
            text,
            string.Empty,
            null);
    }

    public static BlueprintAssistantEvent ToolStarted(string toolName)
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.ToolStarted,
            string.Empty,
            toolName,
            null);
    }

    public static BlueprintAssistantEvent ToolCompleted(string toolName, string text)
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.ToolCompleted,
            text,
            toolName,
            null);
    }

    public static BlueprintAssistantEvent ProposalReady(BlueprintAssistantProposal proposal)
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.ProposalReady,
            string.Empty,
            string.Empty,
            proposal);
    }

    public static BlueprintAssistantEvent Completed()
    {
        return new BlueprintAssistantEvent(
            BlueprintAssistantEventKind.Completed,
            string.Empty,
            string.Empty,
            null);
    }
}

public sealed record BlueprintAssistantToolResult(
    bool Success,
    string Content,
    string Error)
{
    public static BlueprintAssistantToolResult Completed(string content)
    {
        return new BlueprintAssistantToolResult(true, content, string.Empty);
    }

    public static BlueprintAssistantToolResult Failed(string error)
    {
        return new BlueprintAssistantToolResult(false, string.Empty, error);
    }
}

public sealed record BlueprintAssistantProposal(
    string Id,
    string Title,
    string Diff,
    string BaseRevision,
    bool IsValid,
    IReadOnlyList<string> Diagnostics);

public sealed record BlueprintAssistantProposalResult(
    bool Success,
    string Error,
    BlueprintAssistantProposal? Proposal)
{
    public static BlueprintAssistantProposalResult Completed(
        BlueprintAssistantProposal proposal)
    {
        return new BlueprintAssistantProposalResult(true, string.Empty, proposal);
    }

    public static BlueprintAssistantProposalResult Failed(string error)
    {
        return new BlueprintAssistantProposalResult(false, error, null);
    }
}

public sealed record BlueprintAssistantApplyResult(
    bool Success,
    bool Conflict,
    string Error,
    string CurrentRevision);

public sealed record BlueprintAssistantCandidateResult(
    bool Success,
    string Error,
    string CandidateJson);
