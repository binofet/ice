using System.Text.Json;
using System.Text.Json.Serialization;
using Metagen;
namespace Metagen.PortableData;

// Passive transport adapter: Python's AST reader stays out of the CLR process.
// This is a derived portable-profile projection, not another authoritative schema.
public static class ProjectionMetaSource {
    private sealed record Projection(string Source, IReadOnlyList<PortableType> Schema);
    private static IMetaValue Value(object? value) => new MetaValue(value?.GetType().FullName ?? "null", value);
    private static IMetaAttribute Attribute(string name, params object?[] arguments) =>
        new MetaAttribute(name, Array.AsReadOnly(arguments.Select(Value).ToArray()), new Dictionary<string, IMetaValue>());
    private static IEnumerable<IMetaAttribute> Tags(IReadOnlyDictionary<string, string> tags) =>
        tags.Select(tag => Attribute("Metagen.MetaTagAttribute", tag.Key, tag.Value));
    private static object? Literal(object? input, PortableTypeRef type) {
        if (input is not JsonElement element || element.ValueKind == JsonValueKind.Null) return null;
        return type.Kind switch {
            ScalarKind.Boolean => element.GetBoolean(),
            ScalarKind.Int32 => element.GetInt32(),
            ScalarKind.Uint32 => element.GetUInt32(),
            ScalarKind.Float32 => element.GetSingle(),
            ScalarKind.Float64 => element.GetDouble(),
            ScalarKind.String => element.GetString(),
            _ => throw new InvalidDataException("Unsupported projected type")
        };
    }
    public static IMetaSource Load(string path) {
        if (new FileInfo(path).Length > 1024 * 1024)
            throw new InvalidDataException("Schema projection exceeds one MiB");
        var options = new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
            UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow };
        options.Converters.Add(new JsonStringEnumConverter(JsonNamingPolicy.SnakeCaseLower));
        var projection = JsonSerializer.Deserialize<Projection>(File.ReadAllText(path), options)
            ?? throw new InvalidDataException("Empty schema projection");
        if (projection.Schema.Count is < 1 or > 128) throw new InvalidDataException("Invalid type budget");
        var types = new List<IMetaType>();
        foreach (var type in projection.Schema) {
            if (type.Fields.Count is < 1 or > 128) throw new InvalidDataException("Invalid field budget");
            var members = new List<IMetaMember>();
            foreach (var field in type.Fields) {
                var attributes = Tags(field.Tags).ToList();
                attributes.Add(Attribute("Metagen.MetaFieldAttribute", field.Id, field.Name));
                if (field.HasDefault) attributes.Add(Attribute("System.ComponentModel.DefaultValueAttribute", Literal(field.Default, field.Type)));
                if (field.Minimum.HasValue || field.Maximum.HasValue)
                    attributes.Add(Attribute("Metagen.PortableData.MetaRangeAttribute", field.Minimum, field.Maximum));
                members.Add(new MetaMember(field.Name, "field", field.Type.Kind.ToString(), PortableSchema.Project(field.Type),
                    Array.AsReadOnly(attributes.ToArray())));
            }
            var typeAttributes = Tags(type.Tags).ToList();
            typeAttributes.Add(Attribute("Metagen.MetaSchemaAttribute", type.Id, type.Version, type.Name));
            types.Add(new MetaType(type.Name, type.Id, "record", null,
                Array.AsReadOnly(typeAttributes.ToArray()), Array.AsReadOnly(members.ToArray())));
        }
        return new MetaSource(projection.Source, Array.AsReadOnly(types.ToArray()));
    }
}
