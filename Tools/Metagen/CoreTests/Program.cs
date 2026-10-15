using Metagen;
using System.Text;

var source = ReflectionMetaSource.Load(typeof(UserModel).Assembly.Location);
var model = source.Types.Single(t => t.FullName == typeof(UserModel).FullName);
var metadata = model.Attributes.Single(a => a.TypeName == typeof(UserHintAttribute).FullName);
if (!Equals(metadata.Arguments[0].Value, "User-defined meaning") ||
    !Equals(metadata.Arguments[1].Value, "System.Collections.Generic.List`1") ||
    metadata.Arguments[1].TypeReference is not { IsGenericDefinition: true, Arguments.Count: 1 } ||
    metadata.Arguments[2].Value is not IReadOnlyList<IMetaValue> array || array.Count != 2 ||
    !Equals(array[1].Value, 9) || !Equals(metadata.NamedArguments["Level"].Value, 7))
    throw new Exception("Arbitrary user attribute values were lost or their constructors were invoked");
var property = model.Members.Single(m => m.Name == "Nested");
if (property.Type.Name != "System.Collections.Generic.List`1" || property.Type.NullableAnnotation != true ||
    property.Type.Arguments[0].Name != "System.Collections.Generic.Dictionary`2" ||
    property.Type.Arguments[0].Arguments[1].Arguments[0].Name != "System.Nullable`1")
    throw new Exception("Core generic/nullable type-use metadata was flattened");
var good = new ProbeGenerator("one", "demo/types.txt");
var result = Generation.Run(source, new[] { good });
if (result.Artifacts.Count != 1) throw new Exception("Generator composition failed");
foreach (var path in new[] { "../escape", "/absolute", "C:/drive", "a\\b", "a//b", "./relative", "bad\0path" })
    Reject(() => Generation.Run(source, new[] { new ProbeGenerator("bad-path", path) }));
Reject(() => Generation.Run(source, new[] { good, good }));
Reject(() => Generation.Run(source, new[] { good, new ProbeGenerator("two", "DEMO/TYPES.TXT") }));
var failed = Generation.Run(source, new[] { good, new ProbeGenerator("error", "other.txt", true) });
if (failed.Artifacts.Count != 0 || failed.Diagnostics.Single().Severity != DiagnosticSeverity.Error)
    throw new Exception("Failed generation published partial artifacts");
Console.WriteLine("Standalone core: arbitrary attributes, type tokens/arrays/enums, nested type uses, independent generators, conflict/error handling passed");

static void Reject(Action action) {
    try { action(); } catch (InvalidDataException) { return; }
    throw new Exception("Invalid generator request was accepted");
}

public enum HintLevel { Normal = 0, Detailed = 7 }
[AttributeUsage(AttributeTargets.Class)]
public sealed class UserHintAttribute : Attribute {
    public HintLevel Level { get; set; }
    // Every attribute construction would fail. Reflection must inspect metadata, never call this.
    public UserHintAttribute(string text, Type container, int[] values) {
        throw new InvalidOperationException("Attribute executed: " + text + container.Name + values.Length);
    }
}
[UserHint("User-defined meaning", typeof(List<>), new[] { 3, 9 }, Level = HintLevel.Detailed)]
public sealed class UserModel {
    public List<Dictionary<string, List<int?>>>? Nested { get; set; }
}
internal sealed class ProbeGenerator(string id, string path, bool error = false) : IMetaGenerator {
    public string Id => id;
    public GenerationResult Generate(IMetaSource source) => new(
        new[] { new GeneratedArtifact(path, "text/plain", Encoding.UTF8.GetBytes(source.Identity)) },
        error ? new[] { new MetaDiagnostic(DiagnosticSeverity.Error, "EXAMPLE", "Intentional diagnostic") }
              : Array.Empty<MetaDiagnostic>());
}
