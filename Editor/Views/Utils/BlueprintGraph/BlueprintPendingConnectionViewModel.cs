using CommunityToolkit.Mvvm.Input;
using Ludork.Models;
using NodifyM.Avalonia.ViewModelBase;
using System;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintPendingConnectionViewModel : PendingConnectionViewModelBase
{
    private readonly BlueprintGraphEditorViewModel editor;

    public BlueprintPendingConnectionViewModel(BlueprintGraphEditorViewModel editor) : base(editor)
    {
        this.editor = editor;
        CompleteCommand = new RelayCommand<ConnectorViewModelBase?>(complete);
    }

    public RelayCommand<ConnectorViewModelBase?> CompleteCommand { get; }
    public event EventHandler<BlueprintConnectionDropEventArgs>? EmptyDropRequested;

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
        if (target is null)
        {
            if (source is BlueprintGraphPortViewModel port
                && port.Model.Direction == BlueprintGraphPortDirection.Output)
            {
                EmptyDropRequested?.Invoke(this, new BlueprintConnectionDropEventArgs(port));
            }
            Source = null;
            return;
        }
        if (!ReferenceEquals(source, target))
            editor.Connect(source, target);
        Source = null;
    }
}
