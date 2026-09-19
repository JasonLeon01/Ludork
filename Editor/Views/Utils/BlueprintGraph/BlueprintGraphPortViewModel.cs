using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintGraphPortViewModel : ConnectorViewModelBase, IDisposable
{
    private readonly GameDataService gameData;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory parameterEditorFactory;
    private readonly IGameVariableCatalog gameVariables;
    private readonly Func<string, JsonNode?> getRawSiblingValue;
    private readonly Action<string, JsonNode?> setRawSiblingValue;
    private readonly string assetsDirectory;
    private readonly int cellSize;
    private readonly BlueprintGraphDocument document;
    private readonly Func<bool> isReadOnly;
    private readonly string displayTypeName;
    private IReadOnlyList<BlueprintGraphPortViewModel>? dependencyParameters;
    private BlueprintVariableField? parameterField;
    private BlueprintVariableForm? parameterForm;
    private bool disposed;
    private bool usePlainTextInputs;
    private BlueprintParameterTextDraft? textDraft;

    public BlueprintGraphPortViewModel(
        GameDataService gameData,
        BlueprintGraphPort model,
        BlueprintVariableFieldBuilder fieldBuilder,
        BlueprintNodeParameterEditorFactory parameterEditorFactory,
        IGameVariableCatalog gameVariables,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        string assetsDirectory,
        int cellSize,
        BlueprintGraphDocument document,
        Func<bool> isReadOnly)
    {
        Model = model;
        this.gameData = gameData;
        this.fieldBuilder = fieldBuilder;
        this.parameterEditorFactory = parameterEditorFactory;
        this.gameVariables = gameVariables;
        this.getRawSiblingValue = getRawSiblingValue;
        this.setRawSiblingValue = setRawSiblingValue;
        this.assetsDirectory = assetsDirectory;
        this.cellSize = cellSize;
        this.document = document;
        this.isReadOnly = isReadOnly;
        displayTypeName = model.TypeName;
        Title = EditorDisplayName.Format(model.Name);
        CanConnect = !isReadOnly();
        Flow = model.Direction == BlueprintGraphPortDirection.Input
            ? ConnectorFlow.Input
            : ConnectorFlow.Output;
        IsConnected = model.IsConnected;
        if (model.Direction == BlueprintGraphPortDirection.Input
            && model.Kind == BlueprintGraphPortKind.Params
            && model.SupportsEditor)
        {
            displayTypeName = fieldBuilder.GetNodeParameterDisplayTypeName(model);
        }
        model.PropertyChanged += onModelPropertyChanged;
    }

    public BlueprintGraphPort Model { get; }
    public Control? Editor
    {
        get
        {
            ensureParameterEditor();
            return parameterForm;
        }
    }
    public BlueprintVariableForm? ParameterForm
    {
        get
        {
            ensureParameterEditor();
            return parameterForm;
        }
    }
    public event EventHandler? ParameterValueChanged;
    public event EventHandler? ParameterEdited;
    public event EventHandler? InputDraftChanged;
    public BlueprintParameterTextDraft? TextDraft => textDraft;
    public bool HasInputError => textDraft?.Error is not null;

    public void SetPlainTextInputs(bool enabled, BlueprintParameterTextDraft? draft = null)
    {
        usePlainTextInputs = enabled;
        textDraft = enabled && draft is not null && JsonNode.DeepEquals(draft.Value, Model.Value)
            ? draft
            : null;
        if (parameterForm is not null)
        {
            parameterForm.PlainTextEditorFactory = enabled ? createPlainTextEditor : null;
            parameterForm.RefreshEditors();
        }
        notifyInputDraftProperties();
    }
    public string DisplayTitle => Model.Kind == BlueprintGraphPortKind.Params
        ? $"{Title} ({displayTypeName})" + ((textDraft?.Error ?? Model.ValueDiagnostic) is string diagnostic ? Environment.NewLine + diagnostic : string.Empty)
        : Title;
    public bool IsEditorVisible => Model.IsEditorVisible || HasInputError;
    public IBrush Brush => HasInputError || Model.ValueDiagnostic is not null ? Brushes.OrangeRed : Model.Kind == BlueprintGraphPortKind.Exec
        ? BlueprintGraphBrushes.Execution
        : BlueprintGraphBrushes.Parameter;

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Model.PropertyChanged -= onModelPropertyChanged;
        parameterForm?.Dispose();
        parameterForm = null;
        dependencyParameters = null;
    }

    public void ApplyExternalValue(JsonNode? value)
    {
        if (disposed || isReadOnly() || JsonNode.DeepEquals(Model.Value, value))
            return;
        Model.Value = value?.DeepClone();
        if (parameterField is not null)
            parameterField.Value = value?.DeepClone();
        parameterForm?.SetFieldValue(Model.Name, value);
        document.NotifyChanged();
        ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        ParameterEdited?.Invoke(this, EventArgs.Empty);
    }

    public void SetReadOnly(bool value)
    {
        CanConnect = !value;
        if (parameterForm is not null)
            parameterForm.IsReadOnly = value;
    }

    public void SynchronizeDependencies(IEnumerable<BlueprintGraphPortViewModel> parameters)
    {
        dependencyParameters = parameters as IReadOnlyList<BlueprintGraphPortViewModel>
            ?? parameters.ToArray();
        synchronizeParameterFormDependencies();
    }

    private void ensureParameterEditor()
    {
        if (disposed || parameterForm is not null || !IsEditorVisible)
            return;
        parameterField ??= fieldBuilder.BuildNodeParameter(Model);
        BlueprintVariableForm form = new()
        {
            AssetsDirectory = assetsDirectory,
            ProjectDirectory = Path.GetDirectoryName(assetsDirectory) ?? string.Empty,
            CellSize = cellSize,
            GameVariables = gameVariables,
            HistoryGameData = gameData,
            IsReadOnly = isReadOnly(),
            ShowFieldNames = false,
            MinWidth = 180,
            MaxWidth = 280,
        };
        form.CustomValueEditorFactory = request => parameterEditorFactory.Create(
            request,
            getRawSiblingValue,
            setRawSiblingValue,
            () => !disposed);
        form.PlainTextEditorFactory = usePlainTextInputs ? createPlainTextEditor : null;
        form.PointerPressed += onParameterEditorPointerPressed;
        form.SetFields([parameterField]);
        parameterForm = form;
        synchronizeParameterFormDependencies();
        form.ValueChanged += (_, args) =>
        {
            if (isReadOnly() || JsonNode.DeepEquals(Model.Value, args.Value))
                return;
            Model.Value = args.Value?.DeepClone();
            document.NotifyChanged();
            ParameterValueChanged?.Invoke(this, EventArgs.Empty);
            ParameterEdited?.Invoke(this, EventArgs.Empty);
            if (args.RequiresRefresh)
                Dispatcher.UIThread.Post(form.RefreshEditors, DispatcherPriority.Background);
        };
        OnPropertyChanged(nameof(Editor));
        OnPropertyChanged(nameof(ParameterForm));
    }

    private Control createPlainTextEditor(BlueprintVariableEditorRequest request)
    {
        string type = request.Field.UseJsonTableEditor ? Model.TypeName : request.Field.Type;
        if (textDraft is null || !JsonNode.DeepEquals(textDraft.Value, request.Value))
            textDraft = new BlueprintParameterTextDraft(BlueprintNodeTextValues.Format(type, request.Value), request.Value?.DeepClone(), null);
        BlueprintNodeTextValues.TryParse(type, textDraft.Text, out JsonNode? _, out string? error);
        textDraft = textDraft with { Error = error };
        TextBox input = EditorInputs.CreateEditableTextBox(textDraft.Text);
        IBrush? normalBorder = input.BorderBrush;
        void showError()
        {
            input.BorderBrush = textDraft?.Error is null ? normalBorder : Brushes.OrangeRed;
            ToolTip.SetTip(input, textDraft?.Error);
        }
        showError();
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty || disposed || isReadOnly())
                return;
            string text = input.Text ?? string.Empty;
            bool valid = BlueprintNodeTextValues.TryParse(type, text, out JsonNode? value, out string? diagnostic);
            textDraft = new BlueprintParameterTextDraft(text, valid ? value?.DeepClone() : Model.Value?.DeepClone(), diagnostic);
            showError();
            notifyInputDraftProperties();
            if (valid)
                request.Commit(value, false);
            InputDraftChanged?.Invoke(this, EventArgs.Empty);
        };
        return input;
    }

    private void notifyInputDraftProperties()
    {
        OnPropertyChanged(nameof(DisplayTitle));
        OnPropertyChanged(nameof(IsEditorVisible));
        OnPropertyChanged(nameof(Brush));
    }

    private void synchronizeParameterFormDependencies()
    {
        if (parameterForm is null || dependencyParameters is null)
            return;
        foreach (BlueprintGraphPortViewModel parameter in dependencyParameters)
        {
            JsonNode? value = parameter.Model.IsConnected ? null : parameter.Model.Value;
            parameterForm.SetDependencyValue(parameter.Model.Name, value);
        }
    }

    private void onModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (string.Equals(args.PropertyName, nameof(BlueprintGraphPort.IsConnected), StringComparison.Ordinal))
        {
            ensureParameterEditor();
            IsConnected = Model.IsConnected;
            OnPropertyChanged(nameof(IsEditorVisible));
            ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        }
        else if (string.Equals(args.PropertyName, nameof(BlueprintGraphPort.IsEditorVisible), StringComparison.Ordinal))
        {
            ensureParameterEditor();
            OnPropertyChanged(nameof(IsEditorVisible));
        }
        else if (string.Equals(args.PropertyName, nameof(BlueprintGraphPort.ValueDiagnostic), StringComparison.Ordinal))
        {
            OnPropertyChanged(nameof(DisplayTitle));
            OnPropertyChanged(nameof(Brush));
        }
    }

    private static void onParameterEditorPointerPressed(
        object? sender,
        PointerPressedEventArgs args)
    {
        args.Handled = true;
    }
}
