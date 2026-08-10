using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Emit;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;

namespace Ludork.Services.Plugins;

internal sealed record PluginCompilation(
    PluginAssemblyLoadContext LoadContext,
    Assembly Assembly,
    string Diagnostics);

internal static class PluginCompiler
{
    private static readonly Lazy<IReadOnlyList<MetadataReference>> References =
        new(createMetadataReferences);

    public static PluginCompilation Compile(
        PluginPackage package,
        int registryIndex,
        CancellationToken cancellationToken)
    {
        CSharpParseOptions parseOptions = new(
            LanguageVersion.CSharp13,
            DocumentationMode.None,
            SourceCodeKind.Regular);
        List<SyntaxTree> syntaxTrees = [];
        foreach (string sourcePath in package.SourceFiles)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string source = File.ReadAllText(sourcePath, Encoding.UTF8);
            syntaxTrees.Add(CSharpSyntaxTree.ParseText(
                source,
                parseOptions,
                sourcePath,
                Encoding.UTF8,
                cancellationToken));
        }

        string assemblyName = createAssemblyName(package.Manifest.Id, registryIndex);
        CSharpCompilationOptions compilationOptions = new(
            OutputKind.DynamicallyLinkedLibrary,
            optimizationLevel: OptimizationLevel.Release,
            allowUnsafe: true,
            deterministic: true,
            nullableContextOptions: NullableContextOptions.Enable,
            specificDiagnosticOptions: new Dictionary<string, ReportDiagnostic>
            {
                ["CS1701"] = ReportDiagnostic.Suppress,
                ["CS1702"] = ReportDiagnostic.Suppress,
            });
        CSharpCompilation compilation = CSharpCompilation.Create(
            assemblyName,
            syntaxTrees,
            References.Value,
            compilationOptions);
        using MemoryStream assemblyStream = new();
        using MemoryStream symbolsStream = new();
        EmitOptions emitOptions = new(debugInformationFormat: DebugInformationFormat.PortablePdb);
        EmitResult result = compilation.Emit(
            assemblyStream,
            symbolsStream,
            options: emitOptions,
            cancellationToken: cancellationToken);
        string diagnostics = formatDiagnostics(result.Diagnostics);
        if (!result.Success)
            throw new PluginCompilationException(diagnostics);

        assemblyStream.Position = 0;
        symbolsStream.Position = 0;
        PluginAssemblyLoadContext loadContext = new(assemblyName);
        try
        {
            Assembly assembly = loadContext.LoadFromStream(assemblyStream, symbolsStream);
            return new PluginCompilation(loadContext, assembly, diagnostics);
        }
        catch
        {
            loadContext.Unload();
            throw;
        }
    }

    private static IReadOnlyList<MetadataReference> createMetadataReferences()
    {
        StringComparer comparer = OperatingSystem.IsWindows()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;
        HashSet<string> paths = new(comparer);
        bool hasFrameworkReferences = false;
        string? trustedAssemblies =
            AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string;
        if (!string.IsNullOrWhiteSpace(trustedAssemblies))
        {
            foreach (string path in trustedAssemblies.Split(Path.PathSeparator))
            {
                if (File.Exists(path) && isFrameworkReference(path))
                {
                    paths.Add(path);
                    hasFrameworkReferences = true;
                }
            }
        }
        paths.Add(typeof(IEditorPlugin).Assembly.Location);
        string avaloniaContractPath =
            typeof(IAvaloniaPluginUserInterface).Assembly.Location;
        paths.Add(avaloniaContractPath);
        string? avaloniaDirectory = Path.GetDirectoryName(avaloniaContractPath);
        if (!string.IsNullOrWhiteSpace(avaloniaDirectory))
        {
            foreach (string path in Directory.EnumerateFiles(
                         avaloniaDirectory,
                         "Avalonia*.dll",
                         SearchOption.TopDirectoryOnly))
            {
                paths.Add(path);
            }
        }
        if (!hasFrameworkReferences)
        {
            paths.Add(typeof(object).Assembly.Location);
            paths.Add(typeof(Console).Assembly.Location);
            paths.Add(typeof(Enumerable).Assembly.Location);
            paths.Add(typeof(System.Threading.Tasks.Task).Assembly.Location);
        }
        return paths
            .OrderBy(path => path, StringComparer.Ordinal)
            .Select(path => MetadataReference.CreateFromFile(path))
            .ToArray();
    }

    private static string formatDiagnostics(IEnumerable<Diagnostic> diagnostics)
    {
        return string.Join(
            Environment.NewLine,
            diagnostics
                .Where(diagnostic =>
                    diagnostic.Severity == DiagnosticSeverity.Warning
                    || diagnostic.Severity == DiagnosticSeverity.Error)
                .OrderBy(diagnostic => diagnostic.Location.SourceTree?.FilePath, StringComparer.Ordinal)
                .ThenBy(diagnostic => diagnostic.Location.GetLineSpan().StartLinePosition.Line)
                .ThenBy(diagnostic => diagnostic.Id)
                .Select(diagnostic => diagnostic.ToString()));
    }

    private static bool isFrameworkReference(string path)
    {
        string fileName = Path.GetFileName(path);
        return fileName.Equals("mscorlib.dll", StringComparison.OrdinalIgnoreCase)
            || fileName.Equals("netstandard.dll", StringComparison.OrdinalIgnoreCase)
            || fileName.Equals("System.dll", StringComparison.OrdinalIgnoreCase)
            || fileName.Equals("Microsoft.CSharp.dll", StringComparison.OrdinalIgnoreCase)
            || fileName.StartsWith("System.", StringComparison.OrdinalIgnoreCase);
    }

    private static string createAssemblyName(string pluginId, int registryIndex)
    {
        StringBuilder builder = new($"Ludork.DynamicPlugin.{registryIndex}.");
        foreach (char character in pluginId)
            builder.Append(char.IsLetterOrDigit(character) ? character : '_');
        return builder.ToString();
    }
}

internal sealed class PluginCompilationException : Exception
{
    public PluginCompilationException(string diagnostics)
        : base(string.IsNullOrWhiteSpace(diagnostics)
            ? "Plugin compilation failed."
            : diagnostics)
    {
    }
}
