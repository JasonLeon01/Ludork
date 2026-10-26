using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal sealed record MapHistoryValidationResult(DocumentReferenceResult References, LuaMetadataDependencySnapshot Dependencies)
{
    internal IReadOnlyDictionary<string, LuaMetadataFileStamp> Files => Dependencies.Stamps;

    internal bool IsCurrent()
        => Dependencies.IsConsistent && Files.All(pair => LuaMetadataFileStamp.Read(pair.Key) == pair.Value);
}
