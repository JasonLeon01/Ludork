using Ludork.Services;
using Avalonia.Media;
using Ludork.Models;
using NodifyM.Avalonia.ViewModelBase;

namespace Ludork.ViewModels.BlueprintGraph;

public sealed class BlueprintGraphConnectionViewModel : ConnectionViewModelBase
{
    private readonly BlueprintGraphEditorViewModel editor;

    public BlueprintGraphConnectionViewModel(
        BlueprintGraphEditorViewModel editor,
        BlueprintGraphConnection model,
        BlueprintGraphPortViewModel source,
        BlueprintGraphPortViewModel target) : base(editor, source, target)
    {
        this.editor = editor;
        Model = model;
        IsTypeCompatible = editor.Document.ArePortTypesCompatible(source.Model, target.Model);
        Diagnostic = IsTypeCompatible ? null : $"Cannot connect {source.Model.TypeName} to {target.Model.TypeName}";
    }

    public BlueprintGraphConnection Model { get; }
    public bool IsTypeCompatible { get; }
    public string? Diagnostic { get; }
    public IBrush Stroke => !IsTypeCompatible ? Brushes.OrangeRed : Model.Kind == BlueprintGraphPortKind.Exec
        ? BlueprintGraphBrushes.Execution
        : BlueprintGraphBrushes.Parameter;

    public override void DisconnectConnection(ConnectionViewModelBase connection)
    {
        editor.RemoveConnection(this);
    }
}
