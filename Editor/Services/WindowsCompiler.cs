namespace Ludork.Services;

public sealed record WindowsCompiler(string Name, string? Compiler, string Source, string Version);

public sealed record WindowsCompilerStatus(bool NeedsCleanup, string? CurrentCompiler, string? TargetCompiler);
