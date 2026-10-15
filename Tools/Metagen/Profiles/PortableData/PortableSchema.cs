using System.Globalization;
using Metagen;
namespace Metagen.PortableData;

// This is a consumer of the generic reflection interfaces, not a replacement for
// their open-ended attributes. The first native profile deliberately supports scalars.
public sealed record PortableField(uint Id, string Name, PortableTypeRef Type, bool HasDefault,
    object? Default, double? Minimum, double? Maximum, IReadOnlyDictionary<string, string> Tags);
public sealed record PortableType(string Id, uint Version, string Name,
    IReadOnlyList<PortableField> Fields, IReadOnlyDictionary<string, string> Tags);

public static class PortableSchema {
    public static PortableTypeRef Resolve(MetaTypeRef type) {
        bool nullable = type.Name == "System.Nullable`1" && type.Arguments.Count == 1;
        var value = nullable ? type.Arguments[0] : type;
        var kind = value.Name switch {
            "System.Boolean" => ScalarKind.Boolean,
            "System.Int32" => ScalarKind.Int32,
            "System.UInt32" => ScalarKind.Uint32,
            "System.Single" => ScalarKind.Float32,
            "System.Double" => ScalarKind.Float64,
            "System.String" => ScalarKind.String,
            _ => ScalarKind.Unsupported
        };
        return new PortableTypeRef(kind, nullable);
    }
    public static MetaTypeRef Project(PortableTypeRef type) {
        string name = type.Kind switch {
            ScalarKind.Boolean => "System.Boolean",
            ScalarKind.Int32 => "System.Int32",
            ScalarKind.Uint32 => "System.UInt32",
            ScalarKind.Float32 => "System.Single",
            ScalarKind.Float64 => "System.Double",
            ScalarKind.String => "System.String",
            _ => throw new InvalidDataException("Unsupported projected scalar")
        };
        var value = new MetaTypeRef(name, Array.Empty<MetaTypeRef>());
        return type.Nullable ? new MetaTypeRef("System.Nullable`1", new[] { value }) : value;
    }
    private static IMetaAttribute? Attribute(IEnumerable<IMetaAttribute> attributes, string name) =>
        attributes.SingleOrDefault(a => a.TypeName == name);
    private static IReadOnlyDictionary<string, string> Tags(IEnumerable<IMetaAttribute> attributes) =>
        new SortedDictionary<string, string>(attributes.Where(a => a.TypeName == "Metagen.MetaTagAttribute")
            .ToDictionary(a => (string)a.Arguments[0].Value!, a => (string)a.Arguments[1].Value!, StringComparer.Ordinal), StringComparer.Ordinal);
    public static IReadOnlyList<PortableType> Read(IMetaSource source) {
        var types = new List<PortableType>();
        foreach (var type in source.Types) {
            var schema = Attribute(type.Attributes, "Metagen.MetaSchemaAttribute");
            if (schema is null) continue;
            var fields = new List<PortableField>();
            foreach (var member in type.Members) {
                var field = Attribute(member.Attributes, "Metagen.MetaFieldAttribute");
                if (field is null) continue;
                if (Resolve(member.Type).Kind == ScalarKind.Unsupported)
                    throw new InvalidDataException($"Unsupported portable type: {member.ValueType}");
                var literal = Attribute(member.Attributes, "System.ComponentModel.DefaultValueAttribute");
                var range = Attribute(member.Attributes, "Metagen.PortableData.MetaRangeAttribute");
                fields.Add(new PortableField(Convert.ToUInt32(field.Arguments[0].Value),
                    (string)field.Arguments[1].Value!, Resolve(member.Type), literal is not null,
                    literal?.Arguments[0].Value,
                    range?.Arguments[0].Value is null ? null : Convert.ToDouble(range.Arguments[0].Value, CultureInfo.InvariantCulture),
                    range?.Arguments[1].Value is null ? null : Convert.ToDouble(range.Arguments[1].Value, CultureInfo.InvariantCulture), Tags(member.Attributes)));
            }
            if (fields.Any(f => f.Id == 0) || fields.Select(f => f.Id).Distinct().Count() != fields.Count ||
                fields.Select(f => f.Name).Distinct(StringComparer.Ordinal).Count() != fields.Count)
                throw new InvalidDataException($"Duplicate or zero field identity in {type.FullName}");
            types.Add(new PortableType((string)schema.Arguments[0].Value!, Convert.ToUInt32(schema.Arguments[1].Value),
                (string)schema.Arguments[2].Value!, fields.OrderBy(f => f.Id).ToArray(), Tags(type.Attributes)));
        }
        if (types.Select(t => t.Id).Distinct(StringComparer.Ordinal).Count() != types.Count)
            throw new InvalidDataException("Duplicate schema type identity");
        return types.OrderBy(t => t.Id, StringComparer.Ordinal).ToArray();
    }
}
