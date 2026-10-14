using NodifyM.Avalonia.ViewModelBase;

namespace Ludork.ViewModels.Materials;

public sealed class MaterialGraphConnectionViewModel(
    MaterialGraphViewModel editor, MaterialGraphPortViewModel source, MaterialGraphPortViewModel target)
    : ConnectionViewModelBase(editor, source, target)
{
    public MaterialGraphPortViewModel TargetPort { get; } = target;

    public override void DisconnectConnection(ConnectionViewModelBase connection) => editor.DisconnectConnector(TargetPort);
}
