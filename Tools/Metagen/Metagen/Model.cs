namespace Metagen;

// Consumers depend on this model, not Assembly/Type/MemberInfo.
public interface IMetaSource {
    string Identity { get; }
    IReadOnlyList<IMetaAttribute> Attributes { get; }
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
    MetaTypeRef Type { get; }
    IReadOnlyList<IMetaAttribute> Attributes { get; }
    object? ConstantValue { get; }
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
    // Structured metadata for a type literal, alongside its original readable name.
    MetaTypeRef? TypeReference { get; }
}
public sealed record MetaSource(string Identity, IReadOnlyList<IMetaType> Types,
    IReadOnlyList<IMetaAttribute>? SourceAttributes = null) : IMetaSource {
    public IReadOnlyList<IMetaAttribute> Attributes => SourceAttributes ?? Array.Empty<IMetaAttribute>();
}
public sealed record MetaType(string Name, string FullName, string Kind, string? BaseType,
    IReadOnlyList<IMetaAttribute> Attributes, IReadOnlyList<IMetaMember> Members) : IMetaType;
public sealed record MetaMember(string Name, string Kind, string ValueType, MetaTypeRef Type,
    IReadOnlyList<IMetaAttribute> Attributes, object? ConstantValue = null) : IMetaMember;
public sealed record MetaAttribute(string TypeName, IReadOnlyList<IMetaValue> Arguments,
    IReadOnlyDictionary<string, IMetaValue> NamedArguments) : IMetaAttribute;
public sealed record MetaValue(string TypeName, object? Value, MetaTypeRef? TypeReference = null) : IMetaValue;

// A type-use preserves language metadata; consumers choose their own portable meaning.
public sealed record MetaTypeRef(string Name, IReadOnlyList<MetaTypeRef> Arguments,
    int ArrayRank = 0, bool IsEnum = false, string? EnumUnderlyingType = null,
    bool? NullableAnnotation = null, bool IsGenericDefinition = false);
