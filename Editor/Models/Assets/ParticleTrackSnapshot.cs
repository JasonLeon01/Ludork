using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ParticleTrackSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Name => Text("name", "Track");
    public bool Enabled => Boolean("enabled", true);
    public string Mode => Text("mode", "emission");
    public double Duration => Number("duration", 2);
    public double Delay => Number("delay");
    public bool Loop => Boolean("loop", true);
    public bool Prewarm => Boolean("prewarm");
    public int Capacity => (int)Number("capacity", 1024);
    public int Count => (int)Number("count", 32);
    public double Rate => Number("rate", 30);
    public double DistanceRate => Number("distanceRate");
    public IReadOnlyList<ParticleBurstSnapshot> Bursts => Objects("bursts", value => new ParticleBurstSnapshot(value));
    public string Shape => Text("shape", "point");
    public IReadOnlyList<double> Extent => Numbers("extent");
    public double Radius => Number("radius", 16);
    public double InnerRadius => Number("innerRadius", 8);
    public double Direction => Number("direction", -90);
    public double Spread => Number("spread", 30);
    public IReadOnlyList<double> Lifetime => Numbers("lifetime");
    public IReadOnlyList<double> Speed => Numbers("speed");
    public IReadOnlyList<double> SizeMin => Numbers("sizeMin");
    public IReadOnlyList<double> SizeMax => Numbers("sizeMax");
    public IReadOnlyList<double> Rotation => Numbers("rotation");
    public IReadOnlyList<double> AngularVelocity => Numbers("angularVelocity");
    public MapColour ColourMin => MapSnapshotValues.Colour(SnapshotData["colourMin"], new(255,255,255,255));
    public MapColour ColourMax => MapSnapshotValues.Colour(SnapshotData["colourMax"], new(255,255,255,255));
    public IReadOnlyList<double> Gravity => Numbers("gravity");
    public double RadialAcceleration => Number("radialAcceleration");
    public double TangentialAcceleration => Number("tangentialAcceleration");
    public double Damping => Number("damping");
    public string Texture => Text("texture");
    public IReadOnlyList<double> TextureRect => Numbers("textureRect");
    public int Columns => (int)Number("columns", 1);
    public int Rows => (int)Number("rows", 1);
    public int FrameCount => (int)Number("frameCount", 1);
    public double FrameRate => Number("frameRate");
    public bool RandomStartFrame => Boolean("randomStartFrame");
    public bool FrameLoop => Boolean("frameLoop", true);
    public string Space => Text("space", "local");
    public string ScaleMode => Text("scaleMode", "hierarchy");
    public string Blend => Text("blend", "alpha");
    public IReadOnlyList<double> Offset => Numbers("offset");
    public double RotationOffset => Number("rotationOffset");
    public IReadOnlyList<double> Scale => Numbers("scale");
    public IReadOnlyDictionary<string, ParticleCurveBinding> Curves => (SnapshotData["curves"] as JsonObject ?? [])
        .ToDictionary(pair => pair.Key, pair => pair.Value is JsonObject inline
            ? new ParticleCurveBinding(null, new CurveSnapshot(inline))
            : new ParticleCurveBinding(MapSnapshotValues.Text(pair.Value), null));
}
