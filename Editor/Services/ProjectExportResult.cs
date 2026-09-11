namespace Ludork.Services;

public sealed record ProjectExportResult(bool Success, bool Cancelled, string Detail)
{
    public static ProjectExportResult Completed() => new(true, false, string.Empty);
    public static ProjectExportResult CancelledResult() => new(false, true, string.Empty);
    public static ProjectExportResult Failed(string detail) => new(false, false, detail);
}
