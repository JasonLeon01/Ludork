using Ludork.Services;

namespace Ludork.ViewModels;

public sealed class LayerTabViewModel : ViewModelBase
{
    private bool layerVisible;

    public LayerTabViewModel(
        string name,
        bool isOverview,
        bool layerVisible)
    {
        Name = name;
        IsOverview = isOverview;
        this.layerVisible = layerVisible;
    }

    public string Name { get; }
    public bool IsOverview { get; }
    public bool ShowLayerActions => !IsOverview;
    public bool LayerVisible
    {
        get => layerVisible;
        set
        {
            if (!SetProperty(ref layerVisible, value))
                return;
            OnPropertyChanged(nameof(VisibilityTooltip));
        }
    }
    public string VisibilityTooltip => LocaleService.Get(LayerVisible ? "HIDE_LAYER" : "SHOW_LAYER");
}
