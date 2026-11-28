using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport;

internal sealed class GeneralDataBlueprintExporter(IPluginBlueprintWorkspace workspace)
{
    public string ProjectPath => workspace.ProjectPath;

    public async Task<GeneralDataBlueprintScan> ScanAsync(CancellationToken token)
    {
        IPluginBlueprintSnapshot snapshot = await workspace.CaptureAsync(token);
        return await Task.Run(() =>
        {
            GeneralDataBlueprintScanData scan = GeneralDataBlueprintScanner.Scan(ProjectPath, snapshot, token);
            return new GeneralDataBlueprintScan(scan.Types.Keys.Order(StringComparer.Ordinal).ToArray(), scan.Issues);
        }, token);
    }

    public async Task<GeneralDataBlueprintExportResult> ExportAsync(IReadOnlyDictionary<string, string> directories,
        CancellationToken token)
    {
        IPluginBlueprintSnapshot snapshot = await workspace.CaptureAsync(token);
        GeneralDataBlueprintExportBatch batch = await Task.Run(() =>
        {
            GeneralDataBlueprintScanData scan = GeneralDataBlueprintScanner.Scan(ProjectPath, snapshot, token);
            return GeneralDataBlueprintExportBatch.Prepare(snapshot, directories, scan, token);
        }, token);
        if (batch.Files.Count != 0)
            await snapshot.CreateAsync(batch.Files, token);
        return batch.Result;
    }
}
