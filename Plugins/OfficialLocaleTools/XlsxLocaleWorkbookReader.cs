using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Xml;
using System.Xml.Linq;

namespace Ludork.Plugins.OfficialLocaleTools;

internal static class XlsxLocaleWorkbookReader
{
    private sealed record WorkbookRelationship(string Type, string Target, bool External);
    private sealed record SheetRow(int Number, IReadOnlyDictionary<int, string?> Cells);

    public static LocaleWorkbookData Read(
        string workbookPath,
        CancellationToken cancellationToken)
    {
        using FileStream workbookStream = new FileStream(
            workbookPath,
            FileMode.Open,
            FileAccess.Read,
            FileShare.ReadWrite | FileShare.Delete);
        using ZipArchive archive = new ZipArchive(workbookStream, ZipArchiveMode.Read, false);

        XDocument workbookDocument = LoadRequiredXml(archive, "xl/workbook.xml");
        XDocument relationshipDocument = LoadRequiredXml(
            archive,
            "xl/_rels/workbook.xml.rels");
        IReadOnlyDictionary<string, WorkbookRelationship> relationships =
            ReadRelationships(relationshipDocument);
        IReadOnlyList<string> sharedStrings = ReadSharedStrings(archive, relationships);

        Dictionary<string, Dictionary<string, string>> languageMaps =
            new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        Dictionary<string, string> firstLocations =
            new Dictionary<string, string>(StringComparer.Ordinal);
        List<DuplicateLocaleId> duplicates = new List<DuplicateLocaleId>();
        int worksheetCount = 0;

        IEnumerable<XElement> sheets = workbookDocument
            .Descendants()
            .Where(element => element.Name.LocalName == "sheet");
        foreach (XElement sheet in sheets)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string sheetName = AttributeValue(sheet, "name") ?? string.Empty;
            string relationshipId = RelationshipId(sheet);
            if (!relationships.TryGetValue(
                    relationshipId,
                    out WorkbookRelationship? relationship))
            {
                throw new InvalidDataException(
                    "Worksheet relationship was not found for sheet: " + sheetName);
            }
            if (relationship.External ||
                !relationship.Type.EndsWith("/worksheet", StringComparison.Ordinal))
            {
                continue;
            }

            string worksheetPath = ResolveArchivePath("xl/workbook.xml", relationship.Target);
            ZipArchiveEntry worksheetEntry = archive.GetEntry(worksheetPath)
                ?? throw new InvalidDataException(
                    "Worksheet XML was not found for sheet: " + sheetName);
            XDocument worksheetDocument = LoadXml(worksheetEntry);
            ReadWorksheet(
                sheetName,
                worksheetDocument,
                sharedStrings,
                languageMaps,
                firstLocations,
                duplicates,
                cancellationToken);
            worksheetCount++;
        }

        if (worksheetCount == 0)
        {
            throw new InvalidDataException("The locale workbook contains no worksheets.");
        }
        if (languageMaps.Count == 0)
        {
            throw new InvalidDataException("The locale workbook contains no language columns.");
        }

