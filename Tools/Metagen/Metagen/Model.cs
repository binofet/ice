namespace Metagen;

// Consumers depend on this model, not Assembly/Type/MemberInfo.
public interface IMetaSource {
    string Identity { get; }
    IReadOnlyList<IMetaType> Types { get; }
}
public interface IMetaType {
    string Name { get; }
    string FullName { get; }
    string Kind { get; }
    string? BaseType { get; }
    IReadOnlyList<IMetaAttribute> Attributes { get; }
    IReadOnlyList<IMetaMember> Members { get; }
}
public interface IMetaMember {
    string Name { get; }
    string Kind { get; }
    string ValueType { get; }
    IReadOnlyList<IMetaAttribute> Attributes { get; }
}
public interface IMetaAttribute {
    string TypeName { get; }
    IReadOnlyList<IMetaValue> Arguments { get; }
    IReadOnlyDictionary<string, IMetaValue> NamedArguments { get; }
}
public interface IMetaValue {
    string TypeName { get; }
    // Primitives, strings, type names, or a read-only list of IMetaValue.
    object? Value { get; }
}
internal sealed record MetaSource(string Identity, IReadOnlyList<IMetaType> Types) : IMetaSource;
internal sealed record MetaType(string Name, string FullName, string Kind, string? BaseType,
    IReadOnlyList<IMetaAttribute> Attributes, IReadOnlyList<IMetaMember> Members) : IMetaType;
internal sealed record MetaMember(string Name, string Kind, string ValueType,
    IReadOnlyList<IMetaAttribute> Attributes) : IMetaMember;
internal sealed record MetaAttribute(string TypeName, IReadOnlyList<IMetaValue> Arguments,
    IReadOnlyDictionary<string, IMetaValue> NamedArguments) : IMetaAttribute;
internal sealed record MetaValue(string TypeName, object? Value) : IMetaValue;
