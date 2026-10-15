using System.Text;
using Metagen;
namespace MetagenExamples;

// Deliberately references only the core SDK. No PortableData, ICE, or attribute assembly reference.
public sealed class DocumentationGenerator : IMetaGenerator {
    public string Id => "example.documentation";
    public GenerationResult Generate(IMetaSource source) {
        var text = new StringBuilder();
        foreach (var type in source.Types) {
            var attribute = type.Attributes.SingleOrDefault(a => a.TypeName.EndsWith(".DescriptionOnlyAttribute", StringComparison.Ordinal));
            if (attribute is null) continue;
            text.AppendLine($"{type.FullName}: {attribute.Arguments[0].Value}");
            foreach (var (name, value) in attribute.NamedArguments)
                text.AppendLine($"  {name}: {value.Value}");
        }
        return new GenerationResult(new[] { new GeneratedArtifact("custom-attributes.txt", "text/plain",
            Encoding.UTF8.GetBytes(text.ToString())) }, Array.Empty<MetaDiagnostic>());
    }
}
