#pragma once
#include "ice/render/device.h"
namespace ice::render::vulkan {
struct context;
struct pipeline_state;
struct mesh_state;
class pipeline {
public:
	std::shared_ptr<pipeline_state> state;
};
class mesh {
public:
	std::shared_ptr<mesh_state> state;
};
struct draw_item {
	const vulkan::pipeline    *pipeline;
	const vulkan::mesh        *geometry;
	std::span<const std::byte> uniforms;
};
class presentation_surface {
public:
	~presentation_surface();
	ice::expected<frame_status, diagnostic> draw_batch(std::span<const draw_item>,
	                                                   std::array<float, 4>);
	bool                                    resize(int width, int height);
	ice::expected<void, diagnostic>         wait_idle();

private:
	struct implementation;
	explicit presentation_surface(std::unique_ptr<implementation>);
	std::unique_ptr<implementation> state_;
	friend class device;
};
class device {
public:
	static ice::expected<std::unique_ptr<device>, diagnostic> create();
	ice::expected<std::unique_ptr<pipeline>, diagnostic> prepare_pipeline(const shader_snapshot &,
	                                                                      const std::string &,
	                                                                      const std::string &,
	                                                                      pipeline_options);
	ice::expected<std::unique_ptr<mesh>, diagnostic>     create_mesh(const mesh_data &);
	ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
	                                attach(ice::platform::desktop_window &);
	ice::expected<void, diagnostic> wait_idle();
	std::string                     name() const;
	std::string                     take_error();

private:
	std::shared_ptr<context> state_;
};
} // namespace ice::render::vulkan
