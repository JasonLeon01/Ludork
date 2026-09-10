using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record ReferenceRewrite(string Section, string Key, JsonObject Original, JsonObject Candidate);
