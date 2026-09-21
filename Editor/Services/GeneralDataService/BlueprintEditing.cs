using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GeneralDataService
{
    public bool UpdateGeneralMemberEventGraph(
        string typeKey,
        string memberId,
        string eventName,
        BlueprintGraphSaveResult result)
    {
        return generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            && type["events"] is JsonArray events
            && events.Any(value => string.Equals(ProjectDataStore.getString(value), eventName, StringComparison.Ordinal))
            && type["members"]?[memberId] is JsonObject member
            && store.Blueprints.updateEventGraph("General", typeKey, member, "_graph", eventName, result);
    }

}
