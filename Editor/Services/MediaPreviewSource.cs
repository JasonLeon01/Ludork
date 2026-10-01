using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server.Features;
using Microsoft.AspNetCore.Http;
using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class MediaPreviewSource : IDisposable
{
    private readonly IWebHost host;

    private MediaPreviewSource(IWebHost host, Uri address)
    {
        this.host = host;
        Address = address;
    }

    public Uri Address { get; }

    public static async Task<MediaPreviewSource> CreateAsync(string path, string html, CancellationToken token)
    {
        string contentType = MediaFileTypes.GetContentType(path) ?? throw new ArgumentException(nameof(path));
        string resourcePath = $"/{Guid.NewGuid():N}/";
        IWebHost host = new WebHostBuilder()
            .UseKestrel(options => options.Listen(IPAddress.Loopback, 0))
            .Configure(app => app.Run(async context =>
            {
                context.Response.Headers.CacheControl = "no-store";
                if (context.Request.Method is not ("GET" or "HEAD"))
                {
                    context.Response.StatusCode = StatusCodes.Status405MethodNotAllowed;
                    return;
                }
                if (context.Request.Path == resourcePath)
                {
                    context.Response.Headers.ContentSecurityPolicy =
                        "default-src 'none'; media-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'";
                    await Results.Content(html, "text/html; charset=utf-8").ExecuteAsync(context);
                    return;
                }
                if (context.Request.Path != resourcePath + "media")
                {
                    context.Response.StatusCode = StatusCodes.Status404NotFound;
                    return;
                }
                try
                {
                    FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete,
                        65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
                    await Results.File(stream, contentType, enableRangeProcessing: true).ExecuteAsync(context);
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    if (context.Response.HasStarted)
                        context.Abort();
                    else
                        context.Response.StatusCode = StatusCodes.Status404NotFound;
                }
            }))
            .Build();
        try
        {
            await host.StartAsync(token).ConfigureAwait(false);
            string address = host.ServerFeatures.Get<IServerAddressesFeature>()!.Addresses.Single();
            return new MediaPreviewSource(host, new Uri(new Uri(address), resourcePath));
        }
        catch
        {
            host.Dispose();
            throw;
        }
    }

    public void Dispose() => host.Dispose();
}
