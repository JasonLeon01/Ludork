
namespace Ludork.Models;

public sealed class LuaNodeReturnMetadata
{
    public LuaNodeReturnMetadata(string name, LuaTypeReference type)
    {
        Name = name;
        Type = type;
    }

    public string Name { get; }
    public LuaTypeReference Type { get; }
}
