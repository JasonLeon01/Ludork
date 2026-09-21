using System.Text.Json.Nodes;

namespace Ludork.Models;

internal enum GeneralDataViewMode
{
    Form,
    Table,
}

internal sealed record GeneralDataTableColumn(string Name, JsonObject Definition);

internal sealed record GeneralDataTableRow(string Id, JsonObject Member);
