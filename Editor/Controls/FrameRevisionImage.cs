using Avalonia;
using Avalonia.Controls;

namespace Ludork.Controls;

public sealed class FrameRevisionImage : Image
{
    public static readonly StyledProperty<long> FrameRevisionProperty =
        AvaloniaProperty.Register<FrameRevisionImage, long>(nameof(FrameRevision));

    static FrameRevisionImage()
    {
        AffectsRender<FrameRevisionImage>(FrameRevisionProperty);
    }

    public long FrameRevision
    {
        get => GetValue(FrameRevisionProperty);
        set => SetValue(FrameRevisionProperty, value);
    }
}
