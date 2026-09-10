using System;

namespace Ludork.Models;

public sealed record LuaTypeReference
{
    public LuaTypeReference(string? moduleName, string typeName)
    {
        if (string.IsNullOrWhiteSpace(typeName))
            throw new ArgumentException("Type name cannot be empty.", nameof(typeName));
        ModuleName = string.IsNullOrWhiteSpace(moduleName) ? null : moduleName.Trim();
        TypeName = typeName.Trim();
    }

    public LuaMetadataType Schema => LuaMetadataType.Parse(QualifiedName);

    public static LuaTypeReference FromSchema(LuaMetadataType schema)
    {
        return schema.Kind == LuaMetadataTypeKind.Named
            ? Parse(schema.Name)
            : new LuaTypeReference(null, schema.ToString());
    }

    public string? ModuleName { get; }
    public string TypeName { get; }
    public string QualifiedName => ModuleName is null ? TypeName : $"{ModuleName}.{TypeName}";

    public static LuaTypeReference Parse(string qualifiedName)
    {
        if (string.IsNullOrWhiteSpace(qualifiedName))
            throw new ArgumentException("Qualified type name cannot be empty.", nameof(qualifiedName));
        string value = qualifiedName.Trim();
        LuaMetadataType schema = LuaMetadataType.Parse(value);
        if (schema.Kind != LuaMetadataTypeKind.Named)
            return new LuaTypeReference(null, schema.ToString());
        int separator = value.LastIndexOf('.');
        return separator <= 0 || separator == value.Length - 1
            ? new LuaTypeReference(null, value)
            : new LuaTypeReference(value[..separator], value[(separator + 1)..]);
    }

    public LuaTypeReference WithDefaultModule(string? moduleName)
    {
        LuaMetadataType schema = Schema;
        bool builtin = schema.Kind != LuaMetadataTypeKind.Named
            || schema.Name is "any" or "nil" or "bool" or "int" or "float" or "number"
                or "double" or "string" or "function" or "event" or "Pair";
        return !builtin && ModuleName is null && !string.IsNullOrWhiteSpace(moduleName)
            ? new LuaTypeReference(moduleName, TypeName)
            : this;
    }

    public override string ToString()
    {
        return QualifiedName;
    }
}
