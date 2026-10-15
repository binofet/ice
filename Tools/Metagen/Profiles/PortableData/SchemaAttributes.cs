using Metagen;
namespace Metagen.PortableData;

[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class MetaRangeAttribute(double minimum, double maximum) : Attribute {
    public double Minimum { get; } = minimum;
    public double Maximum { get; } = maximum;
}
public enum ScalarKind { Boolean, Int32, Uint32, Float32, Float64, String, Unsupported, Record, Enum, Sequence, Map, Set, Array }
public sealed record PortableTypeRef(ScalarKind Kind, bool Nullable,
    [property: System.Text.Json.Serialization.JsonIgnore(Condition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)] string? Id = null,
    [property: System.Text.Json.Serialization.JsonIgnore(Condition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)] IReadOnlyList<PortableTypeRef>? Arguments = null);

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public sealed class MetaPrototypeAttribute : Attribute;

// Applied to an object occurrence, or to a container whose leaf occurrences are objects.
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class MetaReferenceAttribute(string storage) : Attribute {
    public string Storage { get; } = storage;
}
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class MetaMergeAttribute(string policy, string key = "") : Attribute {
    public string Policy { get; } = policy;
    public string Key { get; } = key;
}
