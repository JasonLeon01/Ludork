using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class LocaleExportHook : IProjectExportHook
{
    private readonly PluginLocalizer _localizer;
    private readonly LocaleTextHintProvider _textHintProvider;

    public LocaleExportHook(
        PluginLocalizer localizer,
        LocaleTextHintProvider textHintProvider)
    {
        _localizer = localizer;
        _textHintProvider = textHintProvider;
    }

    public ProjectExportFiles GetFiles(string projectPath)
    {
        string workbookPath = LocaleProjectPaths.GetWorkbookPath(projectPath);
        SortedSet<string> outputPaths = new SortedSet<string>(StringComparer.Ordinal);
        if (File.Exists(workbookPath))
        {
            LocaleWorkbookData workbook = XlsxLocaleWorkbookReader.Read(workbookPath, CancellationToken.None);
            foreach (string language in workbook.Languages.Keys)
                outputPaths.Add(LocaleProjectPaths.OutputRelativeDirectory + "/" + LocaleExporter.GetFileName(language));
        }
        HashSet<string> expectedPaths = new HashSet<string>(outputPaths, StringComparer.OrdinalIgnoreCase);
        string outputDirectory = LocaleProjectPaths.GetOutputDirectory(projectPath);
        if (Directory.Exists(outputDirectory))
        {
            foreach (string path in Directory.EnumerateFiles(outputDirectory, "*.lua"))
            {
                string fileName = Path.GetFileName(path);
                string relativePath = LocaleProjectPaths.OutputRelativeDirectory + "/" + fileName;
                if (string.Equals(fileName, "Core.lua", StringComparison.OrdinalIgnoreCase)
                    || expectedPaths.Contains(relativePath))
                    continue;
                using StreamReader reader = new StreamReader(path, new UTF8Encoding(false, true), true);
                if (string.Equals(reader.ReadLine(), LocaleExporter.GeneratedMarker, StringComparison.Ordinal))
                    outputPaths.Add(relativePath);
            }
        }
        return new ProjectExportFiles(new[] { LocaleProjectPaths.WorkbookRelativePath }, outputPaths.ToArray());
    }

    public async Task<PluginResult> ExecuteAsync(ProjectOperationContext context)
    {
        context.CancellationToken.ThrowIfCancellationRequested();
        string workbookPath = LocaleProjectPaths.GetWorkbookPath(context.ProjectPath);
        string outputDirectory =
            LocaleProjectPaths.GetOutputDirectory(context.ProjectPath);
        if (!File.Exists(workbookPath))
        {
            context.Output.WriteLine(_localizer.Format("LOCALE_EXPORT_SKIPPED", workbookPath));
            return PluginResult.Completed();
        }

        try
        {
            LocaleExportResult result = await Task.Run(
                () => LocaleExporter.Export(
                    workbookPath,
                    outputDirectory,
                    context.CancellationToken),
                context.CancellationToken).ConfigureAwait(false);
            IReadOnlyList<DuplicateLocaleId> duplicates = result.Duplicates;
            foreach (DuplicateLocaleId duplicate in duplicates)
            {
                context.Output.WriteLine(
                    _localizer.Format(
                        "LOCALE_DUPLICATE_ID",
                        duplicate.Id,
                        duplicate.DuplicateLocation,
                        duplicate.FirstLocation));
            }
            context.Output.WriteLine(
                _localizer.Format(
                    "LOCALE_EXPORT_COMPLETE",
                    result.LanguageCount,
                    result.EntryCount));
            return PluginResult.Completed();
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception)
        {
            string error = _localizer.Format("LOCALE_EXPORT_FAILED", exception.Message);
            return PluginResult.Failed(error);
        }
        finally
        {
            _textHintProvider.Invalidate();
            context.TextHints.Invalidate();
        }
    }
}
