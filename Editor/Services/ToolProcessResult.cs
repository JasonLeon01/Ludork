namespace Ludork.Services;

internal sealed record ToolProcessResult(int ExitCode, string StandardOutput, string StandardError);
