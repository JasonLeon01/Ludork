using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodeDefinitionSet
{
    public BlueprintNodeDefinitionSet(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> runtimeLookup,
        IReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> eventParameters)
    {
        Definitions = definitions;
        RuntimeLookup = runtimeLookup;
        Dictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> parameters =
            new(StringComparer.Ordinal);
        foreach (KeyValuePair<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> pair in eventParameters)
            parameters[pair.Key] = pair.Value;
        EventParameters = new ReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>>(parameters);
    }

    public IReadOnlyList<BlueprintGraphNodeDefinition> Definitions { get; }
    public IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> RuntimeLookup { get; }
    public IReadOnlyDictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> EventParameters { get; }
}

public sealed class BlueprintNodeDefinitionCatalog
{
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly BlueprintGraphContext? context;
    private readonly BlueprintEditorDocument? document;
    private readonly Dictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>> eventParameters =
        new(StringComparer.Ordinal);
    private long cachedMetadataRevision = -1;
    private long cachedResolverRevision = -1;
    private JsonObject? cachedContextData;
    private string? cachedContextKey;
    private string? cachedContextParent;
    private ResolvedBlueprintClass? cachedContextClass;
    private BlueprintNodeDefinitionSet? cachedDefinitionSet;

    public BlueprintNodeDefinitionCatalog(
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintEditorDocument document)
    {
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.document = document;
    }

    public BlueprintNodeDefinitionCatalog(
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        BlueprintGraphContext? context)
    {
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.context = context;
    }

    public static BlueprintNodeDefinitionCatalog CreateGlobal(
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        return new BlueprintNodeDefinitionCatalog(metadataService, classResolver, (BlueprintGraphContext?)null);
    }

    public void Invalidate()
    {
        cachedMetadataRevision = -1;
        cachedResolverRevision = -1;
        cachedContextData = null;
        cachedContextKey = null;
        cachedContextParent = null;
        cachedContextClass = null;
        cachedDefinitionSet = null;
        eventParameters.Clear();
    }

    public BlueprintNodeDefinitionSet GetNodeDefinitionSet(
        ResolvedBlueprintClass? resolvedContext = null)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ResolvedBlueprintClass? resolved = ensureContextCache(resolvedContext);
        if (cachedDefinitionSet is not null)
            return cachedDefinitionSet;

        List<BlueprintGraphNodeDefinition> result = [];
        HashSet<string> definitionKeys = new(StringComparer.Ordinal);
        IReadOnlyList<LuaTypeMetadata> contextMro = resolved?.RootType is null
            ? []
            : metadataService.ResolveMro(resolved.RootType);
        HashSet<string> contextTypes = getContextTypeNames(contextMro);
        foreach (LuaNodeMemberMetadata member in metadataService.EnumerateNodeMembers(
                LuaNodeMemberKind.Function)
            .OrderBy(member => getRuntimeRootPriority(member.RuntimePath))
            .ThenBy(member => member.RuntimePath, StringComparer.Ordinal))
        {
            string runtimePath = getGlobalRuntimePath(member.RuntimePath);
            IReadOnlyList<string> aliases = getGlobalRuntimeAliases(member, runtimePath);
            IReadOnlyList<string> pickerPath = getGlobalPickerPath(member.RuntimePath);
            addDefinition(
                result,
                definitionKeys,
                member,
                runtimePath,
                aliases,
                pickerPath,
                false,
                isContextRelevant(member, contextTypes));
        }

        if (resolved?.RootType is not null)
        {
            foreach (LuaNodeMemberMetadata member in metadataService.GetNodeMembers(
                resolved.RootType,
                LuaNodeMemberKind.Function))
            {
                addDefinition(
                    result,
                    definitionKeys,
                    member,
                    member.Name,
                    getParentRuntimeAliases(member),
                    [LocaleService.Get("PARENT")],
                    true,
                    true);
            }
        }

