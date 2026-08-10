using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialBlueprintAI.Providers;

internal static class ProviderHttp
{
    public static HttpMessageHandler CreateDefaultHandler()
    {
        return new HttpClientHandler
        {
            AutomaticDecompression =
                DecompressionMethods.Brotli |
                DecompressionMethods.Deflate |
                DecompressionMethods.GZip,
        };
    }

    public static async Task<HttpResponseMessage> SendAsync(
        HttpClient httpClient,
        HttpRequestMessage request,
        string errorPrefix,
        string apiKey,
        CancellationToken cancellationToken)
    {
        try
        {
            return await httpClient.SendAsync(
                request,
                HttpCompletionOption.ResponseHeadersRead,
                cancellationToken);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception)
        {
            throw SensitiveDataSanitizer.Wrap(
                errorPrefix,
                exception,
                apiKey);
        }
    }

    public static async Task<string?> ReadLineAsync(
        StreamReader reader,
        string errorPrefix,
        string apiKey,
        CancellationToken cancellationToken)
    {
        try
        {
            return await reader.ReadLineAsync(cancellationToken);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception)
        {
            throw SensitiveDataSanitizer.Wrap(
                errorPrefix,
                exception,
                apiKey);
        }
    }

    public static string TruncateError(string error)
    {
        const int limit = 1000;
        string sanitized = error.Replace('\r', ' ').Replace('\n', ' ').Trim();
        return sanitized.Length <= limit ? sanitized : sanitized[..limit];
    }
}
