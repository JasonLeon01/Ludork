using System.Collections.Generic;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

internal sealed record GeneralDataBlueprintScan(IReadOnlyList<string> Types, IReadOnlyList<GeneralDataBlueprintIssue> Issues);

internal sealed record GeneralDataBlueprintExportResult(int Created, int Existing, int Conflicts, IReadOnlyList<GeneralDataBlueprintIssue> Issues);

internal sealed record GeneralDataBlueprintIssue(GeneralDataBlueprintIssueKind Kind, params string[] Arguments);

internal enum GeneralDataBlueprintIssueKind
{
    InvalidAnnotation,
    InvalidAnnotationJson,
    DuplicateType,
    UnknownType,
    InvalidParent,
    InvalidMemberField,
    InvalidMetadata,
    TypeMissing,
    InvalidDirectory,
    InvalidMemberName,
    TargetCollision,
    FileExists,
}
