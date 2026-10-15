namespace Metagen;

/// Optional convention for generators. Consumers may instead use IMetaSource directly.
public interface IMetaGenerator {
    string Id { get; }
    GenerationResult Generate(IMetaSource source);
}
public enum DiagnosticSeverity { Information, Warning, Error }
public sealed record MetaDiagnostic(DiagnosticSeverity Severity, string Code, string Message,
    string? SourceType = null, string? SourceMember = null);
public sealed record GeneratedArtifact(string RelativePath, string MediaType, ReadOnlyMemory<byte> Content);
public sealed record GenerationResult(IReadOnlyList<GeneratedArtifact> Artifacts,
    IReadOnlyList<MetaDiagnostic> Diagnostics);

/// Combine independent generators without prescribing files, schema semantics, or attribute types.
public static class Generation {
    public static GenerationResult Run(IMetaSource source, IEnumerable<IMetaGenerator> generators) {
        var ids = new HashSet<string>(StringComparer.Ordinal);
        var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var artifacts = new List<GeneratedArtifact>();
        var diagnostics = new List<MetaDiagnostic>();
        foreach (var generator in generators) {
            if (string.IsNullOrWhiteSpace(generator.Id) || !ids.Add(generator.Id))
                throw new InvalidDataException("Generator identities must be nonempty and unique");
            var output = generator.Generate(source);
            diagnostics.AddRange(output.Diagnostics);
            foreach (var artifact in output.Artifacts) {
                // Check portable paths before any caller writes artifacts. No partial disk writes here.
                var parts = artifact.RelativePath.Split('/');
                if (parts.Any(p => p.Length == 0 || p is "." or ".." ||
                    p.IndexOfAny(new[] { '\\', ':', '\0' }) >= 0) || !paths.Add(artifact.RelativePath))
                    throw new InvalidDataException("Invalid or conflicting generated artifact path");
                artifacts.Add(artifact);
            }
        }
        // Diagnostics are data; callers choose their warning policy. Errors never publish artifacts.
        return new GenerationResult(diagnostics.Any(d => d.Severity == DiagnosticSeverity.Error)
            ? Array.Empty<GeneratedArtifact>() : artifacts.AsReadOnly(), diagnostics.AsReadOnly());
    }
}
