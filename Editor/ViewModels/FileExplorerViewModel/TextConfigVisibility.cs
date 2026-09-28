using Ludork.Services;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private sealed class TextConfigVisibility
    {
        private readonly string root;
        private readonly HashSet<string> keys;
        private readonly HashSet<string> ancestors = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, bool> visibility = new(PathComparer);

        public TextConfigVisibility(string projectPath, HashSet<string> keys)
        {
            root = Path.Combine(projectPath, "Data", "TextConfigs");
            this.keys = keys;
            foreach (string key in keys)
            {
                int separator = key.LastIndexOf('/');
                while (separator >= 0)
                {
                    ancestors.Add(key[..separator]);
                    if (separator == 0)
                        break;
                    separator = key.LastIndexOf('/', separator - 1);
                }
            }
        }

        public bool IsVisible(string path, bool directory, CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            if (visibility.TryGetValue(path, out bool visible))
                return visible;
            visible = readVisibility(path, directory, token);
            visibility[path] = visible;
            return visible;
        }

        private bool readVisibility(string path, bool directory, CancellationToken token)
        {
            if (directory)
            {
                if (Directory.Exists(path) && EditorPathSandbox.IsLink(path))
                    return true;
                string relative = Path.GetRelativePath(root, path).Replace('\\', '/').Trim('/');
                if (ancestors.Contains(relative))
                    return true;
                return Directory.Exists(path) && new DirectoryInfo(path).EnumerateFileSystemInfos()
                    .Any(file => IsVisible(file.FullName, file is DirectoryInfo, token));
            }
            if (!Path.GetExtension(path).Equals(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
                return true;
            string key = Path.ChangeExtension(Path.GetRelativePath(root, path), null)!.Replace('\\', '/');
            if (keys.Contains(key) || !File.Exists(path))
                return true;
            try
            {
                return !isTextConfig(path, token);
            }
            catch (JsonException)
            {
                return true;
            }
        }

        private static bool isTextConfig(string path, CancellationToken token)
        {
            using StreamReader source = new(path);
            char[] characters = new char[4096];
            byte[] buffer = new byte[Encoding.UTF8.GetMaxByteCount(characters.Length)];
            Encoder encoder = Encoding.UTF8.GetEncoder();
            JsonReaderState state = new();
            int buffered = 0;
            bool typeValue = false;
            bool textConfig = false;
            bool rootObject = false;
            while (true)
            {
                token.ThrowIfCancellationRequested();
                int count = source.Read(characters, 0, characters.Length);
                bool final = count == 0;
                int capacity = buffered + Encoding.UTF8.GetMaxByteCount(count);
                if (buffer.Length < capacity)
                    Array.Resize(ref buffer, Math.Max(capacity, buffer.Length * 2));
                buffered += encoder.GetBytes(characters.AsSpan(0, count), buffer.AsSpan(buffered), final);
                Utf8JsonReader reader = new(buffer.AsSpan(0, buffered), final, state);
                while (reader.Read())
                {
                    token.ThrowIfCancellationRequested();
                    if (reader.CurrentDepth == 0 && reader.TokenType == JsonTokenType.StartObject)
                        rootObject = true;
                    if (typeValue)
                    {
                        textConfig = reader.TokenType == JsonTokenType.String
                            && (reader.ValueTextEquals("plainTextConfig") || reader.ValueTextEquals("richTextConfig"));
                        typeValue = false;
                    }
                    if (rootObject && reader.CurrentDepth == 1 && reader.TokenType == JsonTokenType.PropertyName)
                        typeValue = reader.ValueTextEquals("type");
                }
                int consumed = checked((int)reader.BytesConsumed);
                state = reader.CurrentState;
                buffered -= consumed;
                buffer.AsSpan(consumed, buffered).CopyTo(buffer);
                if (final)
                    return rootObject && textConfig;
            }
        }
    }
}