        Dictionary<string, IReadOnlyDictionary<string, string>> resultLanguages =
            new Dictionary<string, IReadOnlyDictionary<string, string>>(StringComparer.Ordinal);
        foreach (KeyValuePair<string, Dictionary<string, string>> language in languageMaps)
        {
            resultLanguages[language.Key] = language.Value;
        }
        return new LocaleWorkbookData(resultLanguages, duplicates);
    }

    private static void ReadWorksheet(
        string sheetName,
        XDocument worksheetDocument,
        IReadOnlyList<string> sharedStrings,
        IDictionary<string, Dictionary<string, string>> languageMaps,
        IDictionary<string, string> firstLocations,
        ICollection<DuplicateLocaleId> duplicates,
        CancellationToken cancellationToken)
    {
        List<SheetRow> rows = ReadRows(
            worksheetDocument,
            sharedStrings,
            cancellationToken);
        SheetRow? headerRow = rows.FirstOrDefault(row => row.Number == 1);
        if (headerRow is null ||
            !headerRow.Cells.TryGetValue(1, out string? firstHeader) ||
            !string.Equals(firstHeader?.Trim(), "ID", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                sheetName + ": row 1 must begin with ID and contain a language column.");
        }

        SortedDictionary<int, string> languages = new SortedDictionary<int, string>();
        foreach (KeyValuePair<int, string?> headerCell in headerRow.Cells)
        {
            if (headerCell.Key < 2 || string.IsNullOrWhiteSpace(headerCell.Value))
            {
                continue;
            }
            string language = headerCell.Value.Trim();
            languages[headerCell.Key] = language;
            if (!languageMaps.ContainsKey(language))
            {
                languageMaps[language] = new Dictionary<string, string>(StringComparer.Ordinal);
            }
        }
        if (languages.Count == 0)
        {
            throw new InvalidDataException(
                sheetName + ": row 1 must contain at least one language column.");
        }

        foreach (SheetRow row in rows.Where(row => row.Number > 1))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!row.Cells.TryGetValue(1, out string? rawId) ||
                string.IsNullOrWhiteSpace(rawId))
            {
                continue;
            }

            string id = rawId.Trim();
            string location = sheetName + "!A" + row.Number.ToString(CultureInfo.InvariantCulture);
            if (firstLocations.TryGetValue(id, out string? firstLocation))
            {
                duplicates.Add(new DuplicateLocaleId(id, firstLocation, location));
            }
            else
            {
                firstLocations[id] = location;
            }

            foreach (KeyValuePair<int, string> language in languages)
            {
                if (row.Cells.TryGetValue(language.Key, out string? value) &&
                    value is not null)
                {
                    languageMaps[language.Value][id] = UnescapeEquals(value);
                }
            }
        }
    }

    private static List<SheetRow> ReadRows(
        XDocument worksheetDocument,
        IReadOnlyList<string> sharedStrings,
        CancellationToken cancellationToken)
    {
        XElement? sheetData = worksheetDocument
            .Descendants()
            .FirstOrDefault(element => element.Name.LocalName == "sheetData");
        if (sheetData is null)
        {
            return new List<SheetRow>();
        }

        List<SheetRow> rows = new List<SheetRow>();
        int nextRowNumber = 1;
        foreach (XElement rowElement in sheetData.Elements()
                     .Where(element => element.Name.LocalName == "row"))
        {
            cancellationToken.ThrowIfCancellationRequested();
            int rowNumber = ParsePositiveInteger(
                AttributeValue(rowElement, "r"),
                nextRowNumber);
            nextRowNumber = rowNumber + 1;

            Dictionary<int, string?> cells = new Dictionary<int, string?>();
            int nextColumn = 1;
            foreach (XElement cellElement in rowElement.Elements()
                         .Where(element => element.Name.LocalName == "c"))
            {
                int column = ParseColumn(
                    AttributeValue(cellElement, "r"),
                    nextColumn);
                nextColumn = column + 1;
                cells[column] = ReadCellValue(cellElement, sharedStrings);
            }
            rows.Add(new SheetRow(rowNumber, cells));
        }
        return rows;
    }

    private static string? ReadCellValue(
        XElement cell,
        IReadOnlyList<string> sharedStrings)
    {
        string cellType = AttributeValue(cell, "t") ?? string.Empty;
        XElement? formula = cell.Elements()
            .FirstOrDefault(element => element.Name.LocalName == "f");
        XElement? cachedValue = cell.Elements()
            .FirstOrDefault(element => element.Name.LocalName == "v");

        if (formula is not null)
        {
            if (cachedValue is not null && cachedValue.Value.Length > 0)
            {
                return DecodeCachedValue(cellType, cachedValue.Value, sharedStrings);
            }
            return FormulaFallback(formula.Value);
        }

        if (cellType == "inlineStr")
        {
            XElement? inlineString = cell.Elements()
                .FirstOrDefault(element => element.Name.LocalName == "is");
            return inlineString is null ? null : ReadStringItem(inlineString);
        }
        if (cachedValue is null)
        {
            return null;
        }
        return DecodeCachedValue(cellType, cachedValue.Value, sharedStrings);
    }

    private static string DecodeCachedValue(
        string cellType,
        string rawValue,
        IReadOnlyList<string> sharedStrings)
    {
        if (cellType == "s")
        {
            if (!int.TryParse(
                    rawValue,
                    NumberStyles.Integer,
                    CultureInfo.InvariantCulture,
                    out int sharedStringIndex) ||
                sharedStringIndex < 0 ||
                sharedStringIndex >= sharedStrings.Count)
            {
                throw new InvalidDataException(
                    "A worksheet contains an invalid shared string index.");
            }
            return sharedStrings[sharedStringIndex];
        }
        if (cellType == "b")
        {
            return rawValue == "1" ? "True" : "False";
        }
        return rawValue;
    }

    private static string? FormulaFallback(string formula)
    {
        string rawFormula = "=" + formula;
        if (rawFormula is "=" or "==")
        {
            return rawFormula;
        }
        string result = rawFormula[1..].Trim();
        return result.Length == 0 ? null : result;
    }

    private static string UnescapeEquals(string value)
    {
        return value is "'=" or "'==" ? value[1..] : value;
    }

    private static IReadOnlyList<string> ReadSharedStrings(
        ZipArchive archive,
        IReadOnlyDictionary<string, WorkbookRelationship> relationships)
    {
        WorkbookRelationship? sharedStringRelationship = relationships.Values
            .FirstOrDefault(relationship =>
                !relationship.External &&
                relationship.Type.EndsWith("/sharedStrings", StringComparison.Ordinal));
        string sharedStringPath = sharedStringRelationship is null
            ? "xl/sharedStrings.xml"
            : ResolveArchivePath("xl/workbook.xml", sharedStringRelationship.Target);
        ZipArchiveEntry? entry = archive.GetEntry(sharedStringPath);
        if (entry is null)
        {
            return Array.Empty<string>();
        }

        XDocument document = LoadXml(entry);
        List<string> values = new List<string>();
        foreach (XElement item in document.Descendants()
                     .Where(element => element.Name.LocalName == "si"))
        {
            values.Add(ReadStringItem(item));
        }
        return values;
    }

    private static string ReadStringItem(XElement item)
    {
        StringBuilder builder = new StringBuilder();
        foreach (XElement child in item.Elements())
        {
            if (child.Name.LocalName == "t")
            {
                builder.Append(child.Value);
            }
            else if (child.Name.LocalName == "r")
            {
                foreach (XElement text in child.Elements()
                             .Where(element => element.Name.LocalName == "t"))
                {
                    builder.Append(text.Value);
                }
            }
        }
        return builder.ToString();
    }

    private static IReadOnlyDictionary<string, WorkbookRelationship> ReadRelationships(
        XDocument relationshipDocument)
    {
        Dictionary<string, WorkbookRelationship> relationships =
            new Dictionary<string, WorkbookRelationship>(StringComparer.Ordinal);
        foreach (XElement relationship in relationshipDocument
                     .Descendants()
                     .Where(element => element.Name.LocalName == "Relationship"))
        {
            string? id = AttributeValue(relationship, "Id");
            string? type = AttributeValue(relationship, "Type");
            string? target = AttributeValue(relationship, "Target");
            if (string.IsNullOrEmpty(id) ||
                string.IsNullOrEmpty(type) ||
                string.IsNullOrEmpty(target))
            {
                continue;
            }
            bool external = string.Equals(
                AttributeValue(relationship, "TargetMode"),
                "External",
                StringComparison.OrdinalIgnoreCase);
            relationships[id] = new WorkbookRelationship(type, target, external);
        }
        return relationships;
    }

    private static XDocument LoadRequiredXml(ZipArchive archive, string path)
    {
        ZipArchiveEntry entry = archive.GetEntry(path)
            ?? throw new InvalidDataException(
                "Required workbook entry was not found: " + path);
        return LoadXml(entry);
    }

    private static XDocument LoadXml(ZipArchiveEntry entry)
    {
        XmlReaderSettings settings = new XmlReaderSettings
        {
            DtdProcessing = DtdProcessing.Prohibit,
            XmlResolver = null,
        };
        using Stream stream = entry.Open();
        using XmlReader reader = XmlReader.Create(stream, settings);
        return XDocument.Load(reader, LoadOptions.PreserveWhitespace);
    }

    private static string RelationshipId(XElement sheet)
    {
        XAttribute? relationshipAttribute = sheet.Attributes()
            .FirstOrDefault(attribute =>
                attribute.Name.LocalName == "id" &&
                attribute.Name.NamespaceName.Contains(
                    "relationships",
                    StringComparison.OrdinalIgnoreCase));
        if (relationshipAttribute is null ||
            string.IsNullOrWhiteSpace(relationshipAttribute.Value))
        {
            throw new InvalidDataException(
                "Worksheet relationship id is missing for sheet: " +
                (AttributeValue(sheet, "name") ?? string.Empty));
        }
        return relationshipAttribute.Value;
    }

    private static string ResolveArchivePath(string sourcePath, string target)
    {
        string normalisedTarget = Uri.UnescapeDataString(target.Replace('\\', '/'));
        string combined;
        if (normalisedTarget.StartsWith("/", StringComparison.Ordinal))
        {
            combined = normalisedTarget.TrimStart('/');
        }
        else
        {
            int separator = sourcePath.LastIndexOf('/');
            string sourceDirectory = separator < 0
                ? string.Empty
                : sourcePath[..separator];
            combined = sourceDirectory.Length == 0
                ? normalisedTarget
                : sourceDirectory + "/" + normalisedTarget;
        }

        List<string> parts = new List<string>();
        foreach (string part in combined.Split('/', StringSplitOptions.RemoveEmptyEntries))
        {
            if (part == ".")
            {
                continue;
            }
            if (part == "..")
            {
                if (parts.Count == 0)
                {
                    throw new InvalidDataException(
                        "Workbook relationship target escapes the archive root.");
                }
                parts.RemoveAt(parts.Count - 1);
                continue;
            }
            parts.Add(part);
        }
        return string.Join("/", parts);
    }

    private static int ParsePositiveInteger(string? value, int fallback)
    {
        if (int.TryParse(
                value,
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out int result) &&
            result > 0)
        {
            return result;
        }
        return fallback;
    }

    private static int ParseColumn(string? cellReference, int fallback)
    {
        if (string.IsNullOrEmpty(cellReference))
        {
            return fallback;
        }

        int column = 0;
        int characterCount = 0;
        foreach (char character in cellReference)
        {
            if (!char.IsLetter(character))
            {
                break;
            }
            int value = char.ToUpperInvariant(character) - 'A' + 1;
            if (value is < 1 or > 26)
            {
                return fallback;
            }
            checked
            {
                column = column * 26 + value;
            }
            characterCount++;
        }
        return characterCount == 0 || column <= 0 ? fallback : column;
    }

    private static string? AttributeValue(XElement element, string localName)
    {
        XAttribute? attribute = element.Attributes()
            .FirstOrDefault(candidate => candidate.Name.LocalName == localName);
        return attribute?.Value;
    }
}
