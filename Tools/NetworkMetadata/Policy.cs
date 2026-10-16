using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Metagen;
namespace Ice.NetworkMetadata;

public sealed record NetworkField(uint Id, string Name, string LogicalType, double? Minimum,
    double? Maximum, string Codec, uint CodecVersion, string Unit, string Interpolation, string Reference);
public sealed record NetworkType(string Id, uint Version, string Name, string Writer, string Audience,
    string Delivery, string Conflict, string Direction, string Capability, string Deduplication,
    IReadOnlyList<NetworkField> Fields, string Fingerprint);

public static class NetworkPolicy {
    public const string Profile = "ice.network.scalar/1";
    public static readonly JsonSerializerOptions Json = new() {
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower, WriteIndented = true
    };
    private static readonly HashSet<string> TypeKeys = new(StringComparer.Ordinal) {
        "writer", "audience", "delivery", "conflict", "direction", "capability", "deduplication"
    };
    private static readonly HashSet<string> FieldKeys = new(StringComparer.Ordinal) {
        "codec", "codec_version", "unit", "interpolation", "reference"
    };
    private static string Text(object? value) => value is string text && text.Length <= 1024 && !text.Any(char.IsControl)
        ? text : throw new InvalidDataException("Expected bounded control-free metadata text");
    private static uint Number(object? value) => Convert.ToUInt32(value, CultureInfo.InvariantCulture);
    private static IMetaAttribute? One(IReadOnlyList<IMetaAttribute> attributes, string name) {
        var found = attributes.Where(a => a.TypeName == name).ToArray();
        if (found.Length > 1) throw new InvalidDataException($"Duplicate attribute {name}");
        return found.SingleOrDefault();
    }
    private static void Put(Dictionary<string, string> values, string key, string value) {
        // Even equal duplicates are rejected: source order cannot select policy.
        if (!values.TryAdd(key, value)) throw new InvalidDataException($"Conflicting/duplicate network policy {key}");
    }
    private static Dictionary<string, string> Read(IReadOnlyList<IMetaAttribute> attributes, bool field) {
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var attribute in attributes) {
            if (attribute.TypeName == "Metagen.MetaTagAttribute") {
                var key = Text(attribute.Arguments[0].Value);
                if (!key.StartsWith("ice.net.", StringComparison.Ordinal)) continue;
                Put(values, key[8..], Text(attribute.Arguments[1].Value));
            } else if (attribute.TypeName.StartsWith("Ice.NetworkMetadata.", StringComparison.Ordinal)) {
                var allowedNamed = attribute.TypeName switch {
                    "Ice.NetworkMetadata.ReplicatedAttribute" => new[] { "Audience", "Delivery", "Conflict" },
                    "Ice.NetworkMetadata.NetworkEventAttribute" => new[] { "RequiredCapability", "Delivery", "Deduplication" },
                    "Ice.NetworkMetadata.WireCodecAttribute" => new[] { "Version", "Unit", "Interpolation" },
                    "Ice.NetworkMetadata.EntityReferenceAttribute" => Array.Empty<string>(),
                    _ => throw new InvalidDataException($"Unsupported network attribute {attribute.TypeName}")
                };
                if (attribute.Arguments.Count != 1 || attribute.NamedArguments.Keys.Any(k => !allowedNamed.Contains(k, StringComparer.Ordinal)))
                    throw new InvalidDataException("Invalid network attribute arguments");
                string Arg(int index) => Text(attribute.Arguments[index].Value);
                void Named(string source, string target) {
                    if (attribute.NamedArguments.TryGetValue(source, out var value))
                        Put(values, target, Text(value.Value));
                }
                switch (attribute.TypeName) {
                    case "Ice.NetworkMetadata.ReplicatedAttribute" when !field:
                        Put(values, "writer", Arg(0)); Named("Audience", "audience");
                        Named("Delivery", "delivery"); Named("Conflict", "conflict"); break;
                    case "Ice.NetworkMetadata.NetworkEventAttribute" when !field:
                        Put(values, "direction", Arg(0)); Named("RequiredCapability", "capability");
                        Named("Delivery", "delivery"); Named("Deduplication", "deduplication"); break;
                    case "Ice.NetworkMetadata.WireCodecAttribute" when field:
                        Put(values, "codec", Arg(0));
                        Put(values, "codec_version", attribute.NamedArguments.TryGetValue("Version", out var version)
                            ? Number(version.Value).ToString(CultureInfo.InvariantCulture) : "1");
                        Named("Unit", "unit"); Named("Interpolation", "interpolation"); break;
                    case "Ice.NetworkMetadata.EntityReferenceAttribute" when field:
                        Put(values, "reference", Arg(0)); break;
                    default: throw new InvalidDataException($"Unsupported network attribute/scope {attribute.TypeName}");
                }
            }
        }
        if (values.Any(v => !(field ? FieldKeys : TypeKeys).Contains(v.Key) || v.Value.Length is 0 or > 128))
            throw new InvalidDataException("Unknown network key, wrong scope, or invalid value");
        return values;
    }
    private static string Need(Dictionary<string, string> values, string key) =>
        values.TryGetValue(key, out var value) ? value : throw new InvalidDataException($"Missing network policy {key}");
    private static void Exact(string actual, params string[] allowed) {
        if (!allowed.Contains(actual, StringComparer.Ordinal)) throw new InvalidDataException($"Unsupported policy value {actual}");
    }
    public static string Fingerprint(NetworkType type) {
        // Deliberately omit display/member names, provider identity, source paths and CLR names.
        var meaning = new { profile = Profile, type.Id, type.Version, type.Writer, type.Audience,
            type.Delivery, type.Conflict, type.Direction, type.Capability, type.Deduplication,
            fields = type.Fields.Select(f => new { f.Id, f.LogicalType, f.Minimum, f.Maximum,
                f.Codec, f.CodecVersion, f.Unit, f.Interpolation, f.Reference }) };
        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(JsonSerializer.Serialize(meaning, Json))))
            .ToLowerInvariant();
    }
    public static IReadOnlyList<NetworkType> Read(IMetaSource source) {
        // Consume raw metadata. PortableSchema.Read would discard arbitrary custom attributes.
        if (Read(source.Attributes, false).Count != 0)
            throw new InvalidDataException("Assembly-wide network defaults are not supported by this proof");
        var result = new List<NetworkType>();
        foreach (var type in source.Types) {
            var schema = One(type.Attributes, "Metagen.MetaSchemaAttribute");
            var policy = Read(type.Attributes, false);
            if (schema is null) {
                if (policy.Count != 0 || type.Members.Any(m => Read(m.Attributes, true).Count != 0))
                    throw new InvalidDataException("Network type has no stable schema identity");
                continue;
            }
            if (policy.Count == 0) {
                if (type.Members.Any(m => Read(m.Attributes, true).Count != 0))
                    throw new InvalidDataException("Network field requires explicit component/event policy");
                continue;
            }
            var id = Text(schema.Arguments[0].Value);
            var version = Number(schema.Arguments[1].Value);
            if (type.Kind is not ("class" or "struct" or "record") ||
                type.BaseType is not (null or "System.Object" or "System.ValueType"))
                throw new InvalidDataException("Network record inheritance requires a future explicit projection");
            if (!Regex.IsMatch(id, "^[a-z][a-z0-9_.]{0,127}$") || version == 0)
                throw new InvalidDataException("Invalid stable type ID/version");
            var direction = policy.GetValueOrDefault("direction", "");
            var isEvent = direction.Length != 0;
            string writer = "", audience = "", conflict = "", capability = "", deduplication = "";
            var delivery = Need(policy, "delivery");
            if (isEvent) {
                Exact(direction, "client_to_authority"); Exact(delivery, "reliable_event");
                capability = Need(policy, "capability"); Exact(capability, "weapon.fire");
                deduplication = Need(policy, "deduplication"); Exact(deduplication, "sequence_window_epoch");
                if (policy.ContainsKey("writer") || policy.ContainsKey("audience") || policy.ContainsKey("conflict"))
                    throw new InvalidDataException("Event cannot declare component writer/audience/conflict");
            } else {
                writer = Need(policy, "writer"); Exact(writer, "authority");
                audience = Need(policy, "audience"); Exact(audience, "relevant");
                conflict = Need(policy, "conflict"); Exact(conflict, "whole_field_cas");
                Exact(delivery, "snapshot", "reliable_change");
                if (policy.ContainsKey("capability") || policy.ContainsKey("deduplication"))
                    throw new InvalidDataException("Component cannot declare event policy");
            }
            var fields = new List<NetworkField>();
            foreach (var member in type.Members) {
                var field = One(member.Attributes, "Metagen.MetaFieldAttribute");
                var values = Read(member.Attributes, true);
                if (field is null) {
                    if (values.Count != 0) throw new InvalidDataException("Network member has no stable field ID");
                    continue;
                }
                if (member.Type.Arguments.Count != 0 || member.Type.ArrayRank != 0 || member.Type.IsEnum || member.Type.NullableAnnotation == true)
                    throw new InvalidDataException("Rich/nullable network fields require a future explicit profile");
                var fieldId = Number(field.Arguments[0].Value);
                if (fieldId == 0) throw new InvalidDataException("Zero field ID");
                var codec = Need(values, "codec");
                if (Need(values, "codec_version") != "1") throw new InvalidDataException("Unsupported codec version");
                var unit = Need(values, "unit"); var interpolation = Need(values, "interpolation");
                var reference = values.GetValueOrDefault("reference", "");
                var range = One(member.Attributes, "Metagen.PortableData.MetaRangeAttribute");
                double? minimum = range?.Arguments[0].Value is { } lo ? Convert.ToDouble(lo, CultureInfo.InvariantCulture) : null;
                double? maximum = range?.Arguments[1].Value is { } hi ? Convert.ToDouble(hi, CultureInfo.InvariantCulture) : null;
                var logical = member.Type.Name switch {
                    "System.Single" => "float32", "System.UInt32" => "uint32",
                    "System.String" when reference.Length != 0 => "entity_reference",
                    _ => throw new InvalidDataException("Unsupported network logical type")
                };
                if (logical == "float32") {
                    minimum = minimum.HasValue ? (double)(float)minimum.Value : null;
                    maximum = maximum.HasValue ? (double)(float)maximum.Value : null;
                }
                if (logical == "entity_reference") {
                    Exact(codec, "entity_v1"); Exact(unit, "entity"); Exact(interpolation, "none");
                    Exact(reference, "owned_weapon");
                    if (!isEvent || range is not null) throw new InvalidDataException("Invalid reference policy");
                } else {
                    if (reference.Length != 0 || !minimum.HasValue || !maximum.HasValue ||
                        !double.IsFinite(minimum.Value) || !double.IsFinite(maximum.Value) || minimum > maximum)
                        throw new InvalidDataException("Finite numeric ranges are required");
                    if (logical == "uint32" && (minimum < 0 || maximum > uint.MaxValue ||
                        Math.Truncate(minimum.Value) != minimum || Math.Truncate(maximum.Value) != maximum))
                        throw new InvalidDataException("Invalid uint32 range");
                    switch (codec) {
                        case "signed16_cm" when logical == "float32" && !isEvent:
                            Exact(unit, "m"); Exact(interpolation, "linear");
                            // Bound after float32 normalization, then verify round-even code range.
                            if (Math.Round((double)(float)minimum.Value * 100, MidpointRounding.ToEven) < short.MinValue ||
                                Math.Round((double)(float)maximum.Value * 100, MidpointRounding.ToEven) > short.MaxValue)
                                throw new InvalidDataException("Range exceeds signed16_cm after float32 normalization");
                            if ((float)Math.Round(minimum.Value * 100, MidpointRounding.ToEven) / 100f != (float)minimum.Value ||
                                (float)Math.Round(maximum.Value * 100, MidpointRounding.ToEven) / 100f != (float)maximum.Value)
                                throw new InvalidDataException("This scalar profile requires centimeter-grid range endpoints");
                            break;
                        case "uint16_exact" when logical == "uint32" && !isEvent:
                            Exact(unit, "points"); Exact(interpolation, "none");
                            if (maximum > ushort.MaxValue || delivery != "reliable_change") throw new InvalidDataException("Invalid health codec/delivery"); break;
                        case "uint32_exact" when logical == "uint32" && isEvent:
                            Exact(unit, "sequence"); Exact(interpolation, "none"); break;
                        case "float32_finite" when logical == "float32" && isEvent:
                            Exact(unit, "unit_vector"); Exact(interpolation, "none");
                            if (minimum < -1 || maximum > 1) throw new InvalidDataException("Invalid aim range"); break;
                        default: throw new InvalidDataException("Codec/logical type/domain mismatch");
                    }
                }
                fields.Add(new NetworkField(fieldId, Text(field.Arguments[1].Value), logical, minimum, maximum,
                    codec, 1, unit, interpolation, reference));
            }
            if (fields.Count is < 1 or > 128 || fields.Select(f => f.Id).Distinct().Count() != fields.Count)
                throw new InvalidDataException("Duplicate field identity or field budget exceeded");
            fields = fields.OrderBy(f => f.Id).ToList();
            // Explicit first event adapter contract; do not silently reinterpret another event shape.
            if (isEvent && (fields.Count != 5 || !fields.Select(f => f.Id).SequenceEqual(new uint[] {1, 2, 3, 4, 5}) ||
                fields[0].LogicalType != "entity_reference" || fields[1].Codec != "uint32_exact" ||
                fields.Skip(2).Any(f => f.Codec != "float32_finite")))
                throw new InvalidDataException("Unsupported event adapter shape");
            var item = new NetworkType(id, version, Text(schema.Arguments[2].Value), writer, audience, delivery,
                conflict, direction, capability, deduplication, fields, "");
            result.Add(item with { Fingerprint = Fingerprint(item) });
        }
        if (result.Count is < 1 or > 128 || result.Select(t => t.Id).Distinct(StringComparer.Ordinal).Count() != result.Count)
            throw new InvalidDataException("Duplicate schema identity or type budget exceeded");
        return result.OrderBy(t => t.Id, StringComparer.Ordinal).ToArray();
    }
}
