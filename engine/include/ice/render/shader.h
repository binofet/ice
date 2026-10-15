#pragma once

#include <string>
#include <vector>
namespace ice::render {

/// Programmable pipeline stage; this interface currently describes vertex and fragment stages.
enum class shader_stage { vertex, fragment };
/// Explicit shader source or bytecode language; a backend may reject unsupported languages.
enum class shader_language { glsl, hlsl, msl, spirv };
/// Owned shader source, entry point, label, and origin URI for diagnostics.
struct shader_source {
	/// Programmable pipeline stage for this source.
	shader_stage stage = shader_stage::vertex;
	/// Explicit shader source/bytecode language.
	shader_language language = shader_language::glsl;
	/// Owned origin URI for diagnostics/resource resolution. Owned human-readable label used in
	/// diagnostics. Owned shader entry-point name; backend support is validated at compile time.
	/// Origin URI/source payload or input mode, as specified by the enclosing record.
	std::string source, entry_point = "main", label, uri;
};
/// Reflected binding category: ordinary value or sampled texture.
enum class shader_binding_type { value, texture };
/// Reflected binding name, category, and array element count; native uniform locations are backend-
/// private.
struct shader_binding {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Reflected shader binding category.
	shader_binding_type type = shader_binding_type::value;
	/// Number of reflected array elements in this binding.
	unsigned elements = 1;
};
/// One compiled stage. Source language/backend support is explicit; no GL handles or locations.
class shader {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~shader() {}
	/// Compile a stage transactionally; failure returns diagnostics and retains the last valid
	/// stage.
	virtual bool compile(const shader_source &source, std::string &diagnostics) = 0;
	/// Return whether this object currently holds a successfully created native resource.
	virtual bool valid() const = 0;
	/// Borrow the successfully compiled source description; it remains owned by the stage.
	virtual const shader_source &source() const = 0;
};

} // namespace ice::render
