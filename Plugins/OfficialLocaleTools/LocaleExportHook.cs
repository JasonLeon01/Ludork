using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class LocaleExportHook : IProjectOperationHook
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

    public async Task<PluginResult> ExecuteAsync(ProjectOperationContext context)
    {
        context.CancellationToken.ThrowIfCancellationRequested();
        if (context.Operation == ProjectOperationKind.Pack &&
            context.Packaging is not null)
        {
            context.Packaging.ExcludeFile("Data/Locale/Locale.xlsx");
        }

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
