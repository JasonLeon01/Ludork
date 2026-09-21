using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GeneralDataService
{
    public bool CreateGeneralType(string key)
    {
        if (!store.canCreateDocument("General", key))
            return false;
        generalDocuments.RecordChange(key);
        JsonObject entry = new()
        {
            ["params"] = new JsonObject(),
            ["members"] = new JsonObject(),
        };
        generalDocuments[key] = entry;
        store.refreshModifiedState();
        return true;
    }

    public bool RenameGeneralType(string oldKey, string newKey)
    {
        return store.RenameDocumentResource("General", oldKey, newKey);
    }

    public bool DeleteGeneralType(string key)
    {
        return store.DeleteDocumentResource("General", key);
    }

}
