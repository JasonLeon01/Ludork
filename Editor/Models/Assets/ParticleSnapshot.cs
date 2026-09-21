using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ParticleSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Name => Text("name");
    public double SimulationRate => Number("simulationRate", 60);
    public int Seed => (int)Number("seed", 1);
    public IReadOnlyList<ParticleTrackSnapshot> Tracks => Objects("tracks", value => new ParticleTrackSnapshot(value));
}
