using System.Collections.Generic;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class LocaleWorkbookData
{
    public LocaleWorkbookData(
        IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> languages,
        IReadOnlyList<DuplicateLocaleId> duplicates)
    {
        Languages = languages;
        Duplicates = duplicates;
    }

    public IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> Languages { get; }
    public IReadOnlyList<DuplicateLocaleId> Duplicates { get; }
}
