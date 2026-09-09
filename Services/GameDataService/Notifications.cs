using System;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    private void NotifyUiAssetsChanged() => Documents.AfterChangeNotifications(() =>
        UiAssetsChanged?.Invoke(this, EventArgs.Empty));

    private void NotifyDataRestored() => Documents.AfterChangeNotifications(() =>
        DataRestored?.Invoke(this, EventArgs.Empty));
}
