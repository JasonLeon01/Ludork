
namespace Ludork.Models;

public sealed class BlueprintGraphEventParameterDefinition
{
    public BlueprintGraphEventParameterDefinition(
        string externalKey,
        string name,
        string typeName,
        int index)
    {
        ExternalKey = externalKey;
        Name = name;
        TypeName = typeName;
        Index = index;
    }

    public string ExternalKey { get; }
    public string Name { get; }
    public string TypeName { get; }
    public int Index { get; }
}
