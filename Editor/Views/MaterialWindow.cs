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
    private MaterialGraphViewModel? graphModel;
    private string? selectedParameterId;
    private bool refreshingParameters;
    private string generatedShader = string.Empty;
    private bool committing;
    private bool layoutPending;
    private bool rendering;
    private bool closed;
    private long generation;
    private long contentRevision;
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
        graphModel = new MaterialGraphViewModel(data, registry, onLayoutChanged, FlushPendingChanges, commit);
        graph.DataContext = graphModel;
        foreach (string id in selected)
            if (graphModel.FindNode(id) is MaterialGraphNodeViewModel selectedNode)
                graphModel.SelectedNodes.Add(selectedNode);
        refreshParameters();
        refreshRenderingProperties();
    }

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
            JsonObject asset = (JsonObject)data.DeepClone();
            asset["type"] = "material";
            (JsonObject response, byte[]? pixels) = await preview.RenderAsync(new JsonObject
            {
                ["asset"] = asset, ["materialPath"] = GameDataPath.Materials + "/" + Key + ".json",
                ["materials"] = gameData.Assets.CaptureMaterials(), ["parameters"] = new JsonObject(),
                ["texturePath"] = texturePath, ["width"] = 640, ["height"] = 300,
                ["time"] = clock.Elapsed.TotalSeconds, ["generation"] = requested,
                ["frameRect"] = frameRect.DeepClone(), ["ambient"] = ambient.DeepClone(),
                ["light"] = lightEnabled, ["background"] = new JsonArray(24, 28, 36, 255),
            }, lifetime.Token);
            if (closed || requested != generation)
                return;
            diagnostics.Children.Clear();
            generatedShader = response["generatedVertex"]?.ToString() + "\n\n" + response["generatedFragment"]?.ToString();
            JsonArray? errors = response["diagnostics"] as JsonArray;
            graphModel?.ShowDiagnostics(errors);
            foreach (JsonObject error in (errors ?? []).OfType<JsonObject>())
            {
                Button issue = new() { Content = error["message"]?.ToString() ?? string.Empty, HorizontalAlignment = HorizontalAlignment.Stretch };
                issue.Click += (_, _) =>
                {
                    tabs.SelectedIndex = 1;
                    graph.FocusNode(error["nodeId"]?.GetValue<string>() ?? string.Empty);
                };
                diagnostics.Children.Add(issue);
            }
            if (pixels is not null)
                surface.SetFrame(response["width"]!.GetValue<int>(), response["height"]!.GetValue<int>(), response["stride"]!.GetValue<int>(), pixels);
            else
            {
                surface.Clear();
                if (response["message"] is JsonNode message)
                    diagnostics.Children.Add(new TextBlock { Text = message.ToString(), TextWrapping = TextWrapping.Wrap });
            }
        }
        catch (OperationCanceledException) when (lifetime.IsCancellationRequested) { }
        catch (Exception exception) when (exception is IOException or InvalidOperationException)
        {
            surface.Clear();
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
        foreach (BlueprintVariableForm form in forms)
            form.Dispose();
        _ = preview.DisposeAsync();
        lifetime.Dispose();
    }
}
