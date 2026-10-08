using Avalonia;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.ComponentModel;
using System.Linq;

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
        document.Changed += onDocumentChanged;
    }

    public BlueprintGraphNode Model { get; }
    public bool UsePlainTextInputs { get; internal set; }
    public bool IsInactiveEntry => Model.IsEntry && !document.Connections.Any(connection => connection.IsEntryConnection);
    public double NodeOpacity => IsInactiveEntry ? 0.5 : 1;
    public string EntryNotice => LocaleService.Get(document.InheritsEvents
        ? "BLUEPRINT_EVENT_INACTIVE_INHERITED" : "BLUEPRINT_EVENT_INACTIVE");
    public bool IsUnresolved => !Model.IsResolved;
    public string ToolTip => Model.IsResolved
        ? string.IsNullOrWhiteSpace(Model.Description)
            ? Model.NodeFunction
            : $"{Model.NodeFunction}\n\n{Model.Description}"
        : string.IsNullOrWhiteSpace(Model.Description)
            ? $"{Model.NodeFunction}\n{LocaleService.Get("NODE_UNRESOLVED")}"
            : $"{Model.NodeFunction}\n\n{Model.Description}\n\n{LocaleService.Get("NODE_UNRESOLVED")}";
    public IBrush HeaderBrush => Model.IsEntry
        ? BlueprintGraphBrushes.EventHeader
        : Model.IsResolved && Model.NodeFunction.StartsWith("super.", StringComparison.Ordinal)
            ? BlueprintGraphBrushes.ParentHeader
        : Model.IsVirtual
        ? BlueprintGraphBrushes.VirtualHeader
        : Model.IsResolved
            ? BlueprintGraphBrushes.ResolvedHeader
            : BlueprintGraphBrushes.UnresolvedHeader;

    public void Dispose()
    {
        PropertyChanged -= onViewModelPropertyChanged;
        document.Changed -= onDocumentChanged;
    }

    public void RefreshReadOnly()
    {
        if (isReadOnly() || Model.IsVirtual)
            restoreLocation();
    }

    internal void SetLayoutLocation(Point location)
    {
        Model.X = location.X;
        Model.Y = location.Y;
        Location = location;
    }

    private void onViewModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (!string.Equals(args.PropertyName, nameof(Location), StringComparison.Ordinal))
            return;
        if (isReadOnly() || Model.IsVirtual)
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

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (!Model.IsEntry)
            return;
        OnPropertyChanged(nameof(IsInactiveEntry));
        OnPropertyChanged(nameof(NodeOpacity));
    }
}
