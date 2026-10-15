#include "ice/render/metal/device.h"
#include "ice/core/utf8.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <TargetConditionals.h>
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
	id<MTLDevice>       device;
	id<MTLCommandQueue> queue;
	std::mutex          mutex;
	std::string         error;
	void                completed(id<MTLCommandBuffer> command) {
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
	std::size_t                           uniform_bytes = 0;
};
struct presentation_surface::implementation {
	// Release exclusivity after native state; both common and backend-specific APIs use it.
	std::unique_ptr<layer_lease>          lease;
	std::shared_ptr<detail::device_state> state;
	CAMetalLayer                         *layer;
	dispatch_semaphore_t                  available = dispatch_semaphore_create(3);
	int                                   width = 0, height = 0;
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
	return state == pipeline_state && uniforms.size() >= required && uniforms.size() <= 4096;
}
id<MTLRenderCommandEncoder> encode(id<MTLCommandBuffer> command, id<MTLTexture> target,
                                   id<MTLRenderPipelineState> pipeline,
                                   std::span<const std::byte> uniforms) {
	MTLRenderPassDescriptor *pass        = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture     = target;
	pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 1);
	id<MTLRenderCommandEncoder> encoder  = [command renderCommandEncoderWithDescriptor:pass];
	if (!encoder)
		return nil;
	[encoder setRenderPipelineState:pipeline];
	if (!uniforms.empty()) {
		[encoder setVertexBytes:uniforms.data() length:uniforms.size() atIndex:0];
		[encoder setFragmentBytes:uniforms.data() length:uniforms.size() atIndex:0];
	}
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[encoder endEncoding];
	return encoder;
}
} // namespace
pipeline::pipeline(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
pipeline::~pipeline() = default;
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
                         const std::string &vertex_entry, const std::string &fragment_entry) {
	@autoreleasepool {
		std::vector<unsigned char> bytes;
		std::string                diagnostics;
		if (!resolver.read(uri, bytes, diagnostics))
			return failure(uri + ": " + diagnostics);
		std::string source(bytes.begin(), bytes.end());
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
		descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		MTLRenderPipelineReflection *reflection    = nil;
		auto                         result        = std::make_unique<pipeline::implementation>();
		result->state                              = state_;
		result->native =
		    [state_->device newRenderPipelineStateWithDescriptor:descriptor
		                                                 options:MTLPipelineOptionBindingInfo
		                                              reflection:&reflection
		                                                   error:&error];
		if (!result->native || !reflection)
			return failure(uri + ": " +
			               native_error(error, "Metal pipeline creation/reflection failed"));
		if (@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)) {
			for (NSArray<id<MTLBinding>> *bindings in
			     @[ reflection.vertexBindings, reflection.fragmentBindings ]) {
				for (id<MTLBinding> binding in bindings) {
					if (!binding.used || !binding.argument)
						continue;
					if (binding.type != MTLBindingTypeBuffer || binding.index != 0 ||
					    binding.access != MTLBindingAccessReadOnly)
						return failure(
						    uri +
						    ": this initial pipeline supports only read-only uniform buffer(0)");
					auto buffer = static_cast<id<MTLBufferBinding>>(binding);
					result->uniform_bytes =
					    std::max(result->uniform_bytes, std::size_t(buffer.bufferDataSize));
				}
			}
		} else
			return failure(uri + ": pipeline reflection requires macOS 13 / iOS 16 or newer");
		if (result->uniform_bytes > 4096)
			return failure(uri + ": uniform block exceeds 4096 bytes");
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<pipeline>(new pipeline(std::move(result)));
	}
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
presentation_surface::draw(const pipeline &pipeline, std::span<const std::byte> uniforms) {
	auto &data     = *implementation_;
	auto &prepared = *pipeline.implementation_;
	if (!valid_draw(data.state, prepared.state, uniforms, prepared.uniform_bytes))
		return failure("Foreign pipeline or invalid uniform byte count");
	if (!data.width || !data.height)
		return frame_status::unavailable;
	if (dispatch_semaphore_wait(data.available, DISPATCH_TIME_NOW) != 0)
		return frame_status::busy;
	@autoreleasepool {
		id<CAMetalDrawable>  drawable = [data.layer nextDrawable];
		id<MTLCommandBuffer> command  = drawable ? [data.state->queue commandBuffer] : nil;
		if (!command || !encode(command, drawable.texture, prepared.native, uniforms)) {
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
ice::expected<image, diagnostic> device::render_offscreen(const pipeline &pipeline, int width,
                                                          int                        height,
                                                          std::span<const std::byte> uniforms) {
	auto &prepared = *pipeline.implementation_;
	if (width < 1 || height < 1 || width > 4096 || height > 4096 ||
	    !valid_draw(state_, prepared.state, uniforms, prepared.uniform_bytes))
		return failure("Invalid capture dimensions, foreign pipeline or uniform byte count");
	@autoreleasepool {
		MTLTextureDescriptor *descriptor =
		    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		                                                       width:width
		                                                      height:height
		                                                   mipmapped:NO];
		descriptor.storageMode        = MTLStorageModePrivate;
		descriptor.usage              = MTLTextureUsageRenderTarget;
		id<MTLTexture>       target   = [state_->device newTextureWithDescriptor:descriptor];
		std::size_t          stride   = (std::size_t(width) * 4 + 255) & ~std::size_t(255);
		id<MTLBuffer>        readback = [state_->device newBufferWithLength:stride * height
                                                             options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> command  = [state_->queue commandBuffer];
		if (!target || !readback || !command || !encode(command, target, prepared.native, uniforms))
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
