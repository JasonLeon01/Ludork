using Ludork.Services;
using Ludork.Models;
using Avalonia;
using CommunityToolkit.Mvvm.Input;
using NodifyM.Avalonia.ViewModelBase;
using System;

namespace Ludork.ViewModels.BlueprintGraph;

public sealed class BlueprintPendingConnectionViewModel : PendingConnectionViewModelBase
{
    private readonly BlueprintGraphEditorViewModel editor;

    public BlueprintPendingConnectionViewModel(BlueprintGraphEditorViewModel editor) : base(editor)
    {
        this.editor = editor;
        CompleteCommand = new RelayCommand<ConnectorViewModelBase?>(complete);
    }

    public RelayCommand<ConnectorViewModelBase?> CompleteCommand { get; }
    public event Action<BlueprintGraphPortViewModel>? EmptyDropRequested;
    public BlueprintGraphPortViewModel? PickerSource { get; private set; }
    public Point PickerTarget { get; private set; }
    public bool IsPickerPreviewVisible => PickerSource is not null;

    public void ShowPickerPreview(BlueprintGraphPortViewModel source, Point target)
    {
        ClearPickerPreview();
        PickerTarget = target;
        PickerSource = source;
        source.Disposed += onPickerSourceDisposed;
        OnPropertyChanged(nameof(PickerTarget));
        OnPropertyChanged(nameof(PickerSource));
        OnPropertyChanged(nameof(IsPickerPreviewVisible));
    }

    public void ClearPickerPreview()
    {
        if (PickerSource is null)
            return;
        PickerSource.Disposed -= onPickerSourceDisposed;
        PickerSource = null;
        OnPropertyChanged(nameof(PickerSource));
        OnPropertyChanged(nameof(IsPickerPreviewVisible));
    }

    private void onPickerSourceDisposed(object? sender, EventArgs args) => ClearPickerPreview();

    private void complete(ConnectorViewModelBase? target)
    {
        if (editor.IsReadOnly)
        {
            Source = null;
            return;
        }
        ConnectorViewModelBase? source = Source;
        if (source is null)
            return;
        Source = null;
        if (target is not null && !ReferenceEquals(source, target))
            editor.Connect(source, target);
        else if (target is null && source is BlueprintGraphPortViewModel port
            && port.Model.Direction == BlueprintGraphPortDirection.Output)
            EmptyDropRequested?.Invoke(port);
    }
}
