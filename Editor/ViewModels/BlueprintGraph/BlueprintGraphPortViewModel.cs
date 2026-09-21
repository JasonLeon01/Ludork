using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels.BlueprintGraph;

public sealed class BlueprintGraphPortViewModel : ConnectorViewModelBase, IDisposable
{
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintGraphDocument document;
    private readonly Func<bool> isReadOnly;
    private readonly string displayTypeName;
    private BlueprintVariableField? parameterField;
    private BlueprintParameterTextDraft? textDraft;
    private bool disposed;

    public BlueprintGraphPortViewModel(BlueprintGraphPort model,
        BlueprintVariableFieldBuilder fieldBuilder,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        BlueprintGraphDocument document, Func<bool> isReadOnly)
    {
        Model = model;
        this.fieldBuilder = fieldBuilder;
        this.document = document;
        this.isReadOnly = isReadOnly;
        GetRawSiblingValue = getRawSiblingValue;
        SetRawSiblingValue = setRawSiblingValue;
        displayTypeName = model.Direction == BlueprintGraphPortDirection.Input
            && model.Kind == BlueprintGraphPortKind.Params && model.SupportsEditor
                ? fieldBuilder.GetNodeParameterDisplayTypeName(model) : model.TypeName;
        Title = EditorDisplayName.Format(model.Name);
        CanConnect = !isReadOnly();
        Flow = model.Direction == BlueprintGraphPortDirection.Input ? ConnectorFlow.Input : ConnectorFlow.Output;
        IsConnected = model.IsConnected;
        model.PropertyChanged += onModelPropertyChanged;
    }

    public BlueprintGraphPort Model { get; }
    public BlueprintVariableField ParameterField => parameterField ??= fieldBuilder.BuildNodeParameter(Model);
    public Func<string, JsonNode?> GetRawSiblingValue { get; }
    public Action<string, JsonNode?> SetRawSiblingValue { get; }
    public IReadOnlyList<BlueprintGraphPortViewModel> Dependencies { get; private set; } = [];
    public bool IsReadOnly => isReadOnly();
    public bool IsDisposed => disposed;
    public bool UsePlainTextInputs { get; private set; }
    public BlueprintParameterTextDraft? TextDraft => textDraft;
    public bool HasInputError => textDraft?.Error is not null;
    public event EventHandler? ParameterValueChanged;
    public event EventHandler? ParameterEdited;
    public event EventHandler? InputDraftChanged;
    public event EventHandler? ExternalValueChanged;
    public event EventHandler? DependenciesChanged;
    public event EventHandler? Disposed;

    public string DisplayTitle => Model.Kind == BlueprintGraphPortKind.Params
        ? $"{Title} ({displayTypeName})" + ((textDraft?.Error ?? Model.ValueDiagnostic) is string diagnostic ? Environment.NewLine + diagnostic : string.Empty)
        : Title;
    public bool IsEditorVisible => Model.IsEditorVisible || HasInputError;
    public IBrush Brush => HasInputError || Model.ValueDiagnostic is not null ? Brushes.OrangeRed
        : Model.Kind == BlueprintGraphPortKind.Exec ? BlueprintGraphBrushes.Execution : BlueprintGraphBrushes.Parameter;

    public void SetPlainTextInputs(bool enabled, BlueprintParameterTextDraft? draft = null)
    {
        UsePlainTextInputs = enabled;
        textDraft = enabled && draft is not null && JsonNode.DeepEquals(draft.Value, Model.Value) ? draft : null;
        OnPropertyChanged(nameof(UsePlainTextInputs));
        notifyInputDraftProperties();
    }

    public BlueprintParameterTextDraft PrepareTextDraft(string type, JsonNode? value)
    {
        if (textDraft is null || !JsonNode.DeepEquals(textDraft.Value, value))
            textDraft = new BlueprintParameterTextDraft(BlueprintNodeTextValues.Format(type, value), value?.DeepClone(), null);
        BlueprintNodeTextValues.TryParse(type, textDraft.Text, out _, out string? error);
        textDraft = textDraft with { Error = error };
        return textDraft;
    }

    public bool UpdateTextDraft(string type, string text, out JsonNode? value)
    {
        bool valid = BlueprintNodeTextValues.TryParse(type, text, out value, out string? diagnostic);
        textDraft = new BlueprintParameterTextDraft(text, valid ? value?.DeepClone() : Model.Value?.DeepClone(), diagnostic);
        notifyInputDraftProperties();
        InputDraftChanged?.Invoke(this, EventArgs.Empty);
        return valid;
    }

    public void CommitValue(JsonNode? value)
    {
        if (disposed || IsReadOnly || JsonNode.DeepEquals(Model.Value, value))
            return;
        Model.Value = value?.DeepClone();
        if (parameterField is not null)
            parameterField.Value = value?.DeepClone();
        document.NotifyChanged();
        ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        ParameterEdited?.Invoke(this, EventArgs.Empty);
    }

    public void ApplyExternalValue(JsonNode? value)
    {
        if (disposed || IsReadOnly || JsonNode.DeepEquals(Model.Value, value))
            return;
        CommitValue(value);
        ExternalValueChanged?.Invoke(this, EventArgs.Empty);
    }

    public void SetReadOnly(bool value)
    {
        CanConnect = !value;
        OnPropertyChanged(nameof(IsReadOnly));
    }

    public void SynchronizeDependencies(IEnumerable<BlueprintGraphPortViewModel> parameters)
    {
        Dependencies = parameters as IReadOnlyList<BlueprintGraphPortViewModel> ?? parameters.ToArray();
        DependenciesChanged?.Invoke(this, EventArgs.Empty);
    }

    private void notifyInputDraftProperties()
    {
        OnPropertyChanged(nameof(DisplayTitle));
        OnPropertyChanged(nameof(IsEditorVisible));
        OnPropertyChanged(nameof(Brush));
    }

    private void onModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(BlueprintGraphPort.IsConnected))
        {
            IsConnected = Model.IsConnected;
            OnPropertyChanged(nameof(IsEditorVisible));
            ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        }
        else if (args.PropertyName == nameof(BlueprintGraphPort.IsEditorVisible))
            OnPropertyChanged(nameof(IsEditorVisible));
        else if (args.PropertyName == nameof(BlueprintGraphPort.ValueDiagnostic))
            notifyInputDraftProperties();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Model.PropertyChanged -= onModelPropertyChanged;
        Dependencies = [];
        Disposed?.Invoke(this, EventArgs.Empty);
    }
}
