using System.Collections.Generic;

namespace Ludork.Models;

public sealed record FileExplorerWorkspaceState(IReadOnlyList<FileExplorerTabState> Tabs, int ActiveTabNumber);
