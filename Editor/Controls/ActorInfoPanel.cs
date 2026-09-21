using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ActorInfoPanel : UserControl
{
    private const double ClassContentMinimumWidth = 379;
    private readonly ActorInfoViewModel viewModel = new();
    private readonly Dictionary<string, Button> resetButtons = new(StringComparer.Ordinal);
    private ProjectDataStore? historyData;
    private MapPanel? editorPanel;
    private bool loading;

    public event EventHandler<ActorSelectionChangedEventArgs>? ActorTagChanged;
    public event EventHandler<string>? BlueprintOpenRequested;
    public event EventHandler<string>? BlueprintLocateRequested;

    public ActorInfoPanel()
    {
        InitializeComponent();
        DataContext = viewModel;
        TitleContainer.Background = EditorTheme.Brush("Input");
        NoSelectionLabel.Foreground = EditorTheme.Brush("TextMuted");
        TitleLabel.Text = LocaleService.Get("ACTOR_INFO");
        TagLabel.Text = LocaleService.Get("TAG");
        BlueprintLabel.Text = LocaleService.Get("BLUEPRINT");
        BlueprintOpenButton.Content = LocaleService.Get("OPEN");
        BlueprintLocateButton.Content = LocaleService.Get("LOCATE");
        PositionLabel.Text = LocaleService.Get("POSITION");
        LayerReadOnlyLabel.Text = LocaleService.Get("LAYER_READ_ONLY");
        ClassTitleLabel.Text = LocaleService.Get("CLASS_DETAIL");
        ResetAllButton.Content = LocaleService.Get("RESET_ALL_OVERRIDES");
        NoSelectionLabel.Text = LocaleService.Get("GENERAL_DATA_PLACEHOLDER");
        EditorInputs.ApplyEditable(TagEdit);
        EditorInputs.ApplyReadOnly(BlueprintPath);
        EditorInputs.ApplyEditable(PositionX);
        EditorInputs.ApplyEditable(PositionY);
        TagEdit.TextChanged += (_, _) =>
        {
            if (!loading)
                viewModel.SetTag(TagEdit.Text ?? string.Empty);
        };
        PositionX.ValueChanged += onPositionChanged;
        PositionY.ValueChanged += onPositionChanged;
        BlueprintOpenButton.Click += (_, _) =>
        {
            if (!viewModel.IsRuntime && viewModel.HasProjectBlueprint)
                BlueprintOpenRequested?.Invoke(this, viewModel.BlueprintReference);
        };
        BlueprintLocateButton.Click += (_, _) =>
        {
            if (viewModel.HasProjectBlueprint)
                BlueprintLocateRequested?.Invoke(this, viewModel.BlueprintReference);
        };
        ResetAllButton.Click += (_, _) => viewModel.resetAllOverrides();
        ClassForm.ShowSourceGroups = true;
        ClassForm.FieldActionFactory = createResetAction;
        ClassForm.ValueChanged += (_, args) =>
        {
            if (!loading)
                viewModel.SetVariable(args.Name, args.Value, args.RequiresRefresh);
        };
        ScrollArea.ScrollChanged += (_, args) =>
        {
            if (args.ViewportDelta.X != 0)
                DetailContent.Width = Math.Max(ClassContentMinimumWidth, ScrollArea.Viewport.Width);
        };
        viewModel.HeaderChanged += (_, _) => updateHeader();
        viewModel.EditableStateChanged += (_, _) => updateEditableState();
        viewModel.OverridesChanged += (_, _) => updateResetActions();
        viewModel.FieldsChanged += (_, _) => updateClassFields();
        viewModel.ClassRefreshRequested += (_, _) => Dispatcher.UIThread.Post(viewModel.refreshClassDetail);
        viewModel.RefreshSelectedActorRequested += (_, _) => editorPanel?.refreshSelectedActor();
        viewModel.ActorTagChanged += (_, _) => ActorTagChanged?.Invoke(this,
            new ActorSelectionChangedEventArgs(viewModel.MapKey, viewModel.LayerName, viewModel.ActorIndex, viewModel.ActorData?.ToJson()));
        updateHeader();
        updateEditableState();
    }

    public void configure(
        ProjectDataStore nextGameData,
        LuaMetadataService nextMetadataService,
        BlueprintClassResolver nextClassResolver,
        IGameVariableCatalog nextGameVariables,
        MapPanel nextEditorPanel)
    {
        historyData = nextGameData;
        editorPanel = nextEditorPanel;
        ClassForm.AssetsDirectory = Path.Combine(nextGameData.ProjectPath, "Assets");
        ClassForm.ProjectDirectory = nextGameData.ProjectPath;
        ClassForm.CellSize = nextGameData.Configs.getCellSize();
        ClassForm.GameVariables = nextGameVariables;
        viewModel.Configure(nextGameData, nextMetadataService, nextClassResolver);
        updateHistoryContext();
    }

    public void ConfigureEditingContext(IMapEditingContext context)
    {
        if (viewModel.ConfigureEditingContext(context))
            updateHistoryContext();
    }

    public void setActor(string mapKey, string? layerName, int? actorIndex, JsonObject? actorData)
        => viewModel.setActor(mapKey, layerName, actorIndex, actorData is null ? null : MapDocumentCodec.DecodeActor(actorData),
            PositionX.IsKeyboardFocusWithin || PositionY.IsKeyboardFocusWithin, ClassForm.IsKeyboardFocusWithin);

    public void setLayerEditable(bool editable) => viewModel.setLayerEditable(editable);

    internal void refreshActorProperties() => viewModel.refreshClassDetail();

    public void refreshActorPosition()
        => viewModel.refreshActorPosition(PositionX.IsKeyboardFocusWithin || PositionY.IsKeyboardFocusWithin,
            ClassForm.IsKeyboardFocusWithin);

    private void updateHistoryContext()
    {
        ClassForm.HistoryGameData = viewModel.IsRuntime ? null : historyData;
        if (viewModel.IsRuntime)
        {
            HistoryMergeBehavior.Detach(TagEdit);
            HistoryMergeBehavior.Detach(PositionX);
            HistoryMergeBehavior.Detach(PositionY);
            HistoryMergeBehavior.DetachBoundary(this);
        }
        else if (historyData is not null)
        {
            HistoryMergeBehavior.Attach(TagEdit, historyData);
            HistoryMergeBehavior.Attach(PositionX, historyData);
            HistoryMergeBehavior.Attach(PositionY, historyData);
            HistoryMergeBehavior.AttachBoundary(this, historyData);
        }
    }

    private void updateHeader()
    {
        loading = true;
        IsEnabled = viewModel.HasSelection;
        NoSelectionLabel.IsVisible = !viewModel.HasSelection;
        TitleContainer.IsVisible = viewModel.HasSelection;
        ScrollArea.IsVisible = viewModel.HasSelection;
        if (!string.Equals(TagEdit.Text, viewModel.Tag, StringComparison.Ordinal))
            TagEdit.Text = viewModel.Tag;
        BlueprintPath.Text = viewModel.BlueprintReference;
        PositionX.Maximum = viewModel.MaximumX;
        PositionY.Maximum = viewModel.MaximumY;
        PositionX.Value = viewModel.PositionX;
        PositionY.Value = viewModel.PositionY;
        loading = false;
    }

    private void updateEditableState()
    {
        if (viewModel.CanEdit && !viewModel.IsRuntime)
            EditorInputs.ApplyEditable(TagEdit);
        else
            EditorInputs.ApplyReadOnly(TagEdit);
        PositionX.IsReadOnly = !viewModel.CanMove;
        PositionY.IsReadOnly = !viewModel.CanMove;
        PositionX.Focusable = viewModel.CanMove;
        PositionY.Focusable = viewModel.CanMove;
        ClassForm.IsReadOnly = !viewModel.CanEdit;
        BlueprintOpenButton.IsEnabled = !viewModel.IsRuntime && viewModel.HasProjectBlueprint;
        BlueprintLocateButton.IsEnabled = viewModel.HasProjectBlueprint;
        LayerReadOnlyLabel.IsVisible = viewModel.HasSelection && !viewModel.CanEdit;
        ResetAllButton.IsVisible = !viewModel.IsRuntime;
        ClassSeparator.IsVisible = viewModel.ClassDetailVisible;
        ClassTitleContainer.IsVisible = viewModel.ClassDetailVisible;
        ClassForm.IsVisible = viewModel.ClassDetailVisible;
        updateResetActions();
    }

    private void updateClassFields()
    {
        loading = true;
        resetButtons.Clear();
        if (viewModel.FormFields.Count == 0)
            ClassForm.Clear();
        else
            ClassForm.SetFields(viewModel.FormFields);
        loading = false;
    }

    private Control createResetAction(BlueprintVariableField field)
    {
        Button button = new()
        {
            Content = "↶",
            Width = 24,
            Height = 28,
            Padding = new Thickness(0),
            IsVisible = !viewModel.IsRuntime && viewModel.IsOverridden(field.Name),
            IsEnabled = viewModel.CanEdit && !viewModel.IsRuntime,
        };
        ToolTip.SetTip(button, LocaleService.Get("RESET_OVERRIDE"));
        button.Click += (_, _) => viewModel.resetOverride(field.Name);
        resetButtons[field.Name] = button;
        return button;
    }

    private void updateResetActions()
    {
        foreach (KeyValuePair<string, Button> pair in resetButtons)
        {
            pair.Value.IsVisible = !viewModel.IsRuntime && viewModel.IsOverridden(pair.Key);
            pair.Value.IsEnabled = viewModel.CanEdit && !viewModel.IsRuntime;
        }
        ResetAllButton.IsEnabled = viewModel.CanEdit && !viewModel.IsRuntime && viewModel.HasOverrides;
    }

    private void onPositionChanged(object? sender, NumericUpDownValueChangedEventArgs args)
    {
        if (!loading)
            viewModel.SetPosition(decimal.ToInt32(PositionX.Value ?? 0), decimal.ToInt32(PositionY.Value ?? 0));
    }
}
