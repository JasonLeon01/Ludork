using System;
using System.Collections.Specialized;
using System.Web;

namespace Ludork.Services;

public static class DocumentationService
{
    private static readonly Uri siteRoot = new("https://jasonleon01.github.io/Ludork/");

    public static Uri Help => createUri($"docs/v{EditorVersionService.DocumentationVersion}/");
    public static Uri About => createUri("about/");
    public static Uri Notices => createUri("notices/");

    public static bool IsEmbeddedPage(Uri uri)
    {
        if (!siteRoot.IsBaseOf(uri))
            return false;
        string path = uri.AbsolutePath.TrimEnd('/');
        return path.StartsWith("/Ludork/docs/v", StringComparison.Ordinal)
            || path == "/Ludork/about"
            || path == "/Ludork/notices";
    }

    public static Uri WithEmbeddedMode(Uri uri)
    {
        UriBuilder builder = new(uri);
        NameValueCollection query = HttpUtility.ParseQueryString(builder.Query);
        query["embedded"] = "1";
        builder.Query = query.ToString();
        return builder.Uri;
    }

    private static Uri createUri(string path)
    {
        UriBuilder builder = new(new Uri(siteRoot, path));
        NameValueCollection query = HttpUtility.ParseQueryString(string.Empty);
        query["lang"] = LocaleService.CurrentLanguage;
        query["embedded"] = "1";
        if (!path.StartsWith("docs/", StringComparison.Ordinal))
            query["version"] = EditorVersionService.DocumentationVersion;
        builder.Query = query.ToString();
        return builder.Uri;
    }
}
