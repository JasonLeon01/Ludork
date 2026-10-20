using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Linq;

namespace Ludork.Services;

internal sealed class PlistConfigDocument : ConfigDocument
{
    private readonly Dictionary<string, Dictionary<string, string>> sections;

    public PlistConfigDocument(Dictionary<string, Dictionary<string, string>>? sections = null)
    {
        this.sections = sections ?? new(StringComparer.OrdinalIgnoreCase);
    }

    public IReadOnlyDictionary<string, Dictionary<string, string>> Sections => sections;

    public static PlistConfigDocument Parse(string text)
    {
        XDocument document;
        try
        {
            using StringReader input = new(text);
            using XmlReader reader = XmlReader.Create(input, new XmlReaderSettings
            {
                DtdProcessing = DtdProcessing.Ignore,
                XmlResolver = null,
            });
            document = XDocument.Load(reader, LoadOptions.PreserveWhitespace);
        }
        catch (XmlException exception)
        {
            throw new InvalidDataException("Invalid XML property list: " + exception.Message, exception);
        }

        XElement? root = document.Root;
        if (root is null || root.Name != "plist" || root.Attribute("version")?.Value != "1.0")
            throw invalidStructure();
        XElement[] children = readChildren(root);
        if (children.Length != 1 || children[0].Name != "dict")
            throw invalidStructure();

        PlistConfigDocument result = new();
        foreach (KeyValuePair<string, XElement> section in readDictionary(children[0]))
        {
            if (section.Value.Name != "dict")
                throw invalidStructure();
            Dictionary<string, string> values = new(StringComparer.OrdinalIgnoreCase);
            foreach (KeyValuePair<string, XElement> value in readDictionary(section.Value))
            {
                if (value.Value.Name != "string" || value.Value.HasElements)
                    throw invalidStructure();
                if (!values.TryAdd(value.Key, value.Value.Value))
                    throw new InvalidDataException($"Duplicate property list key in section '{section.Key}': {value.Key}");
            }
            if (!result.sections.TryAdd(section.Key, values))
                throw new InvalidDataException($"Duplicate property list section: {section.Key}");
        }
        return result;
    }

    public override string? GetValue(string section, string key)
    {
        return sections.TryGetValue(section, out Dictionary<string, string>? values)
            && values.TryGetValue(key, out string? value) ? value : null;
    }

    public override void SetValue(string section, string key, string value)
    {
        if (!sections.TryGetValue(section, out Dictionary<string, string>? values))
        {
            values = new(StringComparer.OrdinalIgnoreCase);
            sections.Add(section, values);
        }
        values[key] = value;
    }

    public override string ToText()
    {
        XElement root = new("dict");
        foreach (KeyValuePair<string, Dictionary<string, string>> section in sections)
        {
            XElement values = new("dict");
            foreach (KeyValuePair<string, string> value in section.Value)
            {
                values.Add(new XElement("key", value.Key));
                values.Add(new XElement("string", value.Value));
            }
            root.Add(new XElement("key", section.Key));
            root.Add(values);
        }
        XDocument document = new(
            new XDeclaration("1.0", "UTF-8", null),
            new XDocumentType("plist", "-//Apple//DTD PLIST 1.0//EN",
                "http://www.apple.com/DTDs/PropertyList-1.0.dtd", null),
            new XElement("plist", new XAttribute("version", "1.0"), root));
        using MemoryStream output = new();
        try
        {
            using XmlWriter writer = XmlWriter.Create(output, new XmlWriterSettings
            {
                Encoding = new UTF8Encoding(false),
                Indent = true,
                IndentChars = "\t",
                NewLineChars = "\n",
                NewLineHandling = NewLineHandling.Entitize,
            });
            document.Save(writer);
        }
        catch (ArgumentException exception)
        {
            throw new InvalidDataException("Invalid property list text: " + exception.Message, exception);
        }
        return Encoding.UTF8.GetString(output.ToArray()) + "\n";
    }

    private static IEnumerable<KeyValuePair<string, XElement>> readDictionary(XElement dictionary)
    {
        XElement[] children = readChildren(dictionary);
        if (children.Length % 2 != 0)
            throw invalidStructure();
        Dictionary<string, XElement> values = new(StringComparer.Ordinal);
        for (int index = 0; index < children.Length; index += 2)
        {
            XElement key = children[index];
            if (key.Name != "key" || key.HasElements)
                throw invalidStructure();
            values[key.Value] = children[index + 1];
        }
        return values;
    }

    private static XElement[] readChildren(XElement element)
    {
        if (element.Nodes().OfType<XText>().Any(text => !string.IsNullOrWhiteSpace(text.Value)))
            throw invalidStructure();
        return element.Elements().ToArray();
    }

    private static InvalidDataException invalidStructure() => new(
        "Configuration property lists must contain a root dictionary of section dictionaries with string values.");
}
