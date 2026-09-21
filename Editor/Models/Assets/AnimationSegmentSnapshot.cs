using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class AnimationSegmentSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Type => Text("type");
    public int Asset => (int)Number("asset");
    public AnimationFrameSnapshot? StartFrame => SnapshotData["startFrame"] is JsonObject value ? new(value) : null;
    public AnimationFrameSnapshot? EndFrame => SnapshotData["endFrame"] is JsonObject value ? new(value) : null;
}
