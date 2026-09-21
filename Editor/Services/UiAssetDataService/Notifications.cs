using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class UiAssetDataService
{
    internal void NotifyUiAssetsChanged() => store.Documents.AfterChangeNotifications(() =>
        UiAssetsChanged?.Invoke(store, EventArgs.Empty));

}
