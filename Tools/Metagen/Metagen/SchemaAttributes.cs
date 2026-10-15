namespace Metagen;

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Enum)]
public sealed class MetaSchemaAttribute(string id, uint version, string name) : Attribute {
    public string Id { get; } = id;
    public uint Version { get; } = version;
    public string Name { get; } = name;
}
[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
public sealed class MetaFieldAttribute(uint id, string name) : Attribute {
    public uint Id { get; } = id;
    public string Name { get; } = name;
}
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Field |
                AttributeTargets.Property, AllowMultiple = true)]
public sealed class MetaTagAttribute(string name, string value) : Attribute {
    public string Name { get; } = name;
    public string Value { get; } = value;
}
