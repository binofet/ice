#pragma once

#include "ice/animation/scene_animation.h"
#include <memory>
namespace ice::render {
/// Per-draw scene parameters. View/projection are column-major world-to-camera/clip transforms;
/// pose is borrowed for the draw.
struct scene_view {
	/// Camera-to-clip transform with an explicit backend depth convention. World-to-camera matrix;
	/// the programmable renderer does not invert it again.
	ice::math::mat4 view, projection;
	/// Vertical extent in pixels unless the owning API specifies logical UI units. Horizontal
	/// extent in pixels unless the owning API specifies logical UI units.
	int width = 1, height = 1;
	/// Viewer inspection mode: lit, base color, normals, metallic/roughness, or UV.
	int debug_mode = 0; // 0 lit, 1 base color, 2 normals, 3 metallic/roughness, 4 UV
	/// Request wireframe rasterization where supported by the backend.
	bool wireframe = false;
	/// Asset material index, or a view isolation selector where the type documents -1 as all.
	int material = -1; // -1 all materials; otherwise isolate one
	/// Linear HDR exposure multiplier applied before display conversion.
	float exposure = 1;
	/// Borrowed evaluated pose for this draw; null selects the authored rest pose.
	const ice::animation::pose *pose = nullptr; // null draws the authored rest pose
	/// Optional skeleton-root node index; negative means unspecified.
	bool skeleton = false;
};
/// Context-owned scene rendering interface. Upload retains backend resources; release them before
/// destroying the graphics context.
class scene_renderer {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~scene_renderer() {}
	/// Upload owned backend scene resources transactionally; false records diagnostics and retains
	/// the previous valid scene.
	virtual bool upload(const ice::assets::scene_asset &scene, std::string &diagnostics) = 0;
	/// Draw the uploaded scene with borrowed view/pose data in the owning current context; false
	/// records diagnostics.
	virtual bool draw(const scene_view &view, std::string &diagnostics) = 0;
	/// Release owned native resources in their live owning context; do not retain previous native
	/// names.
	virtual void release() = 0;
};
/// Explicit backend selection; callers use the same asset/view interface.
std::unique_ptr<scene_renderer>
create_scene_renderer_gl(std::shared_ptr<ice::io::resource_resolver> resolver);

} // namespace ice::render
