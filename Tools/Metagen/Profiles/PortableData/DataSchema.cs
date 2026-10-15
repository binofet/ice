using Metagen;
namespace Metagen.PortableData;

public sealed record DataSymbol(uint Id, string Name, int Value);
public sealed record DataField(uint Id, string Name, PortableTypeRef Type, string Storage,
    string Merge, string Key, bool HasDefault, object? Default);
public sealed record DataType(string Id, uint Version, string Name, string Kind, bool Prototype,
    IReadOnlyList<DataField> Fields, IReadOnlyList<DataSymbol> Symbols);

// A richer example consumer. None of these policies are required by the core SDK.
public static class DataSchema {
    private static IMetaAttribute? Find(IEnumerable<IMetaAttribute> attributes, string name) =>
        attributes.SingleOrDefault(a => a.TypeName == name);
    public static IReadOnlyList<DataType> Read(IMetaSource source) {
        var declarations = source.Types.Where(t => Find(t.Attributes, "Metagen.MetaSchemaAttribute") is not null)
            .ToDictionary(t => t.FullName, StringComparer.Ordinal);
        PortableTypeRef Resolve(MetaTypeRef type, int depth = 0) {
            if (depth > 32) throw new InvalidDataException("Type expression nesting exceeds its budget");
            var scalar = PortableSchema.Resolve(type);
            if (scalar.Kind != ScalarKind.Unsupported) return scalar with { Nullable = scalar.Nullable || type.NullableAnnotation == true };
            if (type.Name == "System.Nullable`1" && type.Arguments.Count == 1)
                return Resolve(type.Arguments[0], depth + 1) with { Nullable = true };
            if (declarations.TryGetValue(type.Name, out var named))
                return new PortableTypeRef(named.Kind == "enum" ? ScalarKind.Enum : ScalarKind.Record,
                    type.NullableAnnotation == true, (string)Find(named.Attributes, "Metagen.MetaSchemaAttribute")!.Arguments[0].Value!);
            var kind = type.ArrayRank == 1 ? ScalarKind.Array : type.Name switch {
                "System.Collections.Generic.List`1" => ScalarKind.Sequence,
                "System.Collections.Generic.Dictionary`2" => ScalarKind.Map,
                "System.Collections.Generic.HashSet`1" => ScalarKind.Set,
                _ => throw new InvalidDataException($"Unsupported portable data type: {type.Name}")
            };
            return new PortableTypeRef(kind, type.NullableAnnotation == true,
                Arguments: type.Arguments.Select(a => Resolve(a, depth + 1)).ToArray());
        }
        var result = new List<DataType>();
        foreach (var type in declarations.Values) {
            var schema = Find(type.Attributes, "Metagen.MetaSchemaAttribute")!;
            var fields = new List<DataField>();
            var symbols = new List<DataSymbol>();
            foreach (var member in type.Members) {
                var field = Find(member.Attributes, "Metagen.MetaFieldAttribute");
                if (field is null) continue;
                uint id = Convert.ToUInt32(field.Arguments[0].Value);
                string name = (string)field.Arguments[1].Value!;
                if (type.Kind == "enum") {
                    if (member.Type.EnumUnderlyingType != "System.Int32") throw new InvalidDataException("This enum profile requires int32");
                    symbols.Add(new DataSymbol(id, name, Convert.ToInt32(member.ConstantValue)));
                } else {
                    var reference = Find(member.Attributes, "Metagen.PortableData.MetaReferenceAttribute");
                    var merge = Find(member.Attributes, "Metagen.PortableData.MetaMergeAttribute");
                    var literal = Find(member.Attributes, "System.ComponentModel.DefaultValueAttribute");
                    fields.Add(new DataField(id, name, Resolve(member.Type), (string?)reference?.Arguments[0].Value ?? "inline",
                        (string?)merge?.Arguments[0].Value ?? "replace", (string?)merge?.Arguments[1].Value ?? "",
                        literal is not null, literal?.Arguments[0].Value));
                }
            }
            result.Add(new DataType((string)schema.Arguments[0].Value!, Convert.ToUInt32(schema.Arguments[1].Value),
                (string)schema.Arguments[2].Value!, type.Kind == "enum" ? "enum" : "record",
                Find(type.Attributes, "Metagen.PortableData.MetaPrototypeAttribute") is not null,
                fields.OrderBy(f => f.Id).ToArray(), symbols.OrderBy(s => s.Id).ToArray()));
        }
        return result.OrderBy(t => t.Id, StringComparer.Ordinal).ToArray();
    }
}