        BlueprintGraphNodeDefinition[] definitions = result.ToArray();
        eventParameters.Clear();
        if (resolved?.RootType is not null)
        {
            foreach (LuaNodeMemberMetadata eventMember in metadataService.GetNodeMembers(
                resolved.RootType,
                LuaNodeMemberKind.Event))
            {
                if (!eventParameters.ContainsKey(eventMember.Name))
                    eventParameters[eventMember.Name] = createEventParameters(eventMember);
            }
        }
        cachedDefinitionSet = new BlueprintNodeDefinitionSet(
            definitions,
            createDefinitionLookup(definitions),
            eventParameters);
        cachedMetadataRevision = metadataService.Revision;
        return cachedDefinitionSet;
    }

    private static IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> createDefinitionLookup(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions)
    {
        Dictionary<string, BlueprintGraphNodeDefinition> result = new(StringComparer.Ordinal);
        Dictionary<string, int> priorities = new(StringComparer.Ordinal);
        foreach (BlueprintGraphNodeDefinition definition in definitions)
        {
            IEnumerable<string> paths = definition.RuntimeAliases
                .Prepend(definition.RuntimePath)
                .Where(path => !string.IsNullOrWhiteSpace(path))
                .Distinct(StringComparer.Ordinal);
            foreach (string path in paths)
            {
                int priority = definition.IsParent ? 1000 : 0;
                priority += string.Equals(path, definition.RuntimePath, StringComparison.Ordinal)
                    ? 200
                    : 100;
                if (priorities.TryGetValue(path, out int currentPriority)
                    && currentPriority >= priority)
                {
                    continue;
                }
                priorities[path] = priority;
                result[path] = definition;
            }
        }
        return result;
    }

    private static IReadOnlyList<BlueprintGraphEventParameterDefinition> createEventParameters(
        LuaNodeMemberMetadata eventMember)
    {
        List<BlueprintGraphEventParameterDefinition> result = [];
        for (int index = 0; index < eventMember.Parameters.Count; index++)
        {
            LuaNodeParameterMetadata parameter = eventMember.Parameters[index];
            result.Add(new BlueprintGraphEventParameterDefinition(
                $"default_{index}",
                parameter.Name,
                parameter.Type.QualifiedName,
                index));
        }
        return result.ToArray();
    }

    private ResolvedBlueprintClass? ensureContextCache(
        ResolvedBlueprintClass? resolvedContext)
    {
        long metadataRevision = metadataService.Revision;
        long resolverRevision = classResolver.Revision;
        JsonObject? data = document?.Data ?? context?.Data;
        string key = document?.BlueprintKey ?? context?.BlueprintKey ?? string.Empty;
        string parent = data?["parent"]?.ToJsonString() ?? string.Empty;
        bool suppliedContextIsCurrent = resolvedContext is not null
            && resolvedContext.ResolverRevision == resolverRevision
            && resolvedContext.MetadataRevision == metadataService.CacheRevision;
        if (cachedMetadataRevision == metadataRevision
            && cachedResolverRevision == resolverRevision
            && ReferenceEquals(cachedContextData, data)
            && string.Equals(cachedContextKey, key, StringComparison.Ordinal)
            && string.Equals(cachedContextParent, parent, StringComparison.Ordinal)
            && (!suppliedContextIsCurrent || ReferenceEquals(cachedContextClass, resolvedContext)))
        {
            return cachedContextClass;
        }

        cachedContextClass = suppliedContextIsCurrent ? resolvedContext : resolveContextClass();
        cachedContextData = data;
        cachedContextKey = key;
        cachedContextParent = parent;
        cachedMetadataRevision = metadataService.Revision;
        cachedResolverRevision = classResolver.Revision;
        cachedDefinitionSet = null;
        eventParameters.Clear();
        return cachedContextClass;
    }

    private ResolvedBlueprintClass? resolveContextClass()
    {
        if (document is not null)
        {
            return classResolver.ResolveBlueprint(
                document.Data,
                document.BlueprintKey);
        }
        return context is null
            ? null
            : classResolver.ResolveBlueprint(context.Data, context.BlueprintKey);
    }

    private static void addDefinition(
        ICollection<BlueprintGraphNodeDefinition> target,
        ISet<string> definitionKeys,
        LuaNodeMemberMetadata member,
        string runtimePath,
        IReadOnlyList<string> runtimeAliases,
        IReadOnlyList<string> pickerPath,
        bool isParent,
        bool isContextRelevant)
    {
        string key = (isParent ? "parent:" : "global:") + member.RuntimePath;
        if (!definitionKeys.Add(key))
            return;
        target.Add(createDefinition(
            member,
            runtimePath,
            runtimeAliases,
            pickerPath,
            isParent,
            isContextRelevant));
    }

    private static BlueprintGraphNodeDefinition createDefinition(
        LuaNodeMemberMetadata member,
        string runtimePath,
        IReadOnlyList<string> runtimeAliases,
        IReadOnlyList<string> pickerPath,
        bool isParent,
        bool isContextRelevant)
    {
        List<BlueprintGraphPortDefinition> ports = [];
        IReadOnlyList<string> executionOutputs = member.Pure
            ? []
            : getExecutionOutputs(member);
        bool hasExecution = !member.Pure;
        if (hasExecution)
        {
            ports.Add(new BlueprintGraphPortDefinition(
                "in",
                BlueprintGraphPortKind.Exec,
                BlueprintGraphPortDirection.Input,
                0));
        }

        for (int index = 0; index < member.Parameters.Count; index++)
        {
            LuaNodeParameterMetadata parameter = member.Parameters[index];
            JsonObject parameterMeta = extractParameterMeta(
                member.Meta,
                parameter.Name,
                member.Parameters.Count == 1);
            ports.Add(new BlueprintGraphPortDefinition(
                parameter.Name,
                BlueprintGraphPortKind.Params,
                BlueprintGraphPortDirection.Input,
                index,
                parameter.Type.QualifiedName,
                index,
                true,
                getParameterDefault(parameter),
                parameterMeta));
        }

        for (int index = 0; index < executionOutputs.Count; index++)
        {
            ports.Add(new BlueprintGraphPortDefinition(
                executionOutputs[index],
                BlueprintGraphPortKind.Exec,
                BlueprintGraphPortDirection.Output,
                index));
        }

        int returnPinIndex = 0;
        foreach (LuaNodeReturnMetadata returnValue in member.Returns)
        {
            ports.Add(new BlueprintGraphPortDefinition(
                returnValue.Name,
                BlueprintGraphPortKind.Params,
                BlueprintGraphPortDirection.Output,
                returnPinIndex,
                returnValue.Type.QualifiedName));
            returnPinIndex++;
        }

        string? displayName = getString(member.Meta["DisplayName"]);
        string memberTitle = EditorDisplayName.Format(member.Name);
        string title = displayName ?? (isParent ? $"(parent){memberTitle}" : memberTitle);
        return new BlueprintGraphNodeDefinition(
            runtimePath,
            title,
            ports,
            member.Meta,
            runtimeAliases,
            pickerPath,
            member.Name,
            string.Empty,
            member.DeclaringType,
            isParent,
            isContextRelevant,
            displayName is not null,
            member.IsLatent);
    }

    private static string getGlobalRuntimePath(string runtimePath)
    {
        string[] parts = runtimePath.Split('.', StringSplitOptions.RemoveEmptyEntries);
        return parts.Length > 1 && isProjectRoot(parts[0])
            ? string.Join('.', parts.Skip(1))
            : runtimePath;
    }

    private static IReadOnlyList<string> getGlobalRuntimeAliases(
        LuaNodeMemberMetadata member,
        string runtimePath)
    {
        HashSet<string> aliases = new(StringComparer.Ordinal)
        {
            runtimePath,
            member.RuntimePath,
        };
        addModuleMemberAliases(aliases, member);
        return aliases.ToArray();
    }

    private static IReadOnlyList<string> getParentRuntimeAliases(LuaNodeMemberMetadata member)
    {
        string rootRelativePath = getGlobalRuntimePath(member.RuntimePath);
        HashSet<string> aliases = new(StringComparer.Ordinal)
        {
            member.Name,
            "self." + member.Name,
            member.RuntimePath,
            rootRelativePath,
        };
        addModuleMemberAliases(aliases, member);
        return aliases.ToArray();
    }

    private static void addModuleMemberAliases(
        ISet<string> aliases,
        LuaNodeMemberMetadata member)
    {
        if (member.DeclaringType.ModuleName is not string moduleName)
            return;
        string moduleMemberPath = moduleName + "." + member.Name;
        aliases.Add(moduleMemberPath);
        aliases.Add(getGlobalRuntimePath(moduleMemberPath));
    }

    private static IReadOnlyList<string> getGlobalPickerPath(string runtimePath)
    {
        string[] parts = runtimePath.Split('.', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length <= 1)
            return [];
        return parts[..^1];
    }

    private static bool isProjectRoot(string name)
    {
        return name is "Source" or "Global" or "Engine";
    }

    private static int getRuntimeRootPriority(string runtimePath)
    {
        string root = runtimePath.Split('.', StringSplitOptions.RemoveEmptyEntries)
            .FirstOrDefault() ?? string.Empty;
        return root switch
        {
            "Source" => 0,
            "Global" => 1,
            "Engine" => 2,
            _ => 3,
        };
    }

    private static HashSet<string> getContextTypeNames(IReadOnlyList<LuaTypeMetadata> contextMro)
    {
        HashSet<string> result = new(StringComparer.Ordinal);
        foreach (LuaTypeMetadata metadata in contextMro)
        {
            result.Add(normalizeTypeName(metadata.Type.QualifiedName));
            result.Add(normalizeTypeName(metadata.Type.TypeName));
        }
        return result;
    }

    private static bool isContextRelevant(
        LuaNodeMemberMetadata member,
        IReadOnlySet<string> contextTypes)
    {
        if (member.RuntimePath.Split('.').Contains("NodeFunctions", StringComparer.Ordinal))
            return true;
        if (matchesContextType(member.DeclaringType, contextTypes)
            || member.Returns.Any(returnValue => matchesContextType(returnValue.Type, contextTypes)))
        {
            return true;
        }
        return member.Parameters.Any(parameter => matchesContextType(parameter.Type, contextTypes));
    }

    private static bool matchesContextType(
        LuaTypeReference type,
        IReadOnlySet<string> contextTypes)
    {
        return contextTypes.Contains(normalizeTypeName(type.QualifiedName))
            || contextTypes.Contains(normalizeTypeName(type.TypeName));
    }

    private static string normalizeTypeName(string typeName)
    {
        string result = typeName;
        while (result.EndsWith("[]", StringComparison.Ordinal))
            result = result[..^2];
        return result;
    }

    private static IReadOnlyList<string> getExecutionOutputs(LuaNodeMemberMetadata member)
    {
        if (member.ExecutionOutputs.Count != 0)
            return member.ExecutionOutputs;
        if (member.IsLatent)
        {
            return member.LatentOutputs.Count != 0
                ? member.LatentOutputs
                : ["Started", "Finished"];
        }
        if (isEnabled(member.LoopNode))
            return ["LoopBody", "Completed"];
        return member.Kind == LuaNodeMemberKind.Event || !member.Pure
            ? ["default"]
            : [];
    }

    private static JsonNode? getParameterDefault(LuaNodeParameterMetadata parameter)
    {
        if (parameter.HasDefaultValue)
            return parameter.DefaultValue?.DeepClone();
        return null;
    }

    private static JsonObject extractParameterMeta(
        JsonObject memberMeta,
        string parameterName,
        bool isOnlyParameter)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> entry in memberMeta)
        {
            if (entry.Key is "DisplayName"
                or "DisplayDesc"
                or "VariableDisplayNames"
                or "ParameterDisplayNames"
                or "VariableDisplayDescs"
                or "ParameterDisplayDescs")
            {
                continue;
            }
            if (tryExtractParameterValue(entry.Value, parameterName, out JsonNode? extracted))
            {
                result[entry.Key] = extracted;
            }
            else if (isOnlyParameter && entry.Value is not null)
            {
                result[entry.Key] = entry.Value.DeepClone();
            }
        }
        return result;
    }

    private static bool tryExtractParameterValue(
        JsonNode? value,
        string parameterName,
        out JsonNode? extracted)
    {
        if (value is JsonObject valueObject)
        {
            if (valueObject.TryGetPropertyValue(parameterName, out JsonNode? namedValue))
            {
                extracted = namedValue?.DeepClone();
                return true;
            }
            extracted = null;
            return false;
        }
        if (value is JsonArray valueArray)
        {
            foreach (JsonNode? item in valueArray)
            {
                if (item is JsonArray tuple
                    && tuple.Count != 0
                    && getString(tuple[0]) is string tupleName
                    && string.Equals(tupleName, parameterName, StringComparison.Ordinal))
                {
                    if (tuple.Count == 2)
                    {
                        extracted = tuple[1]?.DeepClone();
                        return true;
                    }
                    JsonArray remaining = [];
                    for (int index = 1; index < tuple.Count; index++)
                        remaining.Add(tuple[index]?.DeepClone());
                    extracted = remaining;
                    return true;
                }
                if (item is JsonObject itemObject
                    && string.Equals(
                        getString(itemObject["name"] ?? itemObject["field"] ?? itemObject["parameter"]),
                        parameterName,
                        StringComparison.Ordinal))
                {
                    extracted = itemObject.TryGetPropertyValue("value", out JsonNode? itemValue)
                        ? itemValue?.DeepClone()
                        : itemObject.DeepClone();
                    return true;
                }
                if (string.Equals(getString(item), parameterName, StringComparison.Ordinal))
                {
                    extracted = JsonValue.Create(true);
                    return true;
                }
            }
            extracted = null;
            return false;
        }
        if (string.Equals(getString(value), parameterName, StringComparison.Ordinal))
        {
            extracted = JsonValue.Create(true);
            return true;
        }
        extracted = null;
        return false;
    }

    private static bool isEnabled(JsonNode? value)
    {
        if (value is null)
            return false;
        return value is not JsonValue scalar
            || !scalar.TryGetValue(out bool enabled)
            || enabled;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }
}

public sealed record BlueprintGraphContext(JsonObject Data, string? BlueprintKey);
