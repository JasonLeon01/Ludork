using Avalonia;
using Ludork.Models;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintPreviewService
{
    internal ActorEmitterDescriptor? tryResolveActorEmitter(ResolvedBlueprintClass resolved)
    {
        ResolvedBlueprintField? field = resolved.GetField("emitterComp");
        if (field?.Value is not JsonObject emitter)
            return null;
        JsonObject? defaults = field.BlueprintDefaultValue as JsonObject;
        string resource = (emitter["resource"] ?? defaults?["resource"])?.GetValue<string>() ?? string.Empty;
        if (resource.Length == 0)
            return null;
        (double x, double y) anchor = parseVec2(emitter["anchor"] ?? defaults?["anchor"], 0.5, 0.5);
        (double x, double y) offset = parseVec2(emitter["offset"] ?? defaults?["offset"], 0, 0);
        (double x, double y) scale = parseVec2(emitter["scale"] ?? defaults?["scale"], 1, 1);
        return new ActorEmitterDescriptor(
            resource,
            new Vector(anchor.x, anchor.y),
            new Vector(offset.x, offset.y),
            getDouble(emitter["rotation"] ?? defaults?["rotation"], 0),
            new Vector(scale.x, scale.y),
            getBool(emitter["beforeActor"] ?? defaults?["beforeActor"], false),
            getActorBounds(resolved));
    }
}
