#pragma once

#include "ice/render/shader.h"
#include <memory>
namespace ice::render {

/// Pipeline face-culling selection; front-face winding is configured separately.
enum class cull_mode { none, back, front };
/// Opaque replacement or conventional alpha blending.
enum class blend_mode { opaque, alpha };
/// Depth, blending, culling, winding, and rasterization state selected when constructing a
/// pipeline.
struct pipeline_state {
	/// Request wireframe rasterization where supported by the backend. Treat clockwise projected
	/// triangles as front-facing. Enable writes to the depth attachment. Enable depth comparison
	/// when drawing the pipeline.
	bool depth_test = true, depth_write = true, clockwise_front = false, wireframe = false;
	/// Pipeline face-culling policy.
	cull_mode cull = cull_mode::back;
	/// Pipeline color-blending policy.
	blend_mode blend = blend_mode::opaque;
};
/// Meaning of an uploaded vertex attribute; joint indices/weights are skin-local.
enum class vertex_semantic {
	position,
	normal,
	tangent,
	tex_coord0,
	tex_coord1,
	color0,
	joints0,
	weights0
};
/// Packed floating-point attribute width; enumerator values equal the scalar count.
enum class vertex_format { float2 = 2, float3 = 3, float4 = 4 };
/// Vertex semantic, scalar format, and byte offset relative to the start of a vertex.
struct vertex_attribute {
	/// Meaning of this vertex attribute.
	vertex_semantic semantic;
	/// Format/token interpreted by the enclosing API.
	vertex_format format;
	/// Attribute byte offset relative to the start of one vertex.
	unsigned offset;
	/// Describe an interleaved floating-point attribute with a byte offset from the vertex start.
	vertex_attribute(vertex_semantic s, vertex_format f, unsigned o)
	    : semantic(s), format(f), offset(o) {}
};
/// Interleaved vertex stride in bytes and owned attribute declarations.
struct vertex_layout {
	/// Total byte stride between adjacent interleaved vertices.
	unsigned stride = 0;
	/// Owned vertex attribute layout declarations.
	std::vector<vertex_attribute> attributes;
};
/// Display-encoded 8-bit color or linear floating-point HDR render-target storage.
enum class color_target_format { display_rgba8, linear_rgba16_float };
/// Pipeline creation description retaining shared shader stages and owned vertex-layout/state
/// metadata.
struct pipeline_desc {
	/// URI fragment or retained fragment stage, as defined by the record. Retained compiled vertex
	/// stage.
	std::shared_ptr<shader> vertex, fragment;
	/// Pipeline/device state snapshot selected for this operation.
	pipeline_state state;
	/// Owned interleaved vertex data for this primitive.
	vertex_layout vertices;
	/// Color-attachment storage/encoding expected by this pipeline.
	color_target_format color_target = color_target_format::display_rgba8;
	/// Owned human-readable label used in diagnostics.
	std::string label;
};
/// Creation retains stages. Command encoders/backends select pipelines; resources are not
/// assigned through per-backend uniform locations in this common interface.
class pipeline {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~pipeline() {}
	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	virtual bool create(const pipeline_desc &description, std::string &diagnostics) = 0;
	/// Return whether this object currently holds a successfully created native resource.
	virtual bool valid() const = 0;
	/// Borrow the successfully created pipeline description and retained shader stages.
	virtual const pipeline_desc &descriptor() const = 0;
	/// Borrow the reflected linked-program bindings; recreation invalidates prior references.
	virtual const std::vector<shader_binding> &bindings() const = 0;
};

} // namespace ice::render
