using Ludork.Models;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class BlueprintEventGraphInitializer
{
    public static BlueprintGraphSaveResult Create(BlueprintGraphNodeDefinition? parentEvent)
    {
        JsonArray nodes = [];
        JsonArray links = [];
        if (parentEvent is not null)
        {
            JsonArray parameters = [];
            foreach (BlueprintGraphPortDefinition port in parentEvent.Ports
                .Where(port => port.Kind == BlueprintGraphPortKind.Params && port.Direction == BlueprintGraphPortDirection.Input)
                .OrderBy(port => port.PinIndex))
            {
                parameters.Add(port.DefaultValue);
                links.Add(new JsonObject
                {
                    ["left"] = $"default_{port.PinIndex}",
                    ["right"] = 0,
                    ["leftOutPin"] = 0,
                    ["rightInPin"] = port.PinIndex,
                    ["linkType"] = "Params",
                });
            }
            nodes.Add(new JsonObject
            {
                ["nodeFunction"] = parentEvent.RuntimePath,
                ["params"] = parameters,
                ["pos"] = new JsonArray(320, 0),
            });
        }
        return new BlueprintGraphSaveResult(
            new JsonObject { ["nodes"] = nodes, ["links"] = links },
            parentEvent is null ? null : JsonValue.Create(0));
    }
}
