using System.Globalization;
using System.Text.RegularExpressions;
using Metagen;
namespace Metagen.PortableData;

public enum ContainerShape { Sequence, Set, Map }
public enum TargetProfile { Cpp20, Cpp23, Rust2021 }

// These attributes describe generated storage, never persistent data identity.
// CustomAttributeData consumes the Type token without constructing this attribute.
[AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
public sealed class ContainerMapAttribute(Type source, ContainerShape shape, TargetProfile target,
    string include, string typeTemplate, string adapter) : Attribute {
    public Type Source { get; } = source;
    public ContainerShape Shape { get; } = shape;
    public TargetProfile Target { get; } = target;
    public string Include { get; } = include;
    public string TypeTemplate { get; } = typeTemplate;
    public string Adapter { get; } = adapter;
}

public sealed record ContainerMapping(string Source, ContainerShape Shape, TargetProfile Target,
    string Include, string TypeTemplate, string Adapter) {
    public int Arity => Shape == ContainerShape.Map ? 2 : 1;
    public string Render(params string[] arguments) {
        if (arguments.Length != Arity) throw new InvalidDataException("Container type argument arity mismatch");
        return Regex.Replace(TypeTemplate, @"\{([0-9]+)\}", match =>
            arguments[int.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture)]);
    }
}

// Independent consumer: emitters may use the mapping, inspect it, or ignore it.
public static class ContainerMappings {
    public static IReadOnlyList<ContainerMapping> Read(IMetaSource source) {
        var result = new List<ContainerMapping>();
        foreach (var attribute in source.Attributes.Where(a => a.TypeName == "Metagen.PortableData.ContainerMapAttribute")) {
            var args = attribute.Arguments;
            if (args.Count != 6 || args[0].Value is not string sourceType || args[1].Value is not int shape ||
                args[2].Value is not int target || args[3].Value is not string include ||
                args[4].Value is not string template || args[5].Value is not string adapter)
                throw new InvalidDataException("Malformed container mapping");
            var mapping = new ContainerMapping(sourceType, (ContainerShape)shape, (TargetProfile)target,
                include, template, adapter);
            var token = args[0].TypeReference;
            if (!Enum.IsDefined(mapping.Shape) || !Enum.IsDefined(mapping.Target) ||
                (token is null ? !mapping.Source.EndsWith("`" + mapping.Arity, StringComparison.Ordinal)
                               : !token.IsGenericDefinition || token.Arguments.Count != mapping.Arity) ||
                !Regex.IsMatch(mapping.Include, @"\A[a-zA-Z0-9_./-]+\z") || mapping.Include.Contains("..") ||
                mapping.Include.StartsWith('/') || !Regex.IsMatch(mapping.Adapter, @"\A[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*\z"))
                throw new InvalidDataException("Invalid container mapping identity, arity, include, or adapter");
            ValidateTemplate(mapping);
            if (result.Any(m => m.Source == mapping.Source && m.Target == mapping.Target))
                throw new InvalidDataException("Ambiguous container mapping for source and target profile");
            result.Add(mapping);
        }
        return result.OrderBy(m => m.Source, StringComparer.Ordinal).ThenBy(m => m.Target).ToArray();
    }
    private static void ValidateTemplate(ContainerMapping mapping) {
        if (mapping.TypeTemplate.Length > 1024) throw new InvalidDataException("Container template exceeds its budget");
        // Deliberately a type-expression grammar, not string.Format or executable source.
        var tokens = Regex.Matches(mapping.TypeTemplate, @"[a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*|\{[0-9]+\}|[<>,]");
        if (string.Concat(tokens.Select(t => t.Value)) != Regex.Replace(mapping.TypeTemplate, @"\s", ""))
            throw new InvalidDataException("Invalid tokens in container type template");
        int position = 0;
        var used = new HashSet<int>();
        void Parse(int depth = 0) {
            if (depth > 16) throw new InvalidDataException("Container template nesting exceeds its budget");
            if (position >= tokens.Count) throw new InvalidDataException("Incomplete type template");
            var token = tokens[position++].Value;
            if (token.StartsWith('{')) {
                if (!int.TryParse(token[1..^1], NumberStyles.None, CultureInfo.InvariantCulture, out int index) || index >= mapping.Arity)
                    throw new InvalidDataException("Type placeholder out of range");
                used.Add(index);
                return;
            }
            if (!char.IsLetter(token[0]) && token[0] != '_') throw new InvalidDataException("Expected a type name");
            if (position == tokens.Count || tokens[position].Value != "<") return;
            ++position;
            Parse(depth + 1);
            while (position < tokens.Count && tokens[position].Value == ",") { ++position; Parse(depth + 1); }
            if (position == tokens.Count || tokens[position++].Value != ">") throw new InvalidDataException("Unbalanced type template");
        }
        Parse();
        if (position != tokens.Count || used.Count != mapping.Arity)
            throw new InvalidDataException("Container template must use every type parameter");
    }
}
