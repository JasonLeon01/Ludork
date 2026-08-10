using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugins.OfficialBlueprintAI.Configuration;

internal sealed record AiProviderProfile(
    string Id,
    string Name,
    string Provider,
    string Model,
    string Endpoint,
    string Organization,
    string Project,
    string CredentialName);

internal sealed record AiSettingsFile(
    int SchemaVersion,
    string ActiveProfileId,
    IReadOnlyList<AiProviderProfile> Profiles);

internal sealed class AiSettingsStore
{
    private const int CurrentSchemaVersion = 1;

    private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };

    private readonly string settingsPath;

    public AiSettingsStore(string pluginDataDirectory)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pluginDataDirectory);
        settingsPath = Path.Combine(pluginDataDirectory, "settings.json");
    }

    public async Task<AiSettingsFile> LoadAsync(CancellationToken cancellationToken)
    {
        if (!File.Exists(settingsPath))
        {
            return CreateDefault();
        }

        await using FileStream stream = new FileStream(
            settingsPath,
            FileMode.Open,
            FileAccess.Read,
            FileShare.Read,
            4096,
            FileOptions.Asynchronous | FileOptions.SequentialScan);
        AiSettingsFile? settings = await JsonSerializer.DeserializeAsync<AiSettingsFile>(
            stream,
            JsonOptions,
            cancellationToken);
        if (settings is null || settings.SchemaVersion != CurrentSchemaVersion)
        {
            throw new InvalidDataException("Unsupported Blueprint AI settings schema.");
        }

        return Normalize(settings);
    }

    public async Task SaveAsync(
        AiSettingsFile settings,
        CancellationToken cancellationToken)
    {
        AiSettingsFile normalized = Normalize(settings);
        string? directory = Path.GetDirectoryName(settingsPath);
        if (string.IsNullOrWhiteSpace(directory))
        {
            throw new InvalidOperationException("Blueprint AI settings path has no directory.");
        }

        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(
            directory,
            $".settings-{Guid.NewGuid():N}.tmp");

        bool moved = false;
        try
        {
            await using (FileStream stream = new FileStream(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                4096,
                FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(
                    stream,
                    normalized,
                    JsonOptions,
                    cancellationToken);
                await stream.FlushAsync(cancellationToken);
            }

            File.Move(temporaryPath, settingsPath, true);
            moved = true;
        }
        finally
        {
            if (!moved && File.Exists(temporaryPath))
            {
                File.Delete(temporaryPath);
            }
        }
    }

    public AiProviderProfile GetActiveProfile(AiSettingsFile settings)
    {
        AiProviderProfile? active = settings.Profiles.FirstOrDefault(
            profile => string.Equals(
                profile.Id,
                settings.ActiveProfileId,
                StringComparison.Ordinal));
        return active ?? settings.Profiles[0];
    }

    public AiSettingsFile UpdateActiveProfile(
        AiSettingsFile settings,
        BlueprintAssistantSettingsUpdate update)
    {
        ValidateSettingsUpdate(update);
        List<AiProviderProfile> profiles = settings.Profiles.ToList();
        string provider = update.Provider.Trim();
        int providerIndex = profiles.FindIndex(
            profile => string.Equals(
                profile.Provider,
                provider,
                StringComparison.OrdinalIgnoreCase));
        string profileId;
        string credentialName;
        string profileName;

        if (providerIndex >= 0)
        {
            AiProviderProfile existing = profiles[providerIndex];
            profileId = existing.Id;
            credentialName = existing.CredentialName;
            profileName = existing.Name;
        }
        else
        {
            profileId = CreateProfileId(provider, profiles);
            credentialName = $"profiles/{profileId}/api-key";
            profileName = provider;
        }

        AiProviderProfile updated = new AiProviderProfile(
            profileId,
            profileName,
            provider,
            update.Model.Trim(),
            NormalizeEndpoint(provider, update.Endpoint),
            update.Organization.Trim(),
            update.Project.Trim(),
            credentialName);

        if (providerIndex >= 0)
        {
            profiles[providerIndex] = updated;
        }
        else
        {
            profiles.Add(updated);
        }

        return new AiSettingsFile(
            CurrentSchemaVersion,
            updated.Id,
            profiles);
    }

    private static AiSettingsFile CreateDefault()
    {
        AiProviderProfile profile = new AiProviderProfile(
            "openai",
            "OpenAI",
            "OpenAI",
            "gpt-5.6-sol",
            "https://api.openai.com/v1",
            string.Empty,
            string.Empty,
            "profiles/openai/api-key");
        return new AiSettingsFile(
            CurrentSchemaVersion,
            profile.Id,
            new List<AiProviderProfile> { profile });
    }

    private static AiSettingsFile Normalize(AiSettingsFile settings)
    {
        if (settings.Profiles is null || settings.Profiles.Count == 0)
        {
            throw new InvalidDataException(
                "Blueprint AI settings must contain at least one provider profile.");
        }
        List<AiProviderProfile> profiles = new List<AiProviderProfile>();
        HashSet<string> profileIds = new HashSet<string>(StringComparer.Ordinal);
        HashSet<string> credentialNames =
            new HashSet<string>(StringComparer.Ordinal);
        foreach (AiProviderProfile profile in settings.Profiles)
        {
            AiProviderProfile normalized = NormalizeProfile(profile);
            if (!profileIds.Add(normalized.Id))
            {
                throw new InvalidDataException(
                    "Blueprint AI settings contain duplicate profile identifiers.");
            }
            if (!credentialNames.Add(normalized.CredentialName))
            {
                throw new InvalidDataException(
                    "Blueprint AI settings contain duplicate credential references.");
            }
            profiles.Add(normalized);
        }

        if (string.IsNullOrWhiteSpace(settings.ActiveProfileId) ||
            !profileIds.Contains(settings.ActiveProfileId))
        {
            throw new InvalidDataException(
                "Blueprint AI settings reference an unknown active profile.");
        }
        return new AiSettingsFile(
            CurrentSchemaVersion,
            settings.ActiveProfileId,
            profiles);
    }

    private static AiProviderProfile NormalizeProfile(AiProviderProfile profile)
    {
        if (profile is null ||
            string.IsNullOrWhiteSpace(profile.Id) ||
            string.IsNullOrWhiteSpace(profile.Name) ||
            string.IsNullOrWhiteSpace(profile.Provider) ||
            string.IsNullOrWhiteSpace(profile.Model) ||
            string.IsNullOrWhiteSpace(profile.Endpoint) ||
            string.IsNullOrWhiteSpace(profile.CredentialName))
        {
            throw new InvalidDataException(
                "Blueprint AI settings contain an incomplete provider profile.");
        }

        string id = profile.Id.Trim();
        if (id.Length == 0 ||
            id[0] == '-' ||
            id[^1] == '-' ||
            id.Any(character =>
                !(char.IsAsciiLetterOrDigit(character) ||
                  character == '-') ||
                char.IsAsciiLetterUpper(character)))
        {
            throw new InvalidDataException(
                "Blueprint AI settings contain an invalid profile identifier.");
        }

        string expectedCredentialName = $"profiles/{id}/api-key";
        if (!string.Equals(
            profile.CredentialName,
            expectedCredentialName,
            StringComparison.Ordinal))
        {
            throw new InvalidDataException(
                "Blueprint AI settings contain an invalid credential reference.");
        }

        return profile with
        {
            Id = id,
            Name = profile.Name.Trim(),
            Provider = profile.Provider.Trim(),
            Model = profile.Model.Trim(),
            Endpoint = NormalizeEndpoint(
                profile.Provider.Trim(),
                profile.Endpoint),
            Organization = profile.Organization?.Trim() ?? string.Empty,
            Project = profile.Project?.Trim() ?? string.Empty,
            CredentialName = expectedCredentialName,
        };
    }

    private static void ValidateSettingsUpdate(
        BlueprintAssistantSettingsUpdate update)
    {
        ArgumentNullException.ThrowIfNull(update);
        if (string.IsNullOrWhiteSpace(update.Provider))
        {
            throw new ArgumentException("Provider is required.", nameof(update));
        }
        if (string.IsNullOrWhiteSpace(update.Model))
        {
            throw new ArgumentException("Model is required.", nameof(update));
        }

        NormalizeEndpoint(update.Provider, update.Endpoint);
    }

    private static string NormalizeEndpoint(string provider, string endpoint)
    {
        string resolved = string.IsNullOrWhiteSpace(endpoint)
            ? GetDefaultEndpoint(provider)
            : endpoint.Trim();
        if (!Uri.TryCreate(resolved, UriKind.Absolute, out Uri? uri))
        {
            throw new ArgumentException("Provider endpoint must be an absolute URI.");
        }
        if (!string.Equals(uri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase) &&
            !(string.Equals(uri.Scheme, Uri.UriSchemeHttp, StringComparison.OrdinalIgnoreCase) &&
              uri.IsLoopback))
        {
            throw new ArgumentException(
                "Provider endpoint must use HTTPS unless it is a loopback address.");
        }
        if (!string.IsNullOrEmpty(uri.UserInfo) ||
            !string.IsNullOrEmpty(uri.Query) ||
            !string.IsNullOrEmpty(uri.Fragment))
        {
            throw new ArgumentException(
                "Provider endpoint cannot contain credentials, a query, or a fragment.");
        }

        return resolved.TrimEnd('/');
    }

    private static string GetDefaultEndpoint(string provider)
    {
        if (string.Equals(provider, "OpenAI", StringComparison.OrdinalIgnoreCase))
        {
            return "https://api.openai.com/v1";
        }
        if (string.Equals(provider, "DeepSeek", StringComparison.OrdinalIgnoreCase))
        {
            return "https://api.deepseek.com";
        }
        if (string.Equals(provider, "Google", StringComparison.OrdinalIgnoreCase))
        {
            return "https://generativelanguage.googleapis.com/v1beta/openai";
        }

        throw new ArgumentException("A custom provider endpoint is required.");
    }

    private static string CreateProfileId(
        string provider,
        IReadOnlyCollection<AiProviderProfile> profiles)
    {
        string baseId = new string(
            provider
                .Trim()
                .ToLowerInvariant()
                .Select(character =>
                    char.IsAsciiLetterOrDigit(character) ? character : '-')
                .ToArray())
            .Trim('-');
        if (string.IsNullOrWhiteSpace(baseId))
        {
            baseId = "provider";
        }

        string candidate = baseId;
        int suffix = 2;
        while (profiles.Any(
            profile => string.Equals(
                profile.Id,
                candidate,
                StringComparison.Ordinal)))
        {
            candidate = $"{baseId}-{suffix}";
            suffix++;
        }

        return candidate;
    }
}
