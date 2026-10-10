using System;
using System.Text;

namespace Ludork.Services;

public sealed record LudorkServerSettings(bool Enabled, string Url, string Key)
{
    public static LudorkServerSettings Disabled { get; } = new(false, string.Empty, string.Empty);

    public string? GetValidationError()
    {
        if (!Enabled)
            return null;
        if (string.IsNullOrEmpty(Url) || !Url.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
            || !Uri.TryCreate(Url, UriKind.Absolute, out Uri? uri)
            || uri.Scheme != "http"
            || uri.Host.Length == 0
            || uri.Port == 0
            || uri.UserInfo.Length != 0
            || uri.Query.Length != 0
            || uri.Fragment.Length != 0)
        {
            return "LUDORK_SERVER_URL_INVALID";
        }
        foreach (char character in Url)
        {
            if (char.IsWhiteSpace(character) || char.IsControl(character))
                return "LUDORK_SERVER_URL_INVALID";
        }
        int pathStart = Url.IndexOf('/', "http://".Length);
        string path = pathStart < 0 ? string.Empty : Url[pathStart..];
        if (path.EndsWith('/'))
            path = path[..^1];
        if (path.Length <= 1 || path.IndexOf('/', 1) >= 0)
        {
            return "LUDORK_SERVER_URL_INVALID";
        }
        string project = Uri.UnescapeDataString(path[1..]);
        if (project is "." or ".." || Encoding.UTF8.GetByteCount(project) > 64
            || project.IndexOfAny(['/', '\\']) >= 0)
        {
            return "LUDORK_SERVER_URL_INVALID";
        }
        foreach (char character in project)
        {
            if (char.IsControl(character))
                return "LUDORK_SERVER_URL_INVALID";
        }
        if (string.IsNullOrEmpty(Key) || Key.Length > 4096)
            return "LUDORK_SERVER_KEY_REQUIRED";
        foreach (char character in Key)
        {
            if (character is < '!' or > '~')
                return "LUDORK_SERVER_KEY_REQUIRED";
        }
        return null;
    }

    public override string ToString() => nameof(LudorkServerSettings) + " { Redacted }";
}
