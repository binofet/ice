using System.Collections.ObjectModel;
using System.Reflection;
using System.Security.Cryptography;
namespace Metagen;

public static class ReflectionMetaSource {
    public static IMetaSource Load(string path) {
        path = Path.GetFullPath(path);
        var assembly = Assembly.LoadFrom(path);
        var types = assembly.GetExportedTypes().OrderBy(t => t.FullName, StringComparer.Ordinal)
            .Select(ReadType).ToArray();
        var fingerprint = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path)));
        return new MetaSource($"{assembly.GetName().Name}:{fingerprint}", Array.AsReadOnly(types),
            ReadAttributes(assembly.GetCustomAttributesData()));
    }
    private static IMetaType ReadType(Type type) {
        const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
        var members = new List<IMetaMember>();
        var nullability = new NullabilityInfoContext();
        foreach (var field in type.GetFields(flags))
            members.Add(new MetaMember(field.Name, "field", Name(field.FieldType), ReadTypeRef(field.FieldType, nullability.Create(field)), ReadAttributes(field),
                field.IsLiteral ? field.GetRawConstantValue() : null));
        foreach (var property in type.GetProperties(flags))
            if (property.GetIndexParameters().Length == 0)
                members.Add(new MetaMember(property.Name, "property", Name(property.PropertyType), ReadTypeRef(property.PropertyType, nullability.Create(property)), ReadAttributes(property)));
        return new MetaType(type.Name, Name(type), type.IsEnum ? "enum" : type.IsInterface ? "interface" : type.IsValueType ? "struct" : "class",
            type.BaseType is null ? null : Name(type.BaseType), ReadAttributes(type),
            Array.AsReadOnly(members.OrderBy(m => m.Name, StringComparer.Ordinal).ToArray()));
    }
    private static MetaTypeRef ReadTypeRef(Type type, NullabilityInfo? nullability = null) {
        bool? nullable = nullability?.ReadState switch {
            NullabilityState.Nullable => true, NullabilityState.NotNull => false, _ => null
        };
        if (type.IsArray)
            return new MetaTypeRef(Name(type), new[] { ReadTypeRef(type.GetElementType()!, nullability?.ElementType) },
                type.GetArrayRank(), NullableAnnotation: nullable);
        return new MetaTypeRef(Name(type.IsGenericType ? type.GetGenericTypeDefinition() : type),
            type.IsGenericType ? type.GetGenericArguments().Select((t, i) => ReadTypeRef(t, nullability?.GenericTypeArguments.ElementAtOrDefault(i))).ToArray() : Array.Empty<MetaTypeRef>(),
            IsEnum: type.IsEnum, EnumUnderlyingType: type.IsEnum ? Name(Enum.GetUnderlyingType(type)) : null,
            NullableAnnotation: nullable, IsGenericDefinition: type.IsGenericTypeDefinition);
    }
    private static string Name(Type type) => type.FullName ?? type.Name;
    private static IMetaValue ReadValue(CustomAttributeTypedArgument argument) {
        object? value = argument.Value;
        MetaTypeRef? typeReference = null;
        if (value is IList<CustomAttributeTypedArgument> array)
            value = Array.AsReadOnly(array.Select(ReadValue).ToArray());
        else if (value is Type type) {
            value = Name(type);
            typeReference = ReadTypeRef(type);
        }
        return new MetaValue(Name(argument.ArgumentType), value, typeReference);
    }
    private static IReadOnlyList<IMetaAttribute> ReadAttributes(MemberInfo member) {
        // CustomAttributeData avoids invoking attribute constructors.
        return ReadAttributes(member.GetCustomAttributesData());
    }
    private static IReadOnlyList<IMetaAttribute> ReadAttributes(IEnumerable<CustomAttributeData> attributes) {
        return Array.AsReadOnly(attributes.Select(attribute => (IMetaAttribute)new MetaAttribute(
            Name(attribute.AttributeType), Array.AsReadOnly(attribute.ConstructorArguments.Select(ReadValue).ToArray()),
            new ReadOnlyDictionary<string, IMetaValue>(attribute.NamedArguments.ToDictionary(
                value => value.MemberName, value => ReadValue(value.TypedValue), StringComparer.Ordinal)))).ToArray());
    }
}
