using Metagen.PortableData;
using System.Globalization;
using System.Text;
using Metagen;
using PlaygroundDefinitions;

// Separate consumer libraries can implement these interfaces without coupling
// to one another. The core SDK does not prescribe an emitter interface.
static string Describe(IMetaType type) {
    var output = new StringBuilder().AppendLine($"{type.Kind} {type.FullName}");
    foreach (var member in type.Members) output.AppendLine($"  {member.Name}: {member.ValueType}");
    return output.ToString();
}
static string WriteIni(IMetaType type) {
    var output = new StringBuilder().AppendLine($"[{type.Name}]");
    foreach (var member in type.Members) {
        var value = member.Attributes.Single(a => a.TypeName == "System.ComponentModel.DefaultValueAttribute").Arguments[0].Value;
        output.AppendLine($"{member.Name}={Convert.ToString(value, CultureInfo.InvariantCulture)}");
    }
    return output.ToString();
}
var source = ReflectionMetaSource.Load(typeof(PaddleSettings).Assembly.Location);
var settings = source.Types.Single(type => type.FullName == "PlaygroundDefinitions.PaddleSettings");
var description = settings.Attributes.Single(attribute => attribute.TypeName.EndsWith("DescriptionOnlyAttribute", StringComparison.Ordinal));
if (!Equals(description.Arguments[0].Value, "Paddle controls") || !Equals(description.NamedArguments["Category"].Value, "Gameplay"))
    throw new Exception("Unknown user attributes or named arguments were lost");
var text = Describe(settings);
var ini = WriteIni(settings);
if (!text.Contains("MaximumSpeed: System.Single") || !ini.Contains("MaximumSpeed=2.8") || !ini.Contains("PredictRebounds=True"))
    throw new Exception("Independent consumers did not receive the expected schema");
var outputPath = args.Length > 0 ? args[0] : Path.Combine(Path.GetTempPath(), "ice-metagen");
Directory.CreateDirectory(outputPath);
File.WriteAllText(Path.Combine(outputPath, "paddle.txt"), text);
File.WriteAllText(Path.Combine(outputPath, "paddle.ini"), ini);
Console.WriteLine($"Metagen proof passed: definition DLL -> consumption interfaces -> text and INI ({outputPath})");
var portable = PortableSchema.Read(source);
var jsonOptions = new System.Text.Json.JsonSerializerOptions {
    WriteIndented = true,
    PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.SnakeCaseLower
};
jsonOptions.Converters.Add(new System.Text.Json.Serialization.JsonStringEnumConverter(
    System.Text.Json.JsonNamingPolicy.SnakeCaseLower));
File.WriteAllText(Path.Combine(outputPath, "csharp-schema.json"),
    System.Text.Json.JsonSerializer.Serialize(new { source = source.Identity, schema = portable }, jsonOptions));
if (portable.Count != 1 || portable[0].Fields.Count != 6 ||
    portable[0].Fields[4].Type is not { Kind: ScalarKind.Float32, Nullable: true })
    throw new Exception("Portable scalar/nullable schema normalization failed");

if (args.Length > 1) {
    var python = ProjectionMetaSource.Load(args[1]);
    var reflectedPortable = System.Text.Json.JsonSerializer.SerializeToNode(portable, jsonOptions);
    var projectedPortable = System.Text.Json.JsonSerializer.SerializeToNode(PortableSchema.Read(python), jsonOptions);
    if (!System.Text.Json.Nodes.JsonNode.DeepEquals(reflectedPortable, projectedPortable))
        throw new Exception("C# reflection and Python projection disagree through IMetaSource");
    Console.WriteLine("Shared SDK proof passed: both providers -> IMetaSource -> the same PortableSchema consumer");
}

var generated = Generation.Run(source, new IMetaGenerator[] { new MetagenExamples.DocumentationGenerator() });
if (generated.Artifacts.Count != 1 || !Encoding.UTF8.GetString(generated.Artifacts[0].Content.Span).Contains("Paddle controls"))
    throw new Exception("Core-only generator lost custom attribute information");
var mappings = ContainerMappings.Read(source);
if (mappings.Count != 4 || mappings.Single(m => m.Target == TargetProfile.Cpp23).Render("float") != "playground::custom_vector<float>")
    throw new Exception("Assembly-wide generic container mapping failed");
var dataSource = ReflectionMetaSource.Load(typeof(MetagenExamples.Session).Assembly.Location);
var data = DataSchema.Read(dataSource);
File.WriteAllText(Path.Combine(outputPath, "csharp-data.json"), System.Text.Json.JsonSerializer.Serialize(data, jsonOptions));
File.WriteAllText(Path.Combine(outputPath, "container-maps.json"), System.Text.Json.JsonSerializer.Serialize(mappings, jsonOptions));
Console.WriteLine("Core-only custom generator and independent rich data/container consumers passed");
