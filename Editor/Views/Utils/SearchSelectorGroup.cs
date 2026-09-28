using System.Collections.Generic;

namespace Ludork.Views.Utils;

internal sealed record SearchSelectorGroup(string Title, IReadOnlyList<string> Options);
