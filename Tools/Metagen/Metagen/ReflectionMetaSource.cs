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
        return new MetaSource($"{assembly.GetName().Name}:{fingerprint}", Array.AsReadOnly(types));
    }
    private static IMetaType ReadType(Type type) {
        const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
        var members = new List<IMetaMember>();
        foreach (var field in type.GetFields(flags))
            members.Add(new MetaMember(field.Name, "field", Name(field.FieldType), ReadAttributes(field)));
        foreach (var property in type.GetProperties(flags))
            if (property.GetIndexParameters().Length == 0)
                members.Add(new MetaMember(property.Name, "property", Name(property.PropertyType), ReadAttributes(property)));
        return new MetaType(type.Name, Name(type), type.IsEnum ? "enum" : type.IsInterface ? "interface" : type.IsValueType ? "struct" : "class",
            type.BaseType is null ? null : Name(type.BaseType), ReadAttributes(type),
            Array.AsReadOnly(members.OrderBy(m => m.Name, StringComparer.Ordinal).ToArray()));
    }
    private static string Name(Type type) => type.FullName ?? type.Name;
    private static IMetaValue ReadValue(CustomAttributeTypedArgument argument) {
        object? value = argument.Value;
        if (value is IList<CustomAttributeTypedArgument> array)
            value = Array.AsReadOnly(array.Select(ReadValue).ToArray());
        else if (value is Type type) value = Name(type);
        return new MetaValue(Name(argument.ArgumentType), value);
    }
    private static IReadOnlyList<IMetaAttribute> ReadAttributes(MemberInfo member) {
        // CustomAttributeData avoids invoking attribute constructors.
        return Array.AsReadOnly(member.GetCustomAttributesData().Select(attribute => (IMetaAttribute)new MetaAttribute(
            Name(attribute.AttributeType), Array.AsReadOnly(attribute.ConstructorArguments.Select(ReadValue).ToArray()),
            new ReadOnlyDictionary<string, IMetaValue>(attribute.NamedArguments.ToDictionary(
                value => value.MemberName, value => ReadValue(value.TypedValue), StringComparer.Ordinal)))).ToArray());
    }
}
