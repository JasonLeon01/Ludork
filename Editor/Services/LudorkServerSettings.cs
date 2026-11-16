using System;
using System.Text;

namespace Ludork.Services;

public sealed record LudorkServerSettings(bool Enabled, string Url, string Key)
{
    public static LudorkServerSettings Disabled { get; } = new(false, string.Empty, string.Empty);

    public string? GetValidationError(bool allowHttps = false)
    {
        if (!Enabled)
            return null;
        string urlError = allowHttps ? "LUDORK_SERVER_WEB_URL_INVALID" : "LUDORK_SERVER_URL_INVALID";
        if (string.IsNullOrEmpty(Url)
            || !Uri.TryCreate(Url, UriKind.Absolute, out Uri? uri)
            || (uri.Scheme != "http" && (!allowHttps || uri.Scheme != "https"))
            || uri.Host.Length == 0
            || uri.Port == 0
            || uri.UserInfo.Length != 0
            || uri.Query.Length != 0
            || uri.Fragment.Length != 0)
        {
            return urlError;
        }
        foreach (char character in Url)
        {
            if (char.IsWhiteSpace(character) || char.IsControl(character))
                return urlError;
        }
        int pathStart = Url.IndexOf('/', uri.Scheme.Length + 3);
        string path = pathStart < 0 ? string.Empty : Url[pathStart..];
        if (path.EndsWith('/'))
            path = path[..^1];
        if (path.Length != 0)
        {
            if (path.IndexOf('/', 1) >= 0)
                return urlError;
            string project = Uri.UnescapeDataString(path[1..]);
            if (project is "." or ".." || Encoding.UTF8.GetByteCount(project) > 64
                || project.IndexOfAny(['/', '\\']) >= 0)
            {
                return urlError;
            }
            foreach (char character in project)
            {
                if (char.IsControl(character))
                    return urlError;
            }
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
