using Ludork.Models;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record DocumentReferenceResult(
    IReadOnlyList<ReferenceRecord> References,
    IReadOnlyDictionary<string, string> GeneralMemberTypes);
