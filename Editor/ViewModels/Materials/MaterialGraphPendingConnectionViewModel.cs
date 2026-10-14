using CommunityToolkit.Mvvm.Input;
using NodifyM.Avalonia.ViewModelBase;

namespace Ludork.ViewModels.Materials;

public sealed class MaterialGraphPendingConnectionViewModel : PendingConnectionViewModelBase
{
    public MaterialGraphPendingConnectionViewModel(MaterialGraphViewModel editor) : base(editor)
    {
        CompleteCommand = new RelayCommand<ConnectorViewModelBase?>(target =>
        {
            if (Source is not null && target is not null)
                editor.Connect(Source, target);
            Source = null;
        });
    }

    public RelayCommand<ConnectorViewModelBase?> CompleteCommand { get; }
}
