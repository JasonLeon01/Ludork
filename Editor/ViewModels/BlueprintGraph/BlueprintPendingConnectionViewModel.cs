using Ludork.Services;
using Ludork.Models;
using CommunityToolkit.Mvvm.Input;
using NodifyM.Avalonia.ViewModelBase;

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
        if (target is not null && !ReferenceEquals(source, target))
            editor.Connect(source, target);
        Source = null;
    }
}
