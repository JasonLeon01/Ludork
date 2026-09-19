using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed record BlueprintNodeInputState(
    int Index,
    int NodeCount,
    JsonObject Node,
    IReadOnlyDictionary<string, BlueprintParameterTextDraft> Drafts)
{
    internal static IEnumerable<(BlueprintNodeInputState State, int Index)> Match(
        IReadOnlyList<BlueprintNodeInputState> states, JsonArray nodes)
    {
        HashSet<int> restored = [];
        foreach (BlueprintNodeInputState state in states)
        {
            int index = state.Index;
            bool matches = index < nodes.Count && !restored.Contains(index)
                && JsonNode.DeepEquals(state.Node, nodes[index]);
            if (!matches)
            {
                int[] candidates = Enumerable.Range(0, nodes.Count)
                    .Where(candidate => !restored.Contains(candidate) && JsonNode.DeepEquals(state.Node, nodes[candidate])).ToArray();
                if (candidates.Length == 1)
                    index = candidates[0];
                else if (index >= nodes.Count || restored.Contains(index) || nodes[index] is not JsonObject current
                    || !JsonNode.DeepEquals(state.Node["nodeFunction"], current["nodeFunction"])
                    || state.NodeCount != nodes.Count && !JsonNode.DeepEquals(state.Node["pos"], current["pos"]))
                    continue;
            }
            restored.Add(index);
            yield return (state, index);
        }
    }
}
