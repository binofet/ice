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
