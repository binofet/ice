using System.Text;
using System.Text.Json;
using Ice.NetworkMetadata;
using Ice.NetworkFixtures;
using Metagen;
using Metagen.PortableData;

if (args.Length != 2) throw new ArgumentException("Usage: NetworkMetadata PYTHON_PROJECTION_JSON NEW_OUTPUT_DIRECTORY");
var reflected = ReflectionMetaSource.Load(typeof(Transform).Assembly.Location);
var projected = ProjectionMetaSource.Load(args[0]);
var csharp = NetworkPolicy.Read(reflected);
var python = NetworkPolicy.Read(projected);
if (JsonSerializer.Serialize(csharp, NetworkPolicy.Json) != JsonSerializer.Serialize(python, NetworkPolicy.Json))
    throw new InvalidDataException("C# custom attributes and passive Python tags disagree");
var generator = new NetworkGenerator();
var generated = Generation.Run(reflected, new[] { generator });
var pythonGenerated = Generation.Run(projected, new[] { generator });
if (generated.Diagnostics.Count != 0 || generated.Artifacts.Count != 4 || pythonGenerated.Artifacts.Count != 4)
    throw new InvalidDataException("Expected complete generated artifacts");
for (var i = 0; i < generated.Artifacts.Count; ++i)
    if (!generated.Artifacts[i].Content.Span.SequenceEqual(pythonGenerated.Artifacts[i].Content.Span))
        throw new InvalidDataException("Provider artifacts differ");

MetaAttribute Tag(string key, string value) => new("Metagen.MetaTagAttribute",
    new IMetaValue[] { new MetaValue("System.String", key), new MetaValue("System.String", value) }, new Dictionary<string, IMetaValue>());
IMetaSource Replace(IMetaSource source, Func<IMetaType, IMetaType> map) => new MetaSource(source.Identity,
    source.Types.Select(map).ToArray(), source.Attributes);
IMetaType Change(IMetaType type, IReadOnlyList<IMetaAttribute>? attributes = null, IReadOnlyList<IMetaMember>? members = null) =>
    new MetaType(type.Name, type.FullName, type.Kind, type.BaseType, attributes ?? type.Attributes, members ?? type.Members);
void Rejected(IMetaSource source) {
    var result = Generation.Run(source, new[] { generator });
    if (result.Artifacts.Count != 0 || !result.Diagnostics.Any(d => d.Severity == DiagnosticSeverity.Error))
        throw new Exception("Invalid policy published partial artifacts");
}
var transform = reflected.Types.Single(t => t.FullName.EndsWith(".Transform", StringComparison.Ordinal));
foreach (var (key, value) in new[] {
    ("ice.net.writre", "authority"), ("ice.net.writer", "authority"),
    ("ice.net.direction", "client_to_authority"), ("ice.net.capability", "weapon.fire")
}) Rejected(Replace(reflected, t => t == transform ? Change(t, t.Attributes.Append(Tag(key, value)).ToArray()) : t));
foreach (var (key, value) in new[] { ("ice.net.codec", "signed16_cm"), ("ice.net.whatever", "x"), ("ice.net.writer", "authority") }) {
    Rejected(Replace(reflected, t => t == transform ? Change(t, members: t.Members.Select((m, i) => i == 0 ?
        new MetaMember(m.Name, m.Kind, m.ValueType, m.Type, m.Attributes.Append(Tag(key, value)).ToArray()) : m).ToArray()) : t));
}
// Reordered source traversal cannot change policy/artifact order.
var reversed = Replace(reflected, t => Change(t, t.Attributes.Reverse().ToArray(), t.Members.Reverse().ToArray()));
reversed = new MetaSource(reversed.Identity, reversed.Types.Reverse().ToArray(), reversed.Attributes);
if (JsonSerializer.Serialize(NetworkPolicy.Read(reversed), NetworkPolicy.Json) != JsonSerializer.Serialize(csharp, NetworkPolicy.Json))
    throw new Exception("Policy depends on declaration/attribute order");
// Presentation renaming must not change wire fingerprints or field order.
var renamed = Replace(reflected, t => Change(t, t.Attributes.Select(a => a.TypeName == "Metagen.MetaSchemaAttribute" ?
    new MetaAttribute(a.TypeName, a.Arguments.Take(2).Append(new MetaValue("System.String", "renamed")).ToArray(), a.NamedArguments) : a).ToArray(),
    t.Members.Select(m => new MetaMember("renamed_" + m.Name, m.Kind, m.ValueType, m.Type, m.Attributes.Select(a =>
        a.TypeName == "Metagen.MetaFieldAttribute" ? new MetaAttribute(a.TypeName,
            new[] { a.Arguments[0], new MetaValue("System.String", "renamed_" + m.Name) }, a.NamedArguments) : a).ToArray())).ToArray()));
if (!NetworkPolicy.Read(renamed).Select(t => t.Fingerprint).SequenceEqual(csharp.Select(t => t.Fingerprint)))
    throw new Exception("Presentation rename changed wire meaning");
var duplicated = new MetaSource(reflected.Identity, reflected.Types.Append(transform).ToArray());
Rejected(duplicated);

