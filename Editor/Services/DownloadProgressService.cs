using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class DownloadProgressService
{
    public static async Task CopyAsync(HttpResponseMessage response, string destination,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        long? total = response.Content.Headers.ContentLength;
        long completed = 0;
        Stopwatch throttle = Stopwatch.StartNew();
        progress.Report(new(ProjectTemplateStage.Downloading, completed, total));
        await using Stream source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using FileStream target = new(destination, FileMode.CreateNew, FileAccess.Write,
            FileShare.None, 65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[65536];
        int count;
        while ((count = await source.ReadAsync(buffer, cancellationToken)) != 0)
        {
            await target.WriteAsync(buffer.AsMemory(0, count), cancellationToken);
            completed += count;
            if (throttle.ElapsedMilliseconds >= 100)
            {
                progress.Report(new(ProjectTemplateStage.Downloading, completed, total));
                throttle.Restart();
            }
        }
        if (total.HasValue && completed != total.Value)
            throw new InvalidDataException(LocaleService.Get("DOWNLOAD_INCOMPLETE"));
        progress.Report(new(ProjectTemplateStage.Downloading, completed, completed));
    }
}
