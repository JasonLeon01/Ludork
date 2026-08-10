using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Ludork.Services;

public static class LocaleBinarySerializer
{
    private static readonly byte[] Magic = "LLOC"u8.ToArray();
    private const ushort FormatVersion = 1;

    public static void Write(string path, IReadOnlyDictionary<string, string> values)
    {
        using FileStream stream = File.Create(path);
        using BinaryWriter writer = new BinaryWriter(stream, Encoding.UTF8, false);
        writer.Write(Magic);
        writer.Write(FormatVersion);
        writer.Write((uint)values.Count);
        foreach (KeyValuePair<string, string> pair in values)
        {
            writeString(writer, pair.Key);
            writeString(writer, pair.Value);
        }
    }

    public static Dictionary<string, string> Read(string path)
    {
        using FileStream stream = File.OpenRead(path);
        using BinaryReader reader = new BinaryReader(stream, Encoding.UTF8, false);
        if (!reader.ReadBytes(Magic.Length).SequenceEqual(Magic))
            throw new InvalidDataException("Invalid Ludork locale binary header.");
        ushort version = reader.ReadUInt16();
        if (version != FormatVersion)
            throw new InvalidDataException($"Unsupported Ludork locale binary version: {version}");
        uint count = reader.ReadUInt32();
        Dictionary<string, string> result = new Dictionary<string, string>(checked((int)count), StringComparer.Ordinal);
        for (uint index = 0u; index < count; index += 1)
            result[readString(reader)] = readString(reader);
        if (stream.Position != stream.Length)
            throw new InvalidDataException("Unexpected trailing data in Ludork locale binary.");
        return result;
    }

    private static void writeString(BinaryWriter writer, string value)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        writer.Write((uint)bytes.Length);
        writer.Write(bytes);
    }

    private static string readString(BinaryReader reader)
    {
        uint byteLength = reader.ReadUInt32();
        if (byteLength > int.MaxValue || byteLength > reader.BaseStream.Length - reader.BaseStream.Position)
            throw new InvalidDataException("Invalid string length in Ludork locale binary.");
        byte[] bytes = reader.ReadBytes((int)byteLength);
        if (bytes.Length != byteLength)
            throw new EndOfStreamException();
        return Encoding.UTF8.GetString(bytes);
    }
}