// Python's generic tag provider also exercises policy-value rejection, independently of
// the duplicate typed-attribute/tag conflict checks above.
IMetaSource MutateTag(string typeId, uint? fieldId, string key, string value) => Replace(projected, t => {
    if (t.FullName != typeId) return t;
    IReadOnlyList<IMetaAttribute> Changed(IReadOnlyList<IMetaAttribute> attributes) => attributes.Select(a =>
        a.TypeName == "Metagen.MetaTagAttribute" && Equals(a.Arguments[0].Value, key) ? Tag(key, value) : a).ToArray();
    return fieldId is null ? Change(t, Changed(t.Attributes)) : Change(t, members: t.Members.Select(m =>
        Equals(Convert.ToUInt32(m.Attributes.Single(a => a.TypeName == "Metagen.MetaFieldAttribute").Arguments[0].Value), fieldId.Value)
            ? new MetaMember(m.Name, m.Kind, m.ValueType, m.Type, Changed(m.Attributes)) : m).ToArray());
});
foreach (var (typeId, fieldId, key, value) in new (string, uint?, string, string)[] {
    ("demo.transform", null, "ice.net.writer", "client"),
    ("demo.transform", null, "ice.net.audience", "anyone"),
    ("demo.transform", null, "ice.net.delivery", "unreliable_event"),
    ("demo.transform", 1, "ice.net.codec", "native_struct_dump"),
    ("demo.transform", 1, "ice.net.codec_version", "2"),
    ("demo.transform", 1, "ice.net.unit", "cm"),
    ("demo.health", 1, "ice.net.interpolation", "linear"),
    ("demo.fire_event", null, "ice.net.capability", "root"),
    ("demo.fire_event", null, "ice.net.deduplication", "none"),
    ("demo.fire_event", 1, "ice.net.reference", "any_object"),
    ("demo.fire_event", 3, "ice.net.codec", "signed16_cm")
}) Rejected(MutateTag(typeId, fieldId, key, value));

foreach (var badRange in new[] { new[] {-400d, 400d}, new[] {double.NaN, 300d}, new[] {1d, -1d}, new[] {.125d, 1d} }) {
    Rejected(Replace(projected, t => t.FullName == "demo.transform" ? Change(t, members: t.Members.Select((m, i) => i == 0
        ? new MetaMember(m.Name, m.Kind, m.ValueType, m.Type, m.Attributes.Select(a => a.TypeName == "Metagen.PortableData.MetaRangeAttribute"
            ? new MetaAttribute(a.TypeName, badRange.Select(v => (IMetaValue)new MetaValue("System.Double", v)).ToArray(), a.NamedArguments) : a).ToArray()) : m).ToArray()) : t));
}
Rejected(Replace(projected, t => t.FullName == "demo.transform" ? Change(t, members: t.Members.Append(t.Members[0]).ToArray()) : t));
Rejected(Replace(projected, t => t.FullName == "demo.transform" ? Change(t, members: t.Members.Select((m, i) => i == 0
    ? new MetaMember(m.Name, m.Kind, m.ValueType, m.Type, m.Attributes.Select(a => a.TypeName == "Metagen.MetaFieldAttribute"
        ? new MetaAttribute(a.TypeName, new IMetaValue[] {new MetaValue("System.UInt32", 0u), a.Arguments[1]}, a.NamedArguments) : a).ToArray()) : m).ToArray()) : t));
Rejected(Replace(projected, t => t.FullName == "demo.transform" ? Change(t,
    t.Attributes.Where(a => a.TypeName != "Metagen.MetaTagAttribute").ToArray()) : t));
Rejected(Replace(projected, t => t.FullName == "demo.transform" ? Change(t, t.Attributes.Select(a => a.TypeName == "Metagen.MetaSchemaAttribute"
    ? new MetaAttribute(a.TypeName, new IMetaValue[] {a.Arguments[0], new MetaValue("System.UInt32", 0u), a.Arguments[2]}, a.NamedArguments) : a).ToArray()) : t));

var changedRange = Replace(projected, t => t.FullName == "demo.transform" ? Change(t, members: t.Members.Select((m, i) => i == 0
    ? new MetaMember(m.Name, m.Kind, m.ValueType, m.Type, m.Attributes.Select(a => a.TypeName == "Metagen.PortableData.MetaRangeAttribute"
        ? new MetaAttribute(a.TypeName, new IMetaValue[] {new MetaValue("System.Double", -200d), new MetaValue("System.Double", 200d)}, a.NamedArguments) : a).ToArray()) : m).ToArray()) : t);
if (NetworkPolicy.Read(changedRange).Single(t => t.Id == "demo.transform").Fingerprint == csharp.Single(t => t.Id == "demo.transform").Fingerprint)
    throw new Exception("Semantic range change did not change the negotiated wire fingerprint");

var destination = Path.GetFullPath(args[1]);
if (Directory.Exists(destination) || File.Exists(destination)) throw new IOException("Output must not already exist");
Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
var staging = destination + ".staging-" + Guid.NewGuid().ToString("N");
try {
    Directory.CreateDirectory(staging);
    foreach (var artifact in generated.Artifacts) File.WriteAllBytes(Path.Combine(staging, artifact.RelativePath), artifact.Content.ToArray());
    File.WriteAllText(Path.Combine(staging, "providers.json"), JsonSerializer.Serialize(new {
        csharp = reflected.Identity, python = projected.Identity, profile = NetworkPolicy.Profile
    }, NetworkPolicy.Json) + "\n", new UTF8Encoding(false));
    Directory.Move(staging, destination);
} finally { if (Directory.Exists(staging)) Directory.Delete(staging, true); }
Console.WriteLine("Network metadata passed: raw custom attributes == passive Python tags; deterministic generation, rename stability and rejected-policy atomicity");
