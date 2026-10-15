#include "native_context.h"
#import <AppKit/AppKit.h>
#include <dlfcn.h>
namespace ice::render::gl::detail {
namespace {
class appkit_context final : public native_context {
public:
	NSOpenGLContext                *context = nil;
	NSOpenGLPixelFormat            *format  = nil;
	NSOpenGLView                   *view    = nil;
	std::shared_ptr<native_context> shared;
	~appkit_context() override {
		if (NSOpenGLContext.currentContext == context)
			[NSOpenGLContext clearCurrentContext];
		[context clearDrawable];
		[view removeFromSuperview];
	}
	bool make_current() override {
		[context makeCurrentContext];
		return NSOpenGLContext.currentContext == context;
	}
	void resize(int width, int height) override {
		(void)width;
		(void)height;
		[context update];
	}
	bool present() override {
		[context flushBuffer];
		return true;
	}
	raw_proc resolve(const char *name) override {
		return reinterpret_cast<raw_proc>(dlsym(RTLD_DEFAULT, name));
	}
};
} // namespace
ice::expected<std::shared_ptr<native_context>, std::string>
create_context(std::shared_ptr<native_context>            shared,
               const ice::platform::native_window_handle *target) {
	@autoreleasepool {
		auto                         result       = std::make_shared<appkit_context>();
		NSOpenGLPixelFormatAttribute attributes[] = {NSOpenGLPFAOpenGLProfile,
		                                             NSOpenGLProfileVersion4_1Core,
		                                             NSOpenGLPFAAccelerated,
		                                             NSOpenGLPFADoubleBuffer,
		                                             NSOpenGLPFAColorSize,
		                                             24,
		                                             NSOpenGLPFAAlphaSize,
		                                             8,
		                                             0};
		result->format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		if (!result->format)
			return ice::unexpected(std::string("No OpenGL 4.1 core pixel format"));
		auto parent     = std::dynamic_pointer_cast<appkit_context>(shared);
		result->context = [[NSOpenGLContext alloc] initWithFormat:result->format
		                                             shareContext:parent ? parent->context : nil];
		if (!result->context)
			return ice::unexpected(std::string("OpenGL core context creation failed"));
		result->shared = std::move(shared);
		if (target) {
			if (target->kind != ice::platform::native_window_kind::appkit || !target->view)
				return ice::unexpected(std::string("OpenGL presentation requires an AppKit view"));
			NSView *host = (__bridge NSView *)target->view;
			result->view = [[NSOpenGLView alloc] initWithFrame:host.bounds
			                                       pixelFormat:result->format];
			if (!result->view)
				return ice::unexpected(std::string("OpenGL presentation view creation failed"));
			result->view.wantsBestResolutionOpenGLSurface = YES;
			result->view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
			[result->view setOpenGLContext:result->context];
			[host addSubview:result->view];
			GLint interval = 1;
			[result->context setValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
		}
		if (!result->make_current())
			return ice::unexpected(std::string("Cannot make OpenGL context current"));
		return std::shared_ptr<native_context>(std::move(result));
	}
}
} // namespace ice::render::gl::detail
