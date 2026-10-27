using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Services;
using Ludork.ViewModels.Materials;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed partial class MaterialWindow : Window, IProjectSaveParticipant
{
    private readonly ProjectDataStore gameData;
    private readonly EditorDocument document;
    private readonly EditorDocumentBinding binding;
    private readonly ProjectSaveService projectSave;
    private readonly Toast toast;
    private readonly MaterialPreviewClient preview;
    private readonly PreviewFrameSurface surface = new();
    private readonly StackPanel parameterDetails = new() { Spacing = 8, Margin = new Thickness(12) };
    private readonly StackPanel parameterDefault = new() { Spacing = 8 };
    private readonly StackPanel renderingProperties = new() { Spacing = 8, Margin = new Thickness(12) };
    private readonly StackPanel previewProperties = new() { Spacing = 8, Margin = new Thickness(12) };
    private readonly ListBox parameterList = new();
    private readonly TabControl tabs = new();
    private readonly StackPanel diagnostics = new() { Spacing = 4 };
    private readonly List<BlueprintVariableForm> forms = [];
    private readonly MaterialGraphControl graph = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly CancellationTokenSource lifetime = new();
    private JsonObject data;
    private EditorDocumentState documentState;
    private JsonObject? registry;
    private JsonObject? previewAsset;
    private JsonArray? previewDiagnostics;
    private MaterialGraphViewModel? graphModel;
    private string? selectedParameterId;
    private bool refreshingParameters;
    private string generatedShader = string.Empty;
    private string? previewError;
    private bool committing;
    private bool layoutPending;
    private bool rendering;
    private bool closed;
    private long generation;
    private long contentRevision;
    private long previewAssetRevision = -1;
    private string? previewAssetPath;
    private long diagnosticsConnectionGeneration = -1;
    private long diagnosticsRevision = -1;
    private string texturePath = string.Empty;
    private JsonArray frameRect = new(0, 0, 32, 32);
    private JsonArray ambient = new(90, 90, 90, 255);
    private bool lightEnabled = true;

    public MaterialWindow(ProjectDataStore gameData, ProjectSaveService projectSave, PreviewRuntimeService runtime, string key)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        document = gameData.GetDocument("Materials", key) ?? throw new ArgumentException("Material does not exist", nameof(key));
        data = document.Data!;
        documentState = document.CaptureState();
        contentRevision = document.LastContentChangeRevision;
        preview = new MaterialPreviewClient(runtime);
        Width = 1400;
        Height = 900;
        MinWidth = 900;
        MinHeight = 600;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(MaterialWindow));
        toast = new Toast(this);
        AddHandler(GotFocusEvent, (_, _) => FlushPendingChanges(), RoutingStrategies.Bubble);
        binding = new EditorDocumentBinding(this, gameData, () => document,
            () => LocaleService.Get("MATERIAL_EDITOR") + " - " + Key, closeWhenDeleted: true);
        binding.HasPendingInputs = () => layoutPending;
        document.Changed += onDocumentChanged;
        graph.GameData = gameData;
        graph.LayoutCommitRequested += (_, _) => commitPendingLayout();
        projectSave.RegisterParticipant(this);
        AddHandler(PointerPressedEvent, (_, _) => FlushPendingChanges(), RoutingStrategies.Tunnel);
        AddHandler(TextInputEvent, (_, _) => FlushPendingChanges(), RoutingStrategies.Tunnel);
        AddHandler(PointerWheelChangedEvent, (_, args) =>
        {
            if (MaterialGraphControl.IsInputEditor(args.Source as Visual))
                FlushPendingChanges();
        }, RoutingStrategies.Tunnel);
        Deactivated += (_, _) => FlushPendingChanges();
        Closing += (_, _) => FlushPendingChanges();
        Content = DeferredWindowInitializer.CreateLoadingContent();
        _ = new DeferredWindowInitializer(this, async token =>
        {
            registry = await preview.DescribeAsync(token);
            buildContent();
            rebuildGraph();
            timer.Start();
        });
        timer.Tick += async (_, _) => await renderAsync();
        AddHandler(KeyDownEvent, async (_, args) =>
        {
            FlushPendingChanges();
            await binding.HandleShortcutAsync(args, projectSave, toast);
        }, RoutingStrategies.Tunnel);
        Closed += (_, _) => dispose();
    }

    public string Key => document.Key;
    public void SetPreviewEnabled(bool enabled)
    {
        if (enabled && !closed)
            timer.Start();
        else
            timer.Stop();
    }

    private void rebuildGraph()
    {
        if (registry is null)
            return;
        string[] selected = graphModel?.SelectedNodes.OfType<MaterialGraphNodeViewModel>().Select(node => node.Id).ToArray() ?? [];
        if (graphModel is not null)
            graphModel.StructureChanged -= onGraphStructureChanged;
        graphModel = new MaterialGraphViewModel(data, registry, onLayoutChanged, FlushPendingChanges, commit);
        graphModel.StructureChanged += onGraphStructureChanged;
        graph.DataContext = graphModel;
        foreach (string id in selected)
            if (graphModel.FindNode(id) is MaterialGraphNodeViewModel selectedNode)
                graphModel.SelectedNodes.Add(selectedNode);
        refreshParameters();
        refreshRenderingProperties();
        graphModel.ShowDiagnostics(previewDiagnostics);
    }

    private void onGraphStructureChanged(object? sender, EventArgs args) => graphModel?.ShowDiagnostics(previewDiagnostics);

    private void addForm(StackPanel panel, JsonObject descriptor, JsonNode? value, Action<JsonNode?> changed)
    {
        BlueprintVariableForm form = MaterialPropertyForm.Create(gameData, descriptor, value, next =>
        {
            FlushPendingChanges();
            changed(next);
        });
        forms.Add(form);
        panel.Children.Add(form);
    }

    private void clearForms(StackPanel panel)
    {
        foreach (BlueprintVariableForm form in panel.Children.OfType<BlueprintVariableForm>().ToArray())
        {
            form.Dispose();
            forms.Remove(form);
        }
        panel.Children.Clear();
    }

    private void commit()
    {
        committing = true;
        try { gameData.Assets.UpdateMaterial(Key, data); }
        finally { committing = false; }
        documentState = document.CaptureState();
        if (contentRevision != document.LastContentChangeRevision)
        {
            contentRevision = document.LastContentChangeRevision;
            graphModel?.RefreshParameterNames();
            generation++;
        }
    }

    private void onLayoutChanged()
    {
        graph.ScheduleLayoutCommit();
        if (layoutPending)
            return;
        layoutPending = true;
        binding.Refresh();
    }

    public void FlushPendingChanges()
    {
        if (layoutPending)
            graph.CompleteLayoutGesture();
    }

    private void commitPendingLayout()
    {
        if (!layoutPending)
            return;
        layoutPending = false;
        if (document.Exists && !closed)
        {
            if (graphModel is not null && documentState.InternalData is JsonObject previous
                && graphModel.HasSameLayout(previous))
                graphModel.ApplyEditorState(previous);
            else
            {
                graph.EnsureLayoutGesture();
                commit();
            }
        }
        binding.Refresh();
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (committing)
            return;
        EditorDocumentState nextState = document.CaptureState();
        if (ReferenceEquals(documentState, nextState) && contentRevision == document.LastContentChangeRevision)
            return;
        layoutPending = false;
        graph.CancelLayoutGesture();
        documentState = nextState;
        binding.Refresh();
        if (document.Data is not JsonObject next)
            return;
        if (contentRevision != document.LastContentChangeRevision)
        {
            contentRevision = document.LastContentChangeRevision;
            data = next;
            generation++;
            rebuildGraph();
        }
        else if (graphModel is not null)
        {
            graphModel.ApplyEditorState(next);
            graph.RestoreLayout();
        }
        else
            data = next;
    }

    private async Task renderAsync()
    {
        if (rendering || closed || !IsVisible || registry is null || WindowState == WindowState.Minimized)
            return;
        rendering = true;
        long requested = generation;
        try
        {
            string materialPath = GameDataPath.Materials + "/" + Key + ".json";
            if (previewAsset is null || previewAssetRevision != contentRevision || previewAssetPath != materialPath)
            {
                previewAsset = new JsonObject();
                foreach (KeyValuePair<string, JsonNode?> entry in data)
                    if (entry.Key != "editor")
                        previewAsset[entry.Key] = entry.Value?.DeepClone();
                previewAsset["type"] = "material";
                previewAssetRevision = contentRevision;
                previewAssetPath = materialPath;
            }
            MaterialPreviewFrame frame = await preview.RenderAsync(new JsonObject
            {
                ["parameters"] = new JsonObject(),
                ["texturePath"] = texturePath, ["width"] = 640, ["height"] = 300,
                ["time"] = clock.Elapsed.TotalSeconds, ["generation"] = requested,
                ["frameRect"] = frameRect.DeepClone(), ["ambient"] = ambient.DeepClone(),
                ["light"] = lightEnabled, ["background"] = new JsonArray(24, 28, 36, 255),
            }, previewAssetRevision, materialPath, previewAsset, lifetime.Token);
            if (closed || requested != generation)
                return;
            previewError = null;
            generatedShader = frame.GeneratedShader;
            if (diagnosticsConnectionGeneration != frame.ConnectionGeneration || diagnosticsRevision != frame.DiagnosticsRevision)
            {
                diagnosticsConnectionGeneration = frame.ConnectionGeneration;
                diagnosticsRevision = frame.DiagnosticsRevision;
                previewDiagnostics = frame.Diagnostics;
                diagnostics.Children.Clear();
                graphModel?.ShowDiagnostics(previewDiagnostics);
                foreach (JsonObject error in previewDiagnostics.OfType<JsonObject>())
                {
                    Button issue = new() { Content = error["message"]?.ToString() ?? string.Empty, HorizontalAlignment = HorizontalAlignment.Stretch };
                    issue.Click += (_, _) =>
                    {
                        tabs.SelectedIndex = 1;
                        graph.FocusNode(error["nodeId"]?.GetValue<string>() ?? string.Empty);
                    };
                    diagnostics.Children.Add(issue);
                }
            }
            surface.SetFrame(frame.Width, frame.Height, frame.Stride, frame.Pixels);
        }
        catch (OperationCanceledException) when (lifetime.IsCancellationRequested) { }
        catch (Exception exception) when (exception is IOException or InvalidDataException or InvalidOperationException)
        {
            if (closed || requested != generation)
                return;
            if (previewError == exception.Message)
                return;
            previewError = exception.Message;
            surface.Clear();
            generatedShader = string.Empty;
            previewDiagnostics = null;
            diagnosticsConnectionGeneration = -1;
            diagnosticsRevision = -1;
            graphModel?.ShowDiagnostics(null);
            diagnostics.Children.Clear();
            diagnostics.Children.Add(new TextBlock { Text = exception.Message, TextWrapping = TextWrapping.Wrap });
        }
        finally { rendering = false; }
    }

    private void dispose()
    {
        closed = true;
        layoutPending = false;
        graph.CancelLayoutGesture();
        projectSave.UnregisterParticipant(this);
        timer.Stop();
        lifetime.Cancel();
        document.Changed -= onDocumentChanged;
        if (graphModel is not null)
            graphModel.StructureChanged -= onGraphStructureChanged;
        foreach (BlueprintVariableForm form in forms)
            form.Dispose();
        _ = preview.DisposeAsync();
        lifetime.Dispose();
    }
}
