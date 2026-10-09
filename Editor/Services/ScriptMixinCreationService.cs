using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Ludork.Services;

public sealed class ScriptMixinCreationService
{
    private readonly LuaMetadataService metadata;

    public ScriptMixinCreationService(LuaMetadataService metadata)
    {
        this.metadata = metadata;
    }

    public string Create(string destinationPath, LuaTypeReference actorType)
    {
        string root = ScriptMixinPaths.GetMixinsRoot(metadata.ProjectPath);
        string fullPath = Path.GetFullPath(Path.IsPathRooted(destinationPath)
            ? destinationPath : Path.Combine(root, destinationPath));
        if (!Path.HasExtension(fullPath))
            fullPath = Path.ChangeExtension(fullPath, "lua");
        string scriptPath = ScriptMixinPaths.Normalize(Path.GetRelativePath(root, fullPath));
        fullPath = ScriptMixinPaths.GetScriptPath(metadata.ProjectPath, scriptPath);
        string metadataPath = ScriptMixinPaths.GetMetadataPath(metadata.ProjectPath, scriptPath);
        string name = ScriptMixinPaths.GetTypeName(scriptPath);
        if (!LuaIdentifier.IsValid(name) || !name.All(character => character == '_' || char.IsAsciiLetterOrDigit(character)))
            throw new InvalidDataException(LocaleService.Get("SCRIPT_MIXIN_INVALID_NAME"));
        if (File.Exists(fullPath) || File.Exists(metadataPath))
            throw new IOException(LocaleService.Get("SCRIPT_MIXIN_EXISTS"));

        IReadOnlyList<LuaNodeMemberMetadata> events = metadata.GetNodeMembers(actorType, LuaNodeMemberKind.Event);
        HashSet<string> localNames = events.SelectMany(member => member.Parameters)
            .Select(parameter => parameter.Name).ToHashSet(StringComparer.Ordinal);
        localNames.Add(name);
        string parentCall = "super";
        StringBuilder script = new();
        if (localNames.Contains("super") || localNames.Contains("_ENV"))
        {
            parentCall = "parentEvent";
            while (localNames.Contains(parentCall))
                parentCall += "_";
            script.Append("local ").Append(parentCall).Append(" = super\n\n");
        }
        script.Append("local ").Append(name).Append(" = {}\n");
        foreach (LuaNodeMemberMetadata member in events)
        {
            string parameters = string.Join(", ", member.Parameters.Select(parameter => parameter.Name));
            script.Append("\n---@diagnostic disable-next-line: unused\nfunction ").Append(name).Append(':').Append(member.Name)
                .Append('(').Append(parameters).Append(")\n    ");
            if (member.Returns.Count != 0)
                script.Append("return ");
            script.Append(parentCall).Append("().").Append(member.Name).Append('(').Append(parameters).Append(")\nend\n");
        }
        script.Append("\nreturn ").Append(name).Append('\n');
        string metadataText = "local _METADATA = {\n    " + name + " = {\n        attrs = {}\n    }\n}\n\nreturn _METADATA\n";

        bool scriptCreated = false;
        bool metadataCreated = false;
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
            writeNewFile(fullPath, script.ToString(), ref scriptCreated);
            writeNewFile(metadataPath, metadataText, ref metadataCreated);
        }
        catch
        {
            try
            {
                if (metadataCreated)
                    File.Delete(metadataPath);
            }
            finally
            {
                if (scriptCreated)
                    File.Delete(fullPath);
            }
            throw;
        }
        return scriptPath;
    }

    private static void writeNewFile(string path, string content, ref bool created)
    {
        using FileStream stream = new(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
        created = true;
        using StreamWriter writer = new(stream, new UTF8Encoding(false));
        writer.Write(content);
    }
}
