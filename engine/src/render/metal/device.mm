#include "ice/render/metal/device.h"
#include "../batch_validation.h"
#include "../readback_state.h"
#include "ice/core/utf8.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <TargetConditionals.h>
#include <cstring>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif
#include <algorithm>
#include <mutex>
#include <set>
#include <utility>

namespace ice::render::metal {
namespace {
struct layer_lease {
	void                    *identity;
	static std::set<void *> &active() {
		// Registry outlives any process-global presentation owner.
		static auto *layers = new std::set<void *>;
		return *layers;
	}
	explicit layer_lease(void *identity) : identity(identity) { active().insert(identity); }
	~layer_lease() { active().erase(identity); }
};
} // namespace
namespace detail {
struct device_state {
	std::shared_ptr<ice::render::detail::readback_accounting> readbacks =
	    std::make_shared<ice::render::detail::readback_accounting>();
	id<MTLDevice>        device;
	id<MTLCommandQueue>  queue;
	std::mutex           mutex;
	std::string          error;
	dispatch_semaphore_t target_available = dispatch_semaphore_create(64);
	void                 completed(id<MTLCommandBuffer> command) {
        if (command.status != MTLCommandBufferStatusError)
            return;
        std::lock_guard guard(mutex);
        if (error.empty()) {
            const char *message = command.error.localizedDescription.UTF8String;
            if (!message)
                message = "Metal command failed";
            std::size_t length = 0;
            while (length < 4096 && message[length])
                ++length;
            // Preserve a UTF-8 boundary when a long native diagnostic is truncated.
            while (length && (static_cast<unsigned char>(message[length]) & 0xc0) == 0x80)
                --length;
            error.assign(message, length);
        }
	}
};
} // namespace detail
struct pipeline::implementation {
	std::shared_ptr<detail::device_state> state;
	id<MTLRenderPipelineState>            native;
	std::size_t                           uniform_bytes    = 0;
	std::uint8_t                          sampled_bindings = 0;
	pipeline_options                      options;
	id<MTLDepthStencilState>              depth;
};
struct mesh::implementation {
	std::shared_ptr<detail::device_state> state;
	vertex_layout                         layout = vertex_layout::position_normal;
	id<MTLBuffer>                         vertices, indices;
	NSUInteger                            index_count = 0;
};
struct texture::implementation {
	std::shared_ptr<detail::device_state> state;
	id<MTLTexture>                        native;
	int                                   width = 0, height = 0;
	bool                                  depth = false, attachment = false, initialized = true;
};
struct render_target::implementation {
	std::shared_ptr<detail::device_state> state;
	render_target_description             description;
	std::unique_ptr<texture>              color, depth;
};
struct presentation_surface::implementation {
	// Release exclusivity after native state; both common and backend-specific APIs use it.
	std::unique_ptr<layer_lease>          lease;
	std::shared_ptr<detail::device_state> state;
	CAMetalLayer                         *layer;
	dispatch_semaphore_t                  available = dispatch_semaphore_create(3);
	int                                   width = 0, height = 0;
	id<MTLTexture>                        depth;
#if TARGET_OS_OSX
	NSView *__weak view                 = nil;
	CALayer       *previous_layer       = nil;
	bool           previous_wants_layer = false;
	~implementation() {
		if (view && view.layer == layer) {
			view.layer      = previous_layer;
			view.wantsLayer = previous_wants_layer;
		}
	}
#endif
};
namespace {
ice::unexpected<diagnostic> failure(std::string message) {
	return ice::unexpected(diagnostic{std::move(message)});
}
NSString *native_string(const std::string &text) {
	return [[NSString alloc] initWithBytes:text.data()
	                                length:text.size()
	                              encoding:NSUTF8StringEncoding];
}
std::string native_error(NSError *error, const char *fallback) {
	const char *message = error.localizedDescription.UTF8String;
	return message ? message : fallback;
}
bool valid_draw(const std::shared_ptr<detail::device_state> &state,
                const std::shared_ptr<detail::device_state> &pipeline_state,
                std::span<const std::byte> uniforms, std::size_t required) {
	return state == pipeline_state && uniforms.size() >= required &&
	       uniforms.size() <= max_uniform_bytes;
}
} // namespace
struct detail::batch_access {
	static ice::expected<void, diagnostic> validate(const std::shared_ptr<device_state> &state,
	                                                const frame_batch                   &batch,
	                                                color_format format = color_format::rgba8_unorm,
	                                                id<MTLTexture> color = nil,
	                                                id<MTLTexture> depth = nil) {
		auto shape = ice::render::detail::validate_batch_shape(batch);
		if (!shape)
			return shape;
		for (const auto &item : batch.draws) {
			const auto &prepared = *item.pipeline->implementation_;
			if (!valid_draw(state, prepared.state, item.uniforms, prepared.uniform_bytes) ||
			    (item.geometry && item.geometry->implementation_->state != state))
				return failure("Foreign frame resource or invalid uniform byte count");
			if (prepared.options.vertices != (item.geometry ? item.geometry->implementation_->layout
			                                                : vertex_layout::procedural))
				return failure("Pipeline and mesh vertex layouts do not match");
			if (prepared.options.target_format != format)
				return failure("Pipeline color format does not match the destination");
			for (unsigned slot = 0; slot < 4; ++slot) {
				auto       sampled  = ice::render::detail::sampled_at(item, slot);
				const bool required = prepared.sampled_bindings & (1u << slot);
				if (required && !sampled)
					return failure("Frame pipeline is missing a required sampled slot");
				if (!sampled)
					continue;
				const auto &view = *sampled->implementation_;
				if (!view.initialized)
					return failure("Sampled target has not received a successful pass");
				if (view.state != state)
					return failure("Frame contains a foreign sampled image");
				if (view.native == color || view.native == depth)
					return failure("Render target color/depth feedback is unsupported");
				if (required &&
				    view.depth != bool(prepared.options.depth_sampled_bindings & (1u << slot)))
					return failure("Sampled slot color/depth kind does not match its declaration");
			}
		}
		return {};
	}
	static id<MTLTexture> depth_texture(const std::shared_ptr<device_state> &state, int width,
	                                    int height) {
		@autoreleasepool {
			auto descriptor =
			    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
			                                                       width:width
			                                                      height:height
			                                                   mipmapped:NO];
			descriptor.storageMode = MTLStorageModePrivate;
			descriptor.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			return [state->device newTextureWithDescriptor:descriptor];
		}
	}
	static bool encode(id<MTLCommandBuffer> command, id<MTLTexture> target, id<MTLTexture> depth,
	                   const frame_batch &batch, bool store_depth = false) {
		// setBytes is limited to four KiB. Prepare every larger owned allocation before
		// creating the render encoder: a nil buffer cannot partially clear/draw the target.
		// ARC retains these buffers through encoding; ordinary retained-reference command
		// buffers then retain each encoded buffer through GPU completion.
		std::vector<id<MTLBuffer>> uniforms(batch.draws.size(), nil);
		for (std::size_t index = 0; index < batch.draws.size(); ++index) {
			const auto &item = batch.draws[index];
			if (item.uniforms.size() <= 4096)
				continue;
			uniforms[index] = [item.pipeline->implementation_->state->device
			    newBufferWithBytes:item.uniforms.data()
			                length:item.uniforms.size()
			               options:MTLResourceStorageModeShared];
			if (!uniforms[index])
				return false;
		}
		MTLRenderPassDescriptor *pass        = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture     = target;
		pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor  = MTLClearColorMake(
            batch.clear_color[0], batch.clear_color[1], batch.clear_color[2], batch.clear_color[3]);
		pass.depthAttachment.texture    = depth;
		pass.depthAttachment.loadAction = MTLLoadActionClear;
		pass.depthAttachment.storeAction =
		    store_depth ? MTLStoreActionStore : MTLStoreActionDontCare;
		pass.depthAttachment.clearDepth     = 1.0;
		id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
		if (!encoder)
			return false;
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
		[encoder setCullMode:MTLCullModeNone];
		for (std::size_t index = 0; index < batch.draws.size(); ++index) {
			const auto &item     = batch.draws[index];
			const auto &prepared = *item.pipeline->implementation_;
			[encoder setRenderPipelineState:prepared.native];
			[encoder setDepthStencilState:prepared.depth];
			if (uniforms[index]) {
				[encoder setVertexBuffer:uniforms[index] offset:0 atIndex:0];
				[encoder setFragmentBuffer:uniforms[index] offset:0 atIndex:0];
			} else if (!item.uniforms.empty()) {
				[encoder setVertexBytes:item.uniforms.data() length:item.uniforms.size() atIndex:0];
				[encoder setFragmentBytes:item.uniforms.data()
				                   length:item.uniforms.size()
				                  atIndex:0];
			}
			for (unsigned slot = 0; slot < 4; ++slot) {
				auto           image   = ice::render::detail::sampled_at(item, slot);
				id<MTLTexture> sampled = image ? image->implementation_->native : nil;
				[encoder setVertexTexture:sampled atIndex:slot];
				[encoder setFragmentTexture:sampled atIndex:slot];
			}
			if (item.geometry) {
				const auto &geometry = *item.geometry->implementation_;
				[encoder setVertexBuffer:geometry.vertices offset:0 atIndex:1];
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
				                    indexCount:geometry.index_count
				                     indexType:MTLIndexTypeUInt32
				                   indexBuffer:geometry.indices
				             indexBufferOffset:0];
			} else {
				[encoder setVertexBuffer:nil offset:0 atIndex:1];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
			}
		}
		[encoder endEncoding];
		return true;
	}
};
pipeline::pipeline(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
pipeline::~pipeline() = default;
mesh::mesh(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
mesh::~mesh() = default;
texture::texture(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
texture::~texture() = default;
render_target::render_target(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
render_target::~render_target() = default;
const texture            &render_target::color() const { return *implementation_->color; }
const texture            &render_target::depth() const { return *implementation_->depth; }
render_target_description render_target::description() const {
	return implementation_->description;
}
ice::expected<void, diagnostic> texture::update(const image &pixels) {
	auto &data = *implementation_;
	if (data.attachment)
		return failure("Render target views cannot be updated from CPU pixels");
	if (pixels.width != data.width || pixels.height != data.height ||
	    pixels.rgba.size() != std::size_t(data.width) * data.height * 4)
		return failure("Texture update requires matching dimensions and exact RGBA8 pixels");
	@autoreleasepool {
		MTLTextureDescriptor *descriptor =
		    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		                                                       width:data.width
		                                                      height:data.height
		                                                   mipmapped:NO];
		descriptor.storageMode     = MTLStorageModeShared;
		descriptor.usage           = MTLTextureUsageShaderRead;
		id<MTLTexture> replacement = [data.state->device newTextureWithDescriptor:descriptor];
		if (!replacement)
			return failure("Metal sampled texture allocation failed");
		[replacement replaceRegion:MTLRegionMake2D(0, 0, data.width, data.height)
		               mipmapLevel:0
		                 withBytes:pixels.rgba.data()
		               bytesPerRow:std::size_t(data.width) * 4];
		// Never overwrite storage that an earlier command buffer may still sample.
		data.native = replacement;
		return {};
	}
}
presentation_surface::presentation_surface(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
presentation_surface::~presentation_surface() = default;
device::device(std::shared_ptr<detail::device_state> state) : state_(std::move(state)) {}
device::~device() = default;
ice::expected<std::unique_ptr<device>, diagnostic> device::create() {
	@autoreleasepool {
		auto state    = std::make_shared<detail::device_state>();
		state->device = MTLCreateSystemDefaultDevice();
		if (!state->device)
			return failure("No Metal device is available");
		state->queue = [state->device newCommandQueue];
		if (!state->queue)
			return failure("Metal command queue creation failed");
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<device>(new device(std::move(state)));
	}
}
std::string device::name() const {
	const char *name = state_->device.name.UTF8String;
	return name ? name : "Metal";
}
std::string device::take_error() {
	std::lock_guard guard(state_->mutex);
	return std::exchange(state_->error, {});
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const ice::io::resource_resolver &resolver, const std::string &uri,
                         const std::string &vertex_entry, const std::string &fragment_entry,
                         pipeline_options options) {
	auto snapshot = shader_snapshot::read(resolver, uri);
	if (!snapshot)
		return ice::unexpected(snapshot.error());
	return prepare_pipeline(*snapshot, vertex_entry, fragment_entry, options);
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const shader_snapshot &snapshot, const std::string &vertex_entry,
                         const std::string &fragment_entry, pipeline_options options) {
	if (options.target_format == color_format::rgba8_srgb)
		return failure("sRGB imported destinations are not supported by the Metal profile");
	if (!ice::render::detail::valid_options(options))
		return failure("Invalid vertex layout or depth-write without depth-test");
	@autoreleasepool {
		const auto &uri = snapshot.uri();
		std::string source(snapshot.source());
		if (!ice::core::valid_utf8_text(source, 1024 * 1024) || source.empty() ||
		    !ice::core::valid_utf8_text(vertex_entry, 128) || vertex_entry.empty() ||
		    !ice::core::valid_utf8_text(fragment_entry, 128) || fragment_entry.empty())
			return failure(uri + ": invalid source text or entry point");
		NSError       *error   = nil;
		id<MTLLibrary> library = [state_->device newLibraryWithSource:native_string(source)
		                                                      options:nil
		                                                        error:&error];
		if (!library)
			return failure(uri + ": " + native_error(error, "Metal compilation failed"));
		MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
		descriptor.label                        = native_string(uri);
		descriptor.vertexFunction   = [library newFunctionWithName:native_string(vertex_entry)];
		descriptor.fragmentFunction = [library newFunctionWithName:native_string(fragment_entry)];
		if (!descriptor.vertexFunction || !descriptor.fragmentFunction)
			return failure(uri + ": a requested shader entry point is missing");
		descriptor.colorAttachments[0].pixelFormat =
		    options.target_format == color_format::rgba16_float ? MTLPixelFormatRGBA16Float
		                                                        : MTLPixelFormatBGRA8Unorm;
		descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		if (options.alpha_blend) {
			auto attachment                        = descriptor.colorAttachments[0];
			attachment.blendingEnabled             = YES;
			attachment.sourceRGBBlendFactor        = MTLBlendFactorOne;
			attachment.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
			attachment.sourceAlphaBlendFactor      = MTLBlendFactorOne;
			attachment.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		}
		bool position_attribute = false;
		for (MTLAttribute *attribute in descriptor.vertexFunction.stageInputAttributes) {
			if (!attribute.active)
				continue;
			const bool vector3 =
			    attribute.attributeIndex < 2 && attribute.attributeType == MTLDataTypeFloat3;
			const bool skinned = options.vertices == vertex_layout::position_normal_uv_skin;
			const bool uv = (options.vertices == vertex_layout::position_normal_uv || skinned) &&
			                attribute.attributeIndex == 2 &&
			                attribute.attributeType == MTLDataTypeFloat2;
			const bool influences =
			    skinned && (attribute.attributeIndex == 3 || attribute.attributeIndex == 4) &&
			    attribute.attributeType == MTLDataTypeFloat4;
			if (options.vertices == vertex_layout::procedural || (!vector3 && !uv && !influences))
				return failure(uri + ": vertex inputs must match the declared "
				                     "position0/normal1[/uv2[/joints3/weights4]] layout");
			position_attribute |= attribute.attributeIndex == 0;
		}
		if (options.vertices != vertex_layout::procedural) {
			if (!position_attribute)
				return failure(uri +
				               ": indexed pipelines require active float3 position at attribute0");
			auto vertices = [MTLVertexDescriptor vertexDescriptor];
			for (unsigned index = 0; index < 2; ++index) {
				vertices.attributes[index].format      = MTLVertexFormatFloat3;
				vertices.attributes[index].offset      = index * 12;
				vertices.attributes[index].bufferIndex = 1;
			}
			const bool skinned = options.vertices == vertex_layout::position_normal_uv_skin;
			if (options.vertices == vertex_layout::position_normal_uv || skinned) {
				vertices.attributes[2].format      = MTLVertexFormatFloat2;
				vertices.attributes[2].offset      = offsetof(mesh_vertex_uv, uv);
				vertices.attributes[2].bufferIndex = 1;
			}
			if (skinned)
				for (unsigned index = 3; index < 5; ++index) {
					vertices.attributes[index].format      = MTLVertexFormatFloat4;
					vertices.attributes[index].offset      = index == 3
					                                             ? offsetof(mesh_vertex_skin, joints)
					                                             : offsetof(mesh_vertex_skin, weights);
					vertices.attributes[index].bufferIndex = 1;
				}
			vertices.layouts[1].stride       = skinned ? sizeof(mesh_vertex_skin)
			                                   : options.vertices == vertex_layout::position_normal_uv
			                                       ? sizeof(mesh_vertex_uv)
			                                       : sizeof(mesh_vertex);
			vertices.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;
			vertices.layouts[1].stepRate     = 1;
			descriptor.vertexDescriptor      = vertices;
		}
		MTLRenderPipelineReflection *reflection = nil;
		auto                         result     = std::make_unique<pipeline::implementation>();
		result->state                           = state_;
		result->options                         = options;
		auto depth                              = [MTLDepthStencilDescriptor new];
		depth.depthCompareFunction =
		    options.depth_test ? MTLCompareFunctionLess : MTLCompareFunctionAlways;
		depth.depthWriteEnabled = options.depth_write;
		result->depth           = [state_->device newDepthStencilStateWithDescriptor:depth];
		if (!result->depth)
			return failure(uri + ": Metal depth-stencil state creation failed");
		result->native =
		    [state_->device newRenderPipelineStateWithDescriptor:descriptor
		                                                 options:MTLPipelineOptionBindingInfo
		                                              reflection:&reflection
		                                                   error:&error];
		if (!result->native || !reflection)
			return failure(uri + ": " +
			               native_error(error, "Metal pipeline creation/reflection failed"));
		if (@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)) {
			for (unsigned stage = 0; stage < 2; ++stage) {
				NSArray<id<MTLBinding>> *bindings =
				    stage == 0 ? reflection.vertexBindings : reflection.fragmentBindings;
				for (id<MTLBinding> binding in bindings) {
					if (!binding.used || !binding.argument)
						continue;
					if (stage == 0 && options.vertices != vertex_layout::procedural &&
					    binding.type == MTLBindingTypeBuffer && binding.index == 1 &&
					    binding.access == MTLBindingAccessReadOnly)
						continue;
					if (binding.type == MTLBindingTypeTexture && binding.index < 4 &&
					    binding.access == MTLBindingAccessReadOnly) {
						auto sampled = static_cast<id<MTLTextureBinding>>(binding);
						if (sampled.textureType != MTLTextureType2D ||
						    sampled.textureDataType != MTLDataTypeFloat ||
						    sampled.arrayLength != 1 ||
						    sampled.depthTexture !=
						        bool(options.depth_sampled_bindings & (1u << binding.index)))
							return failure(uri + ": sampled textures must match declared float "
							                     "color/depth slots0..3");
						result->sampled_bindings |= std::uint8_t(1u << binding.index);
						continue;
					}
					if (binding.type != MTLBindingTypeBuffer || binding.index != 0 ||
					    binding.access != MTLBindingAccessReadOnly)
						return failure(
						    uri + ": this pipeline supports read-only buffer(0) and texture(0..3)");
					auto buffer = static_cast<id<MTLBufferBinding>>(binding);
					result->uniform_bytes =
					    std::max(result->uniform_bytes, std::size_t(buffer.bufferDataSize));
				}
			}
		} else
			return failure(uri + ": pipeline reflection requires macOS 13 / iOS 16 or newer");
		if ((options.sampled_bindings == 0xff && result->sampled_bindings > 1) ||
		    (options.sampled_bindings != 0xff &&
		     options.sampled_bindings != result->sampled_bindings))
			return failure(uri + ": active sampled slots do not match the pipeline descriptor");
		if (result->uniform_bytes > max_uniform_bytes)
			return failure(uri + ": uniform block exceeds 16384 bytes");
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<pipeline>(new pipeline(std::move(result)));
	}
}
ice::expected<std::unique_ptr<render_target>, diagnostic>
device::create_render_target(const render_target_description &description) {
	if (!ice::render::detail::valid_target(description))
		return failure("Invalid render target dimensions or color format");
		// Metal's feature tables specify 16K for Mac and Apple3+, 8K for older Apple GPUs.
		// Keep the public 16K bound even on newer families with larger native limits.
#if TARGET_OS_OSX
	const int maximum_dimension = 16384;
#else
	const int maximum_dimension = [state_->device supportsFamily:MTLGPUFamilyApple3] ? 16384 : 8192;
#endif
	if (description.width > maximum_dimension || description.height > maximum_dimension)
		return failure("Render target exceeds this Metal GPU family's texture dimensions");
	@autoreleasepool {
		auto result         = std::make_unique<render_target::implementation>();
		result->state       = state_;
		result->description = description;
		for (unsigned aspect = 0; aspect < 2; ++aspect) {
			auto data         = std::make_unique<texture::implementation>();
			data->state       = state_;
			data->width       = description.width;
			data->height      = description.height;
			data->depth       = aspect != 0;
			data->attachment  = true;
			data->initialized = false;
			auto format       = aspect ? MTLPixelFormatDepth32Float
			                    : description.format == color_format::rgba16_float
			                        ? MTLPixelFormatRGBA16Float
			                        : MTLPixelFormatBGRA8Unorm;
			auto native =
			    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
			                                                       width:description.width
			                                                      height:description.height
			                                                   mipmapped:NO];
			native.storageMode = MTLStorageModePrivate;
			native.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			data->native       = [state_->device newTextureWithDescriptor:native];
			if (!data->native)
				return failure("Metal render target color/depth allocation failed");
			if (aspect)
				result->depth.reset(new texture(std::move(data)));
			else
				result->color.reset(new texture(std::move(data)));
		}
		// Ownership transfers through tl::expected.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<render_target>(new render_target(std::move(result)));
	}
}
ice::expected<frame_status, diagnostic> device::render_into(render_target     &target,
                                                            const frame_batch &batch) {
	auto &output = *target.implementation_;
	if (output.state != state_)
		return failure("Foreign render target");
	auto valid = detail::batch_access::validate(state_, batch, output.description.format,
	                                            output.color->implementation_->native,
	                                            output.depth->implementation_->native);
	if (!valid)
		return ice::unexpected(valid.error());
	if (dispatch_semaphore_wait(state_->target_available, DISPATCH_TIME_NOW) != 0)
		return frame_status::busy;
	@autoreleasepool {
		auto command = [state_->queue commandBuffer];
		if (!command ||
		    !detail::batch_access::encode(command, output.color->implementation_->native,
		                                  output.depth->implementation_->native, batch, true)) {
			dispatch_semaphore_signal(state_->target_available);
			return failure("Metal target command encoding failed");
		}
		auto state = state_;
		[command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
		  state->completed(completed);
		  dispatch_semaphore_signal(state->target_available);
		}];
		[command commit];
		output.color->implementation_->initialized = true;
		output.depth->implementation_->initialized = true;
		return frame_status::submitted;
	}
}
ice::expected<void, diagnostic> device::wait_idle() {
	@autoreleasepool {
		auto barrier = [state_->queue commandBuffer];
		if (!barrier)
			return failure("Metal device drain allocation failed");
		[barrier commit];
		[barrier waitUntilCompleted];
		// A queue barrier orders GPU work. Also wait for target completion handlers,
		// so asynchronous target failures are visible to synchronous preparation.
		for (unsigned index = 0; index < 64; ++index)
			dispatch_semaphore_wait(state_->target_available, DISPATCH_TIME_FOREVER);
		for (unsigned index = 0; index < 64; ++index)
			dispatch_semaphore_signal(state_->target_available);
		if (barrier.status != MTLCommandBufferStatusCompleted)
			return failure(native_error(barrier.error, "Metal device drain failed"));
		auto error = take_error();
		if (!error.empty())
			return failure(std::move(error));
		return {};
	}
}
ice::expected<std::unique_ptr<mesh>, diagnostic> device::create_mesh(const mesh_data &description) {
	auto valid = ice::render::detail::validate_mesh_data(description);
	if (!valid)
		return ice::unexpected(valid.error());
	return upload_mesh(std::as_bytes(std::span(description.vertices)), description.indices,
	                   vertex_layout::position_normal);
}
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::create_mesh(const textured_mesh_data &description) {
	auto valid = ice::render::detail::validate_mesh_data(description);
	if (!valid)
		return ice::unexpected(valid.error());
	return upload_mesh(std::as_bytes(std::span(description.vertices)), description.indices,
	                   vertex_layout::position_normal_uv);
}
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::create_mesh(const skinned_mesh_data &description) {
	auto valid = ice::render::detail::validate_mesh_data(description);
	if (!valid)
		return ice::unexpected(valid.error());
	return upload_mesh(std::as_bytes(std::span(description.vertices)), description.indices,
	                   vertex_layout::position_normal_uv_skin);
}
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::upload_mesh(std::span<const std::byte> vertices, std::span<const std::uint32_t> indices,
                    vertex_layout layout) {
	@autoreleasepool {
		auto data      = std::make_unique<mesh::implementation>();
		data->state    = state_;
		data->layout   = layout;
		data->vertices = [state_->device newBufferWithBytes:vertices.data()
		                                             length:vertices.size()
		                                            options:MTLResourceStorageModeShared];
		data->indices  = [state_->device newBufferWithBytes:indices.data()
                                                    length:indices.size_bytes()
                                                   options:MTLResourceStorageModeShared];
		if (!data->vertices || !data->indices)
			return failure("Metal mesh vertex/index allocation failed");
		data->index_count = indices.size();
		// Ownership transfers through tl::expected.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<mesh>(new mesh(std::move(data)));
	}
}
ice::expected<std::unique_ptr<texture>, diagnostic> device::create_texture(const image &pixels) {
	if (pixels.width < 1 || pixels.height < 1 || pixels.width > 4096 || pixels.height > 4096 ||
	    pixels.rgba.size() != std::size_t(pixels.width) * pixels.height * 4)
		return failure("Invalid sampled image dimensions or RGBA8 byte count");
	auto data    = std::make_unique<texture::implementation>();
	data->state  = state_;
	data->width  = pixels.width;
	data->height = pixels.height;
	// The owner transfers through tl::expected; analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	auto result   = std::unique_ptr<texture>(new texture(std::move(data)));
	auto uploaded = result->update(pixels);
	if (!uploaded)
		return ice::unexpected(uploaded.error());
	return result;
}
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach_layer(void *layer, int width, int height) {
	if (!layer || width < 0 || height < 0 || width > 16384 || height > 16384)
		return failure("Invalid Metal layer or drawable dimensions");
	@autoreleasepool {
		id native = (__bridge id)layer;
		if (![native isKindOfClass:CAMetalLayer.class])
			return failure("Native endpoint is not a CAMetalLayer");
		if (layer_lease::active().contains(layer))
			return failure("A renderer already owns this Metal layer; drain and detach it first");
		auto result                   = std::make_unique<presentation_surface::implementation>();
		result->lease                 = std::make_unique<layer_lease>(layer);
		result->state                 = state_;
		result->layer                 = native;
		result->layer.device          = state_->device;
		result->layer.pixelFormat     = MTLPixelFormatBGRA8Unorm;
		result->layer.framebufferOnly = YES;
		result->layer.allowsNextDrawableTimeout = YES;
		result->width                           = width;
		result->height                          = height;
		result->layer.drawableSize              = CGSizeMake(width, height);
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<presentation_surface>(new presentation_surface(std::move(result)));
	}
}
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach(ice::platform::desktop_window &window) {
#if TARGET_OS_OSX
	@autoreleasepool {
		auto handle = window.native_handle();
		if (handle.kind != ice::platform::native_window_kind::appkit || !handle.view)
			return failure("Metal desktop attachment requires an AppKit view");
		NSView *view = (__bridge NSView *)handle.view;
		if ([view.layer isKindOfClass:CAMetalLayer.class])
			return failure("Detach the active Metal renderer before attaching another");
		CAMetalLayer *layer  = [CAMetalLayer layer];
		auto          extent = window.extent();
		auto result = attach_layer((__bridge void *)layer, extent.pixel_width, extent.pixel_height);
		if (!result)
			return result;
		auto &data                = *(*result)->implementation_;
		data.view                 = view;
		data.previous_layer       = view.layer;
		data.previous_wants_layer = view.wantsLayer;
		view.wantsLayer           = YES;
		view.layer                = layer;
		layer.contentsScale       = extent.scale;
		return result;
	}
#else
	(void)window;
	return failure("Use attach_layer with the native mobile host's CAMetalLayer");
#endif
}
bool presentation_surface::resize(int width, int height) {
	if (width < 0 || height < 0 || width > 16384 || height > 16384)
		return false;
	if (implementation_->width == width && implementation_->height == height)
		return true;
	implementation_->width              = width;
	implementation_->height             = height;
	implementation_->layer.drawableSize = CGSizeMake(width, height);
	return true;
}
ice::expected<void, diagnostic> presentation_surface::wait_idle() {
	@autoreleasepool {
		id<MTLCommandBuffer> barrier = [implementation_->state->queue commandBuffer];
		if (!barrier)
			return failure("Metal queue drain allocation failed");
		[barrier commit];
		[barrier waitUntilCompleted];
		if (barrier.status != MTLCommandBufferStatusCompleted)
			return failure(native_error(barrier.error, "Metal queue drain failed"));
		return {};
	}
}
ice::expected<frame_status, diagnostic>
presentation_surface::draw(const pipeline &pipeline, std::span<const std::byte> uniforms,
                           const texture *sampled_image) {
	const draw_item item{&pipeline, nullptr, uniforms, sampled_image};
	return draw_batch(frame_batch{std::span(&item, 1)});
}
ice::expected<frame_status, diagnostic> presentation_surface::draw_batch(const frame_batch &batch) {
	auto &data  = *implementation_;
	auto  valid = detail::batch_access::validate(data.state, batch);
	if (!valid)
		return ice::unexpected(valid.error());
	if (!data.width || !data.height)
		return frame_status::unavailable;
	if (!data.depth || data.depth.width != NSUInteger(data.width) ||
	    data.depth.height != NSUInteger(data.height)) {
		auto replacement = detail::batch_access::depth_texture(data.state, data.width, data.height);
		if (!replacement)
			return failure("Metal presentation depth allocation failed");
		data.depth = replacement;
	}
	if (dispatch_semaphore_wait(data.available, DISPATCH_TIME_NOW) != 0)
		return frame_status::busy;
	@autoreleasepool {
		id<CAMetalDrawable>  drawable = [data.layer nextDrawable];
		id<MTLCommandBuffer> command  = drawable ? [data.state->queue commandBuffer] : nil;
		if (!command ||
		    !detail::batch_access::encode(command, drawable.texture, data.depth, batch)) {
			dispatch_semaphore_signal(data.available);
			if (!drawable)
				return frame_status::unavailable;
			return failure("Metal command encoding failed");
		}
		// Capture shared native state and the semaphore, never the surface owner. Closing a
		// preview cannot leave a callback referencing its destroyed C++ wrapper.
		auto                 state     = data.state;
		dispatch_semaphore_t available = data.available;
		[command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
		  state->completed(completed);
		  dispatch_semaphore_signal(available);
		}];
		[command presentDrawable:drawable];
		[command commit];
		return frame_status::submitted;
	}
}
namespace {
struct metal_readback {
	std::shared_ptr<detail::device_state>                      state;
	std::shared_ptr<ice::render::detail::readback_reservation> reservation;
	id<MTLTexture>                                             color, depth;
	id<MTLBuffer>                                              buffer;
	int                                                        width = 0, height = 0;
	std::size_t                                                stride = 0;
	std::atomic<int>                                           completed{0};
	std::string error = "Metal asynchronous readback failed";
};
class metal_readback_operation final : public ice::render::detail::readback_operation {
	std::shared_ptr<metal_readback> transfer_;
	bool                            cancelled_ = false, consumed_ = false;

public:
	explicit metal_readback_operation(std::shared_ptr<metal_readback> transfer)
	    : transfer_(std::move(transfer)) {}
	ice::expected<readback_status, diagnostic> poll() override {
		if (consumed_)
			return readback_status::consumed;
		const auto done = transfer_->completed.load(std::memory_order_acquire);
		if (!done)
			return readback_status::pending;
		if (cancelled_)
			return readback_status::cancelled;
		if (done < 0)
			return failure(transfer_->error);
		return readback_status::ready;
	}
	ice::expected<readback_pixels, diagnostic> take() override {
		const auto status = poll();
		if (!status)
			return ice::unexpected(status.error());
		if (*status != readback_status::ready)
			return failure("Readback pixels are not ready or already consumed");
		readback_pixels result;
		result.width      = transfer_->width;
		result.height     = transfer_->height;
		result.row_stride = transfer_->stride;
		result.layout     = readback_layout::bgra8;
		result.origin     = readback_origin::top_left;
		result.bytes.resize(result.row_stride * std::size_t(result.height));
		std::memcpy(result.bytes.data(), transfer_->buffer.contents, result.bytes.size());
		ice::render::detail::readback_access::retain(result, std::move(transfer_->reservation));
		transfer_->buffer = nil;
		transfer_->color  = nil;
		transfer_->depth  = nil;
		consumed_         = true;
		return result;
	}
	void cancel() noexcept override { cancelled_ = true; }
};
} // namespace
ice::expected<readback_submission, diagnostic>
device::render_offscreen_async(const frame_batch &batch, int width, int height,
                               readback_limits limits) {
	if (width < 1 || height < 1 || width > 4096 || height > 4096)
		return failure("Invalid asynchronous capture dimensions");
	auto valid = detail::batch_access::validate(state_, batch);
	if (!valid)
		return ice::unexpected(valid.error());
	const auto stride = (std::size_t(width) * 4 + 255) & ~std::size_t(255);
	// Logical peak: color+depth, aligned staging and the extracted CPU copy.
	const auto cost =
	    std::size_t(width) * std::size_t(height) * 8 + stride * std::size_t(height) * 2;
	auto reservation = ice::render::detail::reserve_readback(state_->readbacks, limits, cost);
	if (!reservation)
		return readback_submission{};
	@autoreleasepool {
		auto transfer         = std::make_shared<metal_readback>();
		transfer->state       = state_;
		transfer->reservation = std::move(reservation);
		transfer->width       = width;
		transfer->height      = height;
		transfer->stride      = stride;
		auto *descriptor =
		    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		                                                       width:width
		                                                      height:height
		                                                   mipmapped:NO];
		descriptor.storageMode = MTLStorageModePrivate;
		descriptor.usage       = MTLTextureUsageRenderTarget;
		transfer->color        = [state_->device newTextureWithDescriptor:descriptor];
		transfer->depth        = detail::batch_access::depth_texture(state_, width, height);
		transfer->buffer       = [state_->device newBufferWithLength:stride * std::size_t(height)
                                                       options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> command = [state_->queue commandBuffer];
		if (!transfer->color || !transfer->depth || !transfer->buffer || !command ||
		    !detail::batch_access::encode(command, transfer->color, transfer->depth, batch))
			return failure("Metal asynchronous capture allocation/encoding failed");
		id<MTLBlitCommandEncoder> copy = [command blitCommandEncoder];
		if (!copy)
			return failure("Metal asynchronous readback encoder unavailable");
		[copy copyFromTexture:transfer->color
		                 sourceSlice:0
		                 sourceLevel:0
		                sourceOrigin:MTLOriginMake(0, 0, 0)
		                  sourceSize:MTLSizeMake(width, height, 1)
		                    toBuffer:transfer->buffer
		           destinationOffset:0
		      destinationBytesPerRow:stride
		    destinationBytesPerImage:stride * std::size_t(height)];
		[copy endEncoding];
		readback_submission result{frame_status::submitted,
		                           ice::render::detail::readback_access::ticket(
		                               std::make_unique<metal_readback_operation>(transfer))};
		// The block owns all native resources and admission even if the ticket is dropped.
		[command addCompletedHandler:^(id<MTLCommandBuffer> finished) {
		  if (finished.status != MTLCommandBufferStatusCompleted) {
			  try {
				  transfer->error =
					  native_error(finished.error, "Metal asynchronous readback failed");
			  } catch (...) { /* Keep the preallocated fallback diagnostic. */
			  }
			  transfer->completed.store(-1, std::memory_order_release);
		  } else
			  transfer->completed.store(1, std::memory_order_release);
		}];
		[command commit];
		return result;
	}
}
ice::expected<image, diagnostic> device::render_offscreen(const pipeline &pipeline, int width,
                                                          int                        height,
                                                          std::span<const std::byte> uniforms,
                                                          const texture *sampled_image) {
	const draw_item item{&pipeline, nullptr, uniforms, sampled_image};
	return render_offscreen(frame_batch{std::span(&item, 1)}, width, height);
}
ice::expected<image, diagnostic> device::render_offscreen(const frame_batch &batch, int width,
                                                          int height) {
	if (width < 1 || height < 1 || width > 4096 || height > 4096)
		return failure("Invalid capture dimensions");
	auto valid = detail::batch_access::validate(state_, batch);
	if (!valid)
		return ice::unexpected(valid.error());
	@autoreleasepool {
		MTLTextureDescriptor *descriptor =
		    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		                                                       width:width
		                                                      height:height
		                                                   mipmapped:NO];
		descriptor.storageMode        = MTLStorageModePrivate;
		descriptor.usage              = MTLTextureUsageRenderTarget;
		id<MTLTexture>       target   = [state_->device newTextureWithDescriptor:descriptor];
		id<MTLTexture>       depth    = detail::batch_access::depth_texture(state_, width, height);
		std::size_t          stride   = (std::size_t(width) * 4 + 255) & ~std::size_t(255);
		id<MTLBuffer>        readback = [state_->device newBufferWithLength:stride * height
                                                             options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> command  = [state_->queue commandBuffer];
		if (!target || !depth || !readback || !command ||
		    !detail::batch_access::encode(command, target, depth, batch))
			return failure("Metal capture allocation/encoding failed");
		id<MTLBlitCommandEncoder> copy = [command blitCommandEncoder];
		if (!copy)
			return failure("Metal readback encoder creation failed");
		[copy copyFromTexture:target
		                 sourceSlice:0
		                 sourceLevel:0
		                sourceOrigin:MTLOriginMake(0, 0, 0)
		                  sourceSize:MTLSizeMake(width, height, 1)
		                    toBuffer:readback
		           destinationOffset:0
		      destinationBytesPerRow:stride
		    destinationBytesPerImage:stride * height];
		[copy endEncoding];
		[command commit];
		[command waitUntilCompleted];
		if (command.status != MTLCommandBufferStatusCompleted)
			return failure(native_error(command.error, "Metal readback failed"));
		image result{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
		auto *source = static_cast<const std::uint8_t *>(readback.contents);
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x) {
				auto from = source + y * stride + x * 4;
				auto to   = result.rgba.data() + (std::size_t(y) * width + x) * 4;
				to[0]     = from[2];
				to[1]     = from[1];
				to[2]     = from[0];
				to[3]     = from[3];
			}
		return result;
	}
}
} // namespace ice::render::metal
