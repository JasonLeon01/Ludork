using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal void NotifyDataRestored() => Documents.AfterChangeNotifications(() =>
        DataRestored?.Invoke(this, EventArgs.Empty));

}
