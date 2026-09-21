using Avalonia;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.ComponentModel;

namespace Ludork.ViewModels.BlueprintGraph;

public sealed class BlueprintGraphNodeViewModel : NodeViewModelBase, IDisposable
{
    private readonly BlueprintGraphDocument document;
    private readonly Func<bool> isReadOnly;
    private bool restoringLocation;

    public BlueprintGraphNodeViewModel(
        BlueprintGraphNode model,
        BlueprintGraphDocument document,
        Func<bool> isReadOnly)
    {
        Model = model;
        this.document = document;
        this.isReadOnly = isReadOnly;
        Title = model.Title;
        Location = new Point(model.X, model.Y);
        PropertyChanged += onViewModelPropertyChanged;
        model.PropertyChanged += onModelPropertyChanged;
    }

    public BlueprintGraphNode Model { get; }
    public bool UsePlainTextInputs { get; internal set; }
    public string StartMarker => Model.IsStart ? "S" : string.Empty;
    public bool IsUnresolved => !Model.IsResolved;
    public string ToolTip => Model.IsResolved
        ? string.IsNullOrWhiteSpace(Model.Description)
            ? Model.NodeFunction
            : $"{Model.NodeFunction}\n\n{Model.Description}"
        : string.IsNullOrWhiteSpace(Model.Description)
            ? $"{Model.NodeFunction}\n{LocaleService.Get("NODE_UNRESOLVED")}"
            : $"{Model.NodeFunction}\n\n{Model.Description}\n\n{LocaleService.Get("NODE_UNRESOLVED")}";
    public IBrush HeaderBrush => Model.IsVirtual
        ? BlueprintGraphBrushes.VirtualHeader
        : Model.IsResolved
            ? BlueprintGraphBrushes.ResolvedHeader
            : BlueprintGraphBrushes.UnresolvedHeader;

    public void Dispose()
    {
        PropertyChanged -= onViewModelPropertyChanged;
        Model.PropertyChanged -= onModelPropertyChanged;
    }

    public void RefreshReadOnly()
    {
        if (isReadOnly())
            restoreLocation();
    }

    private void onViewModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (!string.Equals(args.PropertyName, nameof(Location), StringComparison.Ordinal))
            return;
        if (isReadOnly())
        {
            restoreLocation();
            return;
        }
        if (Location.X.Equals(Model.X) && Location.Y.Equals(Model.Y))
            return;
        Model.X = Location.X;
        Model.Y = Location.Y;
        document.NotifyChanged();
    }

    private void restoreLocation()
    {
        if (restoringLocation
            || Math.Abs(Location.X - Model.X) < double.Epsilon
                && Math.Abs(Location.Y - Model.Y) < double.Epsilon)
        {
            return;
        }
        restoringLocation = true;
        Location = new Point(Model.X, Model.Y);
        restoringLocation = false;
    }

    private void onModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (string.Equals(args.PropertyName, nameof(BlueprintGraphNode.IsStart), StringComparison.Ordinal))
            OnPropertyChanged(nameof(StartMarker));
    }
}
