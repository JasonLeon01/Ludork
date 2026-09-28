using Ludork.Services;

namespace Ludork.Controls;

public sealed class ParticlePreviewSurface : PreviewFrameSurface
{
    public void SetFrame(ParticlePreviewFrame frame) => SetFrame(frame.Width, frame.Height, frame.Stride, frame.Pixels);
}
