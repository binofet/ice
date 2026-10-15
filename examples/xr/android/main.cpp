#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../debug_trace.h"
#include "../space_anchor.h"
#include "ice/math/types.h"
#include "ice/platform/host.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
ice::examples::xr::debug_trace trace;

__attribute__((format(printf, 2, 3))) void log_message(int priority, const char *format, ...) {
	std::array<char, 1024> message{};
	va_list                arguments;
	va_start(arguments, format);
	std::vsnprintf(message.data(), message.size(), format, arguments);
	va_end(arguments);
	__android_log_write(priority, "ICE-XR", message.data());
	trace.write(priority == ANDROID_LOG_ERROR ? 'E' : 'I', message.data());
}
#define XR_LOG(...) log_message(ANDROID_LOG_INFO, __VA_ARGS__)
#define XR_ERROR(...) log_message(ANDROID_LOG_ERROR, __VA_ARGS__)

const char *command_name(std::int32_t command) {
	switch (command) {
	case APP_CMD_START:
		return "START";
	case APP_CMD_RESUME:
		return "RESUME";
	case APP_CMD_PAUSE:
		return "PAUSE";
	case APP_CMD_STOP:
		return "STOP";
	case APP_CMD_INIT_WINDOW:
		return "INIT_WINDOW";
	case APP_CMD_TERM_WINDOW:
		return "TERM_WINDOW";
	case APP_CMD_GAINED_FOCUS:
		return "GAINED_FOCUS";
	case APP_CMD_LOST_FOCUS:
		return "LOST_FOCUS";
	case APP_CMD_DESTROY:
		return "DESTROY";
	default:
		return "OTHER";
	}
}

template <typename T> T xr_structure(XrStructureType type) {
	T value{};
	value.type = type;
	return value;
}
class lifecycle_client final : public ice::platform::host_client {
public:
	void on_app_state(ice::platform::app_state state) override {
		XR_LOG("ICE host lifecycle=%d", static_cast<int>(state));
	}
};
struct controller_state {
	XrSpace space = XR_NULL_HANDLE;
	XrPath  hand  = XR_NULL_PATH;
	XrPosef pose{};
	float   trigger = 0;
	bool    valid   = false;
};
struct eye_target {
	XrSwapchain                              swapchain = XR_NULL_HANDLE;
	std::vector<XrSwapchainImageOpenGLESKHR> images;
	std::uint32_t                            width = 0, height = 0;
	GLuint                                   framebuffer = 0, depth = 0;
	std::uint32_t                            acquired_image = 0;
	bool                                     acquired = false, waited = false;
};
enum class image_availability { ready, waiting, failed };

// An intentionally small, separate GLES3 probe proves native OpenXR ownership and tracking
// before these contracts join the portable renderer. It does not reuse desktop GL contexts.
class xr_probe {
public:
	explicit xr_probe(android_app &app) : app_(app), host_(client_) {}
	~xr_probe() {
		XR_LOG("Probe teardown begins: running=%d session_state=%d frames=%llu", running_,
		       static_cast<int>(state_), static_cast<unsigned long long>(submitted_frames_));
		shutdown_session();
		host_.stop();
		if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT &&
		    eglMakeCurrent(display_, pbuffer_, pbuffer_, context_)) {
			glFinish();
			for (auto &eye : eyes_) {
				if (eye.framebuffer)
					glDeleteFramebuffers(1, &eye.framebuffer);
				if (eye.depth)
					glDeleteRenderbuffers(1, &eye.depth);
			}
			if (program_)
				glDeleteProgram(program_);
			if (vertices_)
				glDeleteBuffers(1, &vertices_);
			if (vao_)
				glDeleteVertexArrays(1, &vao_);
		}
		for (auto &eye : eyes_)
			if (eye.swapchain)
				xrDestroySwapchain(eye.swapchain);
		for (auto &controller : controllers_)
			if (controller.space)
				xrDestroySpace(controller.space);
		if (space_)
			xrDestroySpace(space_);
		if (session_)
			xrDestroySession(session_);
		if (controller_actions_)
			xrDestroyActionSet(controller_actions_);
		if (instance_)
			xrDestroyInstance(instance_);
		if (display_ != EGL_NO_DISPLAY) {
			eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if (context_ != EGL_NO_CONTEXT)
				eglDestroyContext(display_, context_);
			if (pbuffer_ != EGL_NO_SURFACE)
				eglDestroySurface(display_, pbuffer_);
			eglTerminate(display_);
		}
		if (attached_thread_)
			app_.activity->vm->DetachCurrentThread();
		XR_LOG("Probe teardown complete");
	}
	bool initialize() {
		JNIEnv *environment = nullptr;
		auto   *vm          = app_.activity->vm;
		if (vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) == JNI_EDETACHED) {
			if (vm->AttachCurrentThread(&environment, nullptr) != JNI_OK)
				return fail("AttachCurrentThread failed");
			attached_thread_ = true;
		}
		PFN_xrInitializeLoaderKHR initialize_loader = nullptr;
		if (!check(
		        xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
		                              reinterpret_cast<PFN_xrVoidFunction *>(&initialize_loader)),
		        "resolve Android loader initialization") ||
		    !initialize_loader)
			return false;
		auto loader =
		    xr_structure<XrLoaderInitInfoAndroidKHR>(XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR);
		loader.applicationVM      = vm;
		loader.applicationContext = app_.activity->clazz;
		if (!check(
		        initialize_loader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR *>(&loader)),
		        "initialize Android loader"))
			return false;
		std::uint32_t extension_count = 0;
		if (!check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extension_count, nullptr),
		           "enumerate extensions") ||
		    extension_count > 1024)
			return false;
		std::vector<XrExtensionProperties> extensions(
		    extension_count, xr_structure<XrExtensionProperties>(XR_TYPE_EXTENSION_PROPERTIES));
		if (!check(xrEnumerateInstanceExtensionProperties(nullptr, extension_count,
		                                                  &extension_count, extensions.data()),
		           "read extensions"))
			return false;
		std::array<const char *, 2> required{XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
		                                     XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
		for (const auto *name : required)
			if (std::none_of(extensions.begin(), extensions.end(), [name](const auto &extension) {
				    return std::strcmp(name, extension.extensionName) == 0;
			    }))
				return fail(std::string("Required OpenXR extension is missing: ") + name);
		auto android =
		    xr_structure<XrInstanceCreateInfoAndroidKHR>(XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR);
		android.applicationVM       = vm;
		android.applicationActivity = app_.activity->clazz;
		auto create = xr_structure<XrInstanceCreateInfo>(XR_TYPE_INSTANCE_CREATE_INFO);
		create.next = &android;
		std::snprintf(create.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "%s",
		              "ICE XR Lab");
		std::snprintf(create.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "%s", "ICE");
		create.applicationInfo.applicationVersion = 1;
		create.applicationInfo.engineVersion      = 1;
		create.applicationInfo.apiVersion         = XR_MAKE_VERSION(1, 0, 0);
		create.enabledExtensionCount              = std::uint32_t(required.size());
		create.enabledExtensionNames              = required.data();
		if (!check(xrCreateInstance(&create, &instance_), "create instance"))
			return false;
		auto runtime = xr_structure<XrInstanceProperties>(XR_TYPE_INSTANCE_PROPERTIES);
		if (!check(xrGetInstanceProperties(instance_, &runtime), "runtime properties"))
			return false;
		XR_LOG("Runtime: %s version=%llu", runtime.runtimeName,
		       static_cast<unsigned long long>(runtime.runtimeVersion));
		auto system       = xr_structure<XrSystemGetInfo>(XR_TYPE_SYSTEM_GET_INFO);
		system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
		if (!check(xrGetSystem(instance_, &system, &system_), "find headset system"))
			return false;
		auto properties = xr_structure<XrSystemProperties>(XR_TYPE_SYSTEM_PROPERTIES);
		if (!check(xrGetSystemProperties(instance_, system_, &properties), "system properties"))
			return false;
		XR_LOG("Headset: %s orientation=%u position=%u", properties.systemName,
		       properties.trackingProperties.orientationTracking,
		       properties.trackingProperties.positionTracking);
		PFN_xrGetOpenGLESGraphicsRequirementsKHR graphics_requirements = nullptr;
		if (!check(xrGetInstanceProcAddr(
		               instance_, "xrGetOpenGLESGraphicsRequirementsKHR",
		               reinterpret_cast<PFN_xrVoidFunction *>(&graphics_requirements)),
		           "resolve GLES requirements") ||
		    !graphics_requirements)
			return false;
		auto requirements = xr_structure<XrGraphicsRequirementsOpenGLESKHR>(
		    XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR);
		if (!check(graphics_requirements(instance_, system_, &requirements),
		           "query GLES requirements") ||
		    !initialize_graphics())
			return false;
		GLint major = 0, minor = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
		auto version = XR_MAKE_VERSION(major, minor, 0);
		if (version < requirements.minApiVersionSupported ||
		    version > requirements.maxApiVersionSupported)
			return fail("GLES context does not meet runtime requirements");
		XR_LOG("Graphics: %s / %s", reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
		       reinterpret_cast<const char *>(glGetString(GL_VERSION)));
		auto binding = xr_structure<XrGraphicsBindingOpenGLESAndroidKHR>(
		    XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR);
		binding.display         = display_;
		binding.config          = config_;
		binding.context         = context_;
		auto session_create     = xr_structure<XrSessionCreateInfo>(XR_TYPE_SESSION_CREATE_INFO);
		session_create.next     = &binding;
		session_create.systemId = system_;
		if (!check(xrCreateSession(instance_, &session_create, &session_), "create session"))
			return false;
		std::uint32_t blend_count = 0;
		if (!check(xrEnumerateEnvironmentBlendModes(instance_, system_,
		                                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
		                                            &blend_count, nullptr),
		           "enumerate blend modes") ||
		    blend_count > 32)
			return false;
		std::vector<XrEnvironmentBlendMode> blends(blend_count);
		if (!check(xrEnumerateEnvironmentBlendModes(instance_, system_,
		                                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
		                                            blend_count, &blend_count, blends.data()),
		           "read blend modes"))
			return false;
		if (std::find(blends.begin(), blends.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE) ==
		    blends.end())
			return fail("This room probe requires an opaque stereo blend mode");

		std::uint32_t space_count = 0;
		if (!check(xrEnumerateReferenceSpaces(session_, 0, &space_count, nullptr),
		           "enumerate reference spaces") ||
		    space_count > 32)
			return false;
		std::vector<XrReferenceSpaceType> spaces(space_count);
		if (!check(xrEnumerateReferenceSpaces(session_, space_count, &space_count, spaces.data()),
		           "read reference spaces"))
			return false;
		bool stage =
		    std::find(spaces.begin(), spaces.end(), XR_REFERENCE_SPACE_TYPE_STAGE) != spaces.end();
		auto space_create =
		    xr_structure<XrReferenceSpaceCreateInfo>(XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
		space_create.referenceSpaceType =
		    stage ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
		space_type_                                     = space_create.referenceSpaceType;
		space_create.poseInReferenceSpace.orientation.w = 1;
		floor_height_                                   = stage ? 0.f : -1.5f;
		if (!check(xrCreateReferenceSpace(session_, &space_create, &space_),
		           "create world reference space"))
			return false;
		XR_LOG("World reference: %s",
		       stage ? "stage/floor" : "local/head origin with synthetic floor at -1.5m");
		return initialize_controllers() && initialize_swapchains();
	}
	void application_command(std::int32_t command) {
		XR_LOG("Android command=%s(%d) activity_state=%d window=%d", command_name(command), command,
		       app_.activityState, app_.window != nullptr);
		if (command == APP_CMD_RESUME)
			resumed_ = true;
		if (command == APP_CMD_PAUSE)
			resumed_ = false;
		update_lifecycle();
	}
	bool running() const { return running_; }
	bool finished() const { return finished_; }
	void idle_heartbeat() {
		const auto now = std::chrono::steady_clock::now();
		if (running_ || now < next_heartbeat_)
			return;
		next_heartbeat_ = now + std::chrono::seconds(5);
		XR_LOG("Waiting: session_state=%d android_resumed=%d activity_state=%d window=%d "
		       "submitted_frames=%llu",
		       static_cast<int>(state_), resumed_, app_.activityState, app_.window != nullptr,
		       static_cast<unsigned long long>(submitted_frames_));
	}
	bool poll_events() {
		for (;;) {
			auto event  = xr_structure<XrEventDataBuffer>(XR_TYPE_EVENT_DATA_BUFFER);
			auto result = xrPollEvent(instance_, &event);
			if (result == XR_EVENT_UNAVAILABLE)
				return true;
			if (!check(result, "poll runtime events"))
				return false;
			if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
				runtime_lost_ = true;
				running_      = false;
				finished_     = true;
				return true;
			}
			if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
				const auto &change =
				    *reinterpret_cast<const XrEventDataReferenceSpaceChangePending *>(&event);
				if (change.session == session_ && change.referenceSpaceType == space_type_) {
					glm::mat4 old_from_new(1);
					if (change.poseValid) {
						const auto &pose = change.poseInPreviousSpace;
						old_from_new =
						    glm::translate(glm::mat4(1), glm::vec3(pose.position.x, pose.position.y,
						                                           pose.position.z)) *
						    glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x,
						                             pose.orientation.y, pose.orientation.z));
						XR_LOG("Reference origin change time=%lld valid=1 old_from_new "
						       "position=(%.4f,%.4f,%.4f) orientation=(%.4f,%.4f,%.4f,%.4f)",
						       static_cast<long long>(change.changeTime), pose.position.x,
						       pose.position.y, pose.position.z, pose.orientation.x,
						       pose.orientation.y, pose.orientation.z, pose.orientation.w);
					} else {
						XR_LOG("Reference origin change time=%lld valid=0; physical continuity "
						       "cannot be recovered from this event",
						       static_cast<long long>(change.changeTime));
					}
					if (!anchor_.queue_change(change.changeTime, change.poseValid, old_from_new))
						return fail("Too many pending reference-space changes");
				}
				continue;
			}
			if (event.type != XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
				continue;
			auto &change = *reinterpret_cast<const XrEventDataSessionStateChanged *>(&event);
			if (change.session != session_)
				continue;
			state_ = change.state;
			XR_LOG("OpenXR session state=%d", static_cast<int>(state_));
			if (state_ == XR_SESSION_STATE_READY && !shutting_down_) {
				auto begin = xr_structure<XrSessionBeginInfo>(XR_TYPE_SESSION_BEGIN_INFO);
				begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				if (!check(xrBeginSession(session_, &begin), "begin session"))
					return false;
				running_ = true;
			} else if (state_ == XR_SESSION_STATE_STOPPING) {
				release_owned_images();
				if (!check(xrEndSession(session_), "end session"))
					return false;
				running_ = false;
			} else if (state_ == XR_SESSION_STATE_EXITING ||
			           state_ == XR_SESSION_STATE_LOSS_PENDING) {
				finished_     = true;
				running_      = false;
				runtime_lost_ = state_ == XR_SESSION_STATE_LOSS_PENDING;
			}
			update_lifecycle();
		}
	}
	bool frame() {
		auto wait   = xr_structure<XrFrameWaitInfo>(XR_TYPE_FRAME_WAIT_INFO);
		auto timing = xr_structure<XrFrameState>(XR_TYPE_FRAME_STATE);
		if (!check(xrWaitFrame(session_, &wait, &timing), "wait frame"))
			return false;
		auto begin = xr_structure<XrFrameBeginInfo>(XR_TYPE_FRAME_BEGIN_INFO);
		if (!check(xrBeginFrame(session_, &begin), "begin frame"))
			return false;
		const auto origin_changes = anchor_.advance(timing.predictedDisplayTime);
		if (origin_changes.applied)
			XR_LOG("Room physical placement preserved across %zu reference-origin changes",
			       origin_changes.applied);
		if (origin_changes.unknown)
			XR_LOG("Room coordinates retained across %zu unknown-origin changes; physical "
			       "continuity is unavailable",
			       origin_changes.unknown);
		std::array<XrCompositionLayerProjectionView, 2> projection_views{};
		bool                                            render_ok = true, submitted = false;
		render_ok = update_controllers(timing.predictedDisplayTime);
		if (timing.shouldRender && render_ok) {
			auto locate                  = xr_structure<XrViewLocateInfo>(XR_TYPE_VIEW_LOCATE_INFO);
			locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			locate.displayTime           = timing.predictedDisplayTime;
			locate.space                 = space_;
			auto                  tracking = xr_structure<XrViewState>(XR_TYPE_VIEW_STATE);
			std::array<XrView, 2> views{xr_structure<XrView>(XR_TYPE_VIEW),
			                            xr_structure<XrView>(XR_TYPE_VIEW)};
			std::uint32_t         count = 0;
			render_ok = check(xrLocateViews(session_, &locate, &tracking, 2, &count, views.data()),
			                  "locate stereo views") &&
			            count == 2;
			constexpr XrViewStateFlags valid =
			    XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
			constexpr XrViewStateFlags tracked =
			    valid | XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
			if (render_ok && (tracking.viewStateFlags & valid) == valid) {
				if (!anchor_.ready() && (tracking.viewStateFlags & tracked) == tracked)
					anchor_scene(views);
				submitted = anchor_.ready();
				// Acquire/wait both eyes before drawing either. A timed-out image stays owned
				// until a later wait succeeds; no duplicate acquire or premature release occurs.
				for (auto &target : eyes_) {
					auto availability = prepare_eye(target);
					if (availability == image_availability::failed) {
						render_ok = false;
						submitted = false;
						break;
					}
					if (availability != image_availability::ready)
						submitted = false;
				}
				for (std::size_t eye = 0; eye < eyes_.size() && submitted; ++eye) {
					if (!render_eye(eyes_[eye], views[eye],
					                double(timing.predictedDisplayTime) * 1e-9)) {
						render_ok = false;
						submitted = false;
						break;
					}
					projection_views[eye] = xr_structure<XrCompositionLayerProjectionView>(
					    XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
					projection_views[eye].pose                      = views[eye].pose;
					projection_views[eye].fov                       = views[eye].fov;
					projection_views[eye].subImage.swapchain        = eyes_[eye].swapchain;
					projection_views[eye].subImage.imageRect.extent = {
					    std::int32_t(eyes_[eye].width), std::int32_t(eyes_[eye].height)};
				}
				if (submitted && (submitted_frames_ == 0 || submitted_frames_ % 120 == 0)) {
					const auto &head = views[0].pose.position;
					XR_LOG(
					    "Stereo frame=%llu views=2 flags=%llu position=(%.3f,%.3f,%.3f) focused=%d",
					    static_cast<unsigned long long>(frames_),
					    static_cast<unsigned long long>(tracking.viewStateFlags), head.x, head.y,
					    head.z, state_ == XR_SESSION_STATE_FOCUSED);
					XR_LOG("Controllers: left=%d trigger=%.2f right=%d trigger=%.2f",
					       controllers_[0].valid, controllers_[0].trigger, controllers_[1].valid,
					       controllers_[1].trigger);
				}
			}
		}
		if (submitted)
			++submitted_frames_;
		auto layer =
		    xr_structure<XrCompositionLayerProjection>(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
		layer.space              = space_;
		layer.viewCount          = std::uint32_t(projection_views.size());
		layer.views              = projection_views.data();
		const auto *layer_header = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);
		auto        end          = xr_structure<XrFrameEndInfo>(XR_TYPE_FRAME_END_INFO);
		end.displayTime          = timing.predictedDisplayTime;
		end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		end.layerCount           = submitted ? 1 : 0;
		end.layers               = submitted ? &layer_header : nullptr;
		bool ended               = !runtime_lost_ && check(xrEndFrame(session_, &end), "end frame");
		++frames_;
		auto now =
		    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
		        .count();
		host_.frame(now);
		return render_ok && ended;
	}

private:
	void release_owned_images() {
		for (auto &eye : eyes_) {
			if (!eye.acquired)
				continue;
			if (!eye.waited) {
				auto wait =
				    xr_structure<XrSwapchainImageWaitInfo>(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
				wait.timeout = 0;
				auto result  = xrWaitSwapchainImage(eye.swapchain, &wait);
				if (result == XR_TIMEOUT_EXPIRED || XR_FAILED(result))
					continue;
				eye.waited = true;
			}
			auto release =
			    xr_structure<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
			if (XR_SUCCEEDED(xrReleaseSwapchainImage(eye.swapchain, &release))) {
				eye.acquired = false;
				eye.waited   = false;
			}
		}
	}
	void shutdown_session() {
		shutting_down_ = true;
		if (!session_ || !running_ || runtime_lost_)
			return;
		if (!check(xrRequestExitSession(session_), "request orderly session exit"))
			return;
		XR_LOG("Orderly session exit requested");
		// NativeActivity teardown must not wait forever for a broken/disconnected runtime.
		// Normal exit observes STOPPING and calls xrEndSession; xrDestroySession is valid
		// in any state and remains the bounded fallback after loss or missing stop events.
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (running_ && !runtime_lost_ && std::chrono::steady_clock::now() < deadline) {
			release_owned_images();
			if (!poll_events())
				break;
			if (running_)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		if (running_)
			XR_LOG("Orderly stop deadline reached; destroying session without active API calls");
		else
			XR_LOG("Orderly session stop completed");
	}
	image_availability prepare_eye(eye_target &eye) {
		if (!eye.acquired) {
			auto acquire =
			    xr_structure<XrSwapchainImageAcquireInfo>(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO);
			if (!check(xrAcquireSwapchainImage(eye.swapchain, &acquire, &eye.acquired_image),
			           "acquire eye image"))
				return image_availability::failed;
			eye.acquired = true;
			if (eye.acquired_image >= eye.images.size()) {
				fail("Runtime returned an out-of-range eye image");
				return image_availability::failed;
			}
		}
		if (!eye.waited) {
			auto wait = xr_structure<XrSwapchainImageWaitInfo>(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
			wait.timeout = 0;
			auto result  = xrWaitSwapchainImage(eye.swapchain, &wait);
			// Timeout is a successful availability result, not an error or a release permit.
			// Keep this exact image acquired and retry it during a later paced XR frame.
			if (result == XR_TIMEOUT_EXPIRED)
				return image_availability::waiting;
			if (!check(result, "wait eye image"))
				return image_availability::failed;
			eye.waited = true;
		}
		return image_availability::ready;
	}

	bool fail(const std::string &message) {
		XR_ERROR("%s", message.c_str());
		return false;
	}
	bool check(XrResult result, const char *operation) {
		if (XR_SUCCEEDED(result))
			return true;
		if (result == XR_ERROR_INSTANCE_LOST || result == XR_ERROR_SESSION_LOST) {
			runtime_lost_ = true;
			finished_     = true;
			running_      = false;
		}
		char description[XR_MAX_RESULT_STRING_SIZE]{};
		if (instance_ && result != XR_ERROR_INSTANCE_LOST)
			xrResultToString(instance_, result, description);
		XR_ERROR("%s failed: %d %s", operation, int(result), description);
		return false;
	}
	void update_lifecycle() {
		using ice::platform::app_state;
		host_.set_state(!resumed_                            ? app_state::background
		                : state_ == XR_SESSION_STATE_FOCUSED ? app_state::active
		                                                     : app_state::inactive);
	}
	bool initialize_graphics() {
		display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr) ||
		    !eglBindAPI(EGL_OPENGL_ES_API))
			return fail("Cannot initialize Android EGL");
		const EGLint attributes[] = {EGL_RENDERABLE_TYPE,
		                             EGL_OPENGL_ES3_BIT_KHR,
		                             EGL_SURFACE_TYPE,
		                             EGL_PBUFFER_BIT,
		                             EGL_RED_SIZE,
		                             8,
		                             EGL_GREEN_SIZE,
		                             8,
		                             EGL_BLUE_SIZE,
		                             8,
		                             EGL_ALPHA_SIZE,
		                             8,
		                             EGL_DEPTH_SIZE,
		                             24,
		                             EGL_NONE};
		EGLint       count        = 0;
		if (!eglChooseConfig(display_, attributes, &config_, 1, &count) || count != 1)
			return fail("No Android GLES3 pbuffer configuration");
		// The compositor owns the headset images. EGL needs only a tiny preparation
		// pbuffer; no Android activity window is mistaken for either stereo eye.
		const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
		context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, context_attributes);
		const EGLint dimensions[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
		pbuffer_                  = eglCreatePbufferSurface(display_, config_, dimensions);
		if (context_ == EGL_NO_CONTEXT || pbuffer_ == EGL_NO_SURFACE ||
		    !eglMakeCurrent(display_, pbuffer_, pbuffer_, context_))
			return fail("Cannot create/current Android GLES3 context");
		constexpr const char *vertex_source   = R"(#version 300 es
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
uniform mat4 mvp;
uniform mat4 model;
out vec3 world_normal;
void main(){ gl_Position=mvp*vec4(position,1); world_normal=mat3(transpose(inverse(model)))*normal; }
)";
		constexpr const char *fragment_source = R"(#version 300 es
precision highp float;
in vec3 world_normal;
uniform vec3 tint;
out vec4 color;
void main(){ float light=.25+.75*max(dot(normalize(world_normal),normalize(vec3(.3,1,.5))),0.0); color=vec4(tint*light,1); }
)";
		auto                  compile         = [this](GLenum stage, const char *source) {
            GLuint shader = glCreateShader(stage);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint ready = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ready);
            if (!ready) {
                char log[2048]{};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                fail(log);
                glDeleteShader(shader);
                return GLuint(0);
            }
            return shader;
		};
		GLuint vertex   = compile(GL_VERTEX_SHADER, vertex_source),
		       fragment = compile(GL_FRAGMENT_SHADER, fragment_source);
		if (!vertex || !fragment) {
			if (vertex)
				glDeleteShader(vertex);
			if (fragment)
				glDeleteShader(fragment);
			return false;
		}
		program_ = glCreateProgram();
		glAttachShader(program_, vertex);
		glAttachShader(program_, fragment);
		glLinkProgram(program_);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		GLint linked = 0;
		glGetProgramiv(program_, GL_LINK_STATUS, &linked);
		if (!linked) {
			char log[2048]{};
			glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
			return fail(log);
		}
		mvp_location_   = glGetUniformLocation(program_, "mvp");
		model_location_ = glGetUniformLocation(program_, "model");
		tint_location_  = glGetUniformLocation(program_, "tint");
		const std::array<glm::vec3, 8>          corners{{{-.5f, -.5f, -.5f},
		                                                 {.5f, -.5f, -.5f},
		                                                 {.5f, .5f, -.5f},
		                                                 {-.5f, .5f, -.5f},
		                                                 {-.5f, -.5f, .5f},
		                                                 {.5f, -.5f, .5f},
		                                                 {.5f, .5f, .5f},
		                                                 {-.5f, .5f, .5f}}};
		const std::array<std::array<int, 4>, 6> faces{
		    {{4, 5, 6, 7}, {1, 0, 3, 2}, {0, 4, 7, 3}, {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}}};
		const std::array<glm::vec3, 6> normals{
		    {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
		std::vector<float> vertices;
		vertices.reserve(36 * 6);
		for (std::size_t face = 0; face < faces.size(); ++face)
			for (int index : {0, 1, 2, 0, 2, 3}) {
				const auto &point  = corners[faces[face][index]];
				const auto &normal = normals[face];
				vertices.insert(vertices.end(),
				                {point.x, point.y, point.z, normal.x, normal.y, normal.z});
			}
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);
		glGenBuffers(1, &vertices_);
		glBindBuffer(GL_ARRAY_BUFFER, vertices_);
		glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(float)), vertices.data(),
		             GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		                      reinterpret_cast<void *>(3 * sizeof(float)));
		return glGetError() == GL_NO_ERROR || fail("GLES scene preparation failed");
	}
	bool initialize_controllers() {
		auto set = xr_structure<XrActionSetCreateInfo>(XR_TYPE_ACTION_SET_CREATE_INFO);
		std::snprintf(set.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE, "%s", "controllers");
		std::snprintf(set.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE, "%s",
		              "ICE XR Controllers");
		if (!check(xrCreateActionSet(instance_, &set, &controller_actions_),
		           "create controller action set"))
			return false;
		if (!check(xrStringToPath(instance_, "/user/hand/left", &controllers_[0].hand),
		           "left hand path") ||
		    !check(xrStringToPath(instance_, "/user/hand/right", &controllers_[1].hand),
		           "right hand path"))
			return false;
		std::array<XrPath, 2> hands{controllers_[0].hand, controllers_[1].hand};
		auto                  action = xr_structure<XrActionCreateInfo>(XR_TYPE_ACTION_CREATE_INFO);
		action.actionType            = XR_ACTION_TYPE_POSE_INPUT;
		action.countSubactionPaths   = std::uint32_t(hands.size());
		action.subactionPaths        = hands.data();
		std::snprintf(action.actionName, XR_MAX_ACTION_NAME_SIZE, "%s", "grip_pose");
		std::snprintf(action.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%s",
		              "Controller Grip");
		if (!check(xrCreateAction(controller_actions_, &action, &grip_action_),
		           "create grip action"))
			return false;
		action.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		std::snprintf(action.actionName, XR_MAX_ACTION_NAME_SIZE, "%s", "trigger");
		std::snprintf(action.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%s",
		              "Trigger Pressure");
		if (!check(xrCreateAction(controller_actions_, &action, &trigger_action_),
		           "create trigger action"))
			return false;
		std::array<XrActionSuggestedBinding, 4> bindings{};
		const std::array<const char *, 4>       paths{
            "/user/hand/left/input/grip/pose", "/user/hand/right/input/grip/pose",
            "/user/hand/left/input/trigger/value", "/user/hand/right/input/trigger/value"};
		for (std::size_t index = 0; index < bindings.size(); ++index) {
			bindings[index].action = index < 2 ? grip_action_ : trigger_action_;
			if (!check(xrStringToPath(instance_, paths[index], &bindings[index].binding),
			           "controller binding path"))
				return false;
		}
		auto suggested = xr_structure<XrInteractionProfileSuggestedBinding>(
		    XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING);
		if (!check(xrStringToPath(instance_, "/interaction_profiles/oculus/touch_controller",
		                          &suggested.interactionProfile),
		           "Touch controller profile"))
			return false;
		suggested.countSuggestedBindings = std::uint32_t(bindings.size());
		suggested.suggestedBindings      = bindings.data();
		if (XR_FAILED(xrSuggestInteractionProfileBindings(instance_, &suggested)))
			XR_LOG("Touch controller bindings unavailable; stereo scene remains usable without "
			       "controllers");
		auto attach =
		    xr_structure<XrSessionActionSetsAttachInfo>(XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO);
		attach.countActionSets = 1;
		attach.actionSets      = &controller_actions_;
		if (!check(xrAttachSessionActionSets(session_, &attach), "attach controller actions"))
			return false;
		for (auto &controller : controllers_) {
			auto create   = xr_structure<XrActionSpaceCreateInfo>(XR_TYPE_ACTION_SPACE_CREATE_INFO);
			create.action = grip_action_;
			create.subactionPath                   = controller.hand;
			create.poseInActionSpace.orientation.w = 1;
			if (!check(xrCreateActionSpace(session_, &create, &controller.space),
			           "create controller grip space"))
				return false;
		}
		return true;
	}
	bool update_controllers(XrTime display_time) {
		for (auto &controller : controllers_) {
			controller.valid   = false;
			controller.trigger = 0;
		}
		if (state_ != XR_SESSION_STATE_FOCUSED)
			return true;
		XrActiveActionSet active{};
		active.actionSet           = controller_actions_;
		auto sync                  = xr_structure<XrActionsSyncInfo>(XR_TYPE_ACTIONS_SYNC_INFO);
		sync.countActiveActionSets = 1;
		sync.activeActionSets      = &active;
		auto synchronized          = xrSyncActions(session_, &sync);
		if (synchronized == XR_SESSION_NOT_FOCUSED)
			return true;
		if (!check(synchronized, "sync controller actions"))
			return false;
		for (auto &controller : controllers_) {
			auto get          = xr_structure<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
			get.action        = grip_action_;
			get.subactionPath = controller.hand;
			auto pose         = xr_structure<XrActionStatePose>(XR_TYPE_ACTION_STATE_POSE);
			if (!check(xrGetActionStatePose(session_, &get, &pose), "read grip action"))
				return false;
			if (!pose.isActive)
				continue;
			auto location = xr_structure<XrSpaceLocation>(XR_TYPE_SPACE_LOCATION);
			if (!check(xrLocateSpace(controller.space, space_, display_time, &location),
			           "locate controller grip"))
				return false;
			constexpr XrSpaceLocationFlags valid =
			    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
			controller.valid = (location.locationFlags & valid) == valid;
			controller.pose  = location.pose;
			get.action       = trigger_action_;
			auto trigger     = xr_structure<XrActionStateFloat>(XR_TYPE_ACTION_STATE_FLOAT);
			if (!check(xrGetActionStateFloat(session_, &get, &trigger), "read trigger action"))
				return false;
			if (trigger.isActive && std::isfinite(trigger.currentState))
				controller.trigger = std::clamp(trigger.currentState, 0.f, 1.f);
		}
		return true;
	}

	bool initialize_swapchains() {
		std::uint32_t count = 0;
		if (!check(xrEnumerateViewConfigurationViews(instance_, system_,
		                                             XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
		                                             &count, nullptr),
		           "enumerate stereo configuration") ||
		    count != 2)
			return fail("Exactly two stereo views are required by this probe");
		std::array<XrViewConfigurationView, 2> views{
		    xr_structure<XrViewConfigurationView>(XR_TYPE_VIEW_CONFIGURATION_VIEW),
		    xr_structure<XrViewConfigurationView>(XR_TYPE_VIEW_CONFIGURATION_VIEW)};
		if (!check(xrEnumerateViewConfigurationViews(instance_, system_,
		                                             XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2,
		                                             &count, views.data()),
		           "read stereo configuration"))
			return false;
		std::uint32_t format_count = 0;
		if (!check(xrEnumerateSwapchainFormats(session_, 0, &format_count, nullptr),
		           "enumerate swapchain formats") ||
		    format_count > 128)
			return false;
		std::vector<std::int64_t> formats(format_count);
		if (!check(
		        xrEnumerateSwapchainFormats(session_, format_count, &format_count, formats.data()),
		        "read swapchain formats"))
			return false;
		std::int64_t format =
		    std::find(formats.begin(), formats.end(), GL_SRGB8_ALPHA8) != formats.end()
		        ? GL_SRGB8_ALPHA8
		        : GL_RGBA8;
		if (std::find(formats.begin(), formats.end(), format) == formats.end())
			return fail("Runtime exposes no supported RGBA8 swapchain format");
		for (std::size_t index = 0; index < eyes_.size(); ++index) {
			auto &eye  = eyes_[index];
			eye.width  = std::min(views[index].recommendedImageRectWidth, 1600u);
			eye.height = std::min(views[index].recommendedImageRectHeight, 1600u);
			if (!eye.width || !eye.height)
				return fail("Runtime recommended zero-size eye images");
			auto create = xr_structure<XrSwapchainCreateInfo>(XR_TYPE_SWAPCHAIN_CREATE_INFO);
			create.usageFlags =
			    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
			create.format      = format;
			create.sampleCount = 1;
			create.width       = eye.width;
			create.height      = eye.height;
			create.faceCount   = 1;
			create.arraySize   = 1;
			create.mipCount    = 1;
			if (!check(xrCreateSwapchain(session_, &create, &eye.swapchain),
			           "create eye swapchain"))
				return false;
			std::uint32_t image_count = 0;
			if (!check(xrEnumerateSwapchainImages(eye.swapchain, 0, &image_count, nullptr),
			           "enumerate eye images") ||
			    !image_count || image_count > 8)
				return fail("Swapchain image count exceeds probe budget");
			eye.images.assign(image_count, xr_structure<XrSwapchainImageOpenGLESKHR>(
			                                   XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR));
			if (!check(xrEnumerateSwapchainImages(
			               eye.swapchain, image_count, &image_count,
			               reinterpret_cast<XrSwapchainImageBaseHeader *>(eye.images.data())),
			           "read eye images"))
				return false;
			glGenFramebuffers(1, &eye.framebuffer);
			glGenRenderbuffers(1, &eye.depth);
			glBindRenderbuffer(GL_RENDERBUFFER, eye.depth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, GLsizei(eye.width),
			                      GLsizei(eye.height));
			XR_LOG("Eye %zu: %ux%u images=%u format=%lld", index, eye.width, eye.height,
			       image_count, static_cast<long long>(format));
		}
		return glGetError() == GL_NO_ERROR || fail("Eye target preparation failed");
	}
	void anchor_scene(const std::array<XrView, 2> &views) {
		// STAGE forward may differ from the user's initial gaze. Anchor the room once,
		// preserving floor height and metric scale; subsequent head motion never drags it.
		const auto &pose = views[0].pose;
		glm::vec3   center((views[0].pose.position.x + views[1].pose.position.x) * .5f, 0,
		                   (views[0].pose.position.z + views[1].pose.position.z) * .5f);
		auto        forward = glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y,
		                                pose.orientation.z) *
		               glm::vec3(0, 0, -1);
		float yaw = forward.x * forward.x + forward.z * forward.z > .001f
		                ? std::atan2(-forward.x, -forward.z)
		                : 0;
		anchor_.initialize(
		    glm::rotate(glm::translate(glm::mat4(1), center), yaw, glm::vec3(0, 1, 0)));
		XR_LOG("Room initial placement from tracked head pose at x=%.3f z=%.3f yaw=%.3f", center.x,
		       center.z, yaw);
	}

	void cube(const glm::mat4 &view_projection, const glm::vec3 &position, const glm::vec3 &size,
	          const glm::vec3 &color, float rotation = 0) {
		auto model = glm::translate(glm::mat4(1), position);
		model      = glm::rotate(model, rotation, glm::normalize(glm::vec3(.3f, 1, .15f)));
		model      = glm::scale(model, size);
		draw_cube(view_projection, model, color);
	}
	void draw_cube(const glm::mat4 &view_projection, const glm::mat4 &model,
	               const glm::vec3 &color) {
		auto mvp = view_projection * model;
		glUniformMatrix4fv(mvp_location_, 1, GL_FALSE, &mvp[0][0]);
		glUniformMatrix4fv(model_location_, 1, GL_FALSE, &model[0][0]);
		glUniform3fv(tint_location_, 1, &color.x);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
	bool render_eye(eye_target &eye, const XrView &view, double seconds) {
		glBindFramebuffer(GL_FRAMEBUFFER, eye.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		                       eye.images[eye.acquired_image].image, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eye.depth);
		bool ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		if (ready) {
			glViewport(0, 0, GLsizei(eye.width), GLsizei(eye.height));
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glDisable(GL_SCISSOR_TEST);
			glClearColor(.008f, .015f, .025f, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glUseProgram(program_);
			glBindVertexArray(vao_);
			float left = std::tan(view.fov.angleLeft), right = std::tan(view.fov.angleRight);
			float down = std::tan(view.fov.angleDown), up = std::tan(view.fov.angleUp);
			constexpr float near_plane = .05f, far_plane = 40;
			glm::mat4       projection(0);
			projection[0][0] = 2 / (right - left);
			projection[1][1] = 2 / (up - down);
			projection[2][0] = (right + left) / (right - left);
			projection[2][1] = (up + down) / (up - down);
			projection[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
			projection[2][3] = -1;
			projection[3][2] = -(2 * far_plane * near_plane) / (far_plane - near_plane);
			const auto &pose = view.pose;
			glm::mat4   camera =
			    glm::translate(glm::mat4(1),
			                   glm::vec3(pose.position.x, pose.position.y, pose.position.z)) *
			    glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y,
			                             pose.orientation.z));
			auto view_projection       = projection * glm::inverse(camera);
			auto scene_view_projection = view_projection * anchor_.transform();
			cube(scene_view_projection, {0, floor_height_ - .1f, -1}, {8, .1f, 8},
			     {.025f, .045f, .065f});
			for (int x = -1; x <= 1; ++x)
				for (int z = -1; z <= 1; ++z)
					cube(scene_view_projection,
					     {float(x) * 2.05f, floor_height_ - .035f, float(z) * 2.05f - 1},
					     {2, .03f, 2}, {.055f, .09f, .105f});
			cube(scene_view_projection, {0, floor_height_ + .35f, -2}, {1.2f, .7f, .7f},
			     {.05f, .12f, .14f});
			cube(scene_view_projection, {0, floor_height_ + 1.2f, -2}, {.4f, .4f, .4f},
			     {.06f, .85f, .7f}, float(std::fmod(seconds * .35, 6.283185307)));
			cube(scene_view_projection, {-2, floor_height_ + 1, -3}, {.12f, 2, .12f},
			     {.85f, .5f, .12f});
			cube(scene_view_projection, {2, floor_height_ + 1, -3}, {.12f, 2, .12f},
			     {.85f, .5f, .12f});
			for (std::size_t hand = 0; hand < controllers_.size(); ++hand) {
				const auto &controller = controllers_[hand];
				if (!controller.valid)
					continue;
				const auto &grip = controller.pose;
				auto        model =
				    glm::translate(glm::mat4(1),
				                   glm::vec3(grip.position.x, grip.position.y, grip.position.z)) *
				    glm::mat4_cast(glm::quat(grip.orientation.w, grip.orientation.x,
				                             grip.orientation.y, grip.orientation.z));
				model = glm::scale(model, glm::vec3(.05f + .025f * controller.trigger, .05f, .14f));
				glm::vec3 color =
				    hand == 0 ? glm::vec3(.08f, .45f, .9f) : glm::vec3(.9f, .45f, .08f);
				color = glm::mix(color, glm::vec3(1), controller.trigger * .7f);
				draw_cube(view_projection, model, color);
			}

			ready = glGetError() == GL_NO_ERROR;
			// Publish GLES commands before returning the image to the OpenXR compositor.
			glFlush();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		auto release =
		    xr_structure<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
		bool released =
		    check(xrReleaseSwapchainImage(eye.swapchain, &release), "release eye image");
		if (released) {
			eye.acquired = false;
			eye.waited   = false;
		}
		return (ready || fail("Eye framebuffer/rendering failed")) && released;
	}
	android_app        &app_;
	lifecycle_client    client_;
	ice::platform::host host_;
	bool           attached_thread_ = false, resumed_ = false, running_ = false, finished_ = false;
	bool           shutting_down_ = false, runtime_lost_ = false;
	XrInstance     instance_           = XR_NULL_HANDLE;
	XrSystemId     system_             = XR_NULL_SYSTEM_ID;
	XrSession      session_            = XR_NULL_HANDLE;
	XrSessionState state_              = XR_SESSION_STATE_UNKNOWN;
	XrSpace        space_              = XR_NULL_HANDLE;
	XrActionSet    controller_actions_ = XR_NULL_HANDLE;
	XrAction       grip_action_ = XR_NULL_HANDLE, trigger_action_ = XR_NULL_HANDLE;
	std::array<controller_state, 2> controllers_;

	XrReferenceSpaceType            space_type_ = XR_REFERENCE_SPACE_TYPE_LOCAL;
	ice::examples::xr::space_anchor anchor_;

	std::array<eye_target, 2> eyes_;
	EGLDisplay                display_ = EGL_NO_DISPLAY;
	EGLConfig                 config_  = nullptr;
	EGLContext                context_ = EGL_NO_CONTEXT;
	EGLSurface                pbuffer_ = EGL_NO_SURFACE;
	GLuint                    program_ = 0, vao_ = 0, vertices_ = 0;
	GLint                     mvp_location_ = -1, model_location_ = -1, tint_location_ = -1;
	float                     floor_height_     = 0;
	std::uint64_t             frames_           = 0;
	std::uint64_t             submitted_frames_ = 0;
	std::chrono::steady_clock::time_point next_heartbeat_{};
};
} // namespace

void android_main(android_app *app) {
	try {
		const bool trace_ready =
		    app->activity->internalDataPath && trace.initialize(app->activity->internalDataPath);
		XR_LOG("Build marker=space-anchor-v2 cplusplus=%ld pid=%d private_trace=%d", __cplusplus,
		       static_cast<int>(getpid()), trace_ready);
		xr_probe probe(*app);
		app->userData = &probe;
		app->onAppCmd = [](android_app *owner, std::int32_t command) {
			static_cast<xr_probe *>(owner->userData)->application_command(command);
		};
		ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
		if (probe.initialize()) {
			while (!app->destroyRequested && !probe.finished()) {
				int   events  = 0;
				void *data    = nullptr;
				int   timeout = probe.running() ? 0 : 100;
				while (ALooper_pollOnce(timeout, nullptr, &events, &data) >= 0) {
					if (data)
						static_cast<android_poll_source *>(data)->process(
						    app, static_cast<android_poll_source *>(data));
					timeout = 0;
					if (app->destroyRequested)
						break;
				}
				if (app->destroyRequested || !probe.poll_events())
					break;
				probe.idle_heartbeat();
				if (probe.running() && !probe.frame())
					break;
			}
		}
		app->onAppCmd = nullptr;
		app->userData = nullptr;
	} catch (const std::exception &error) {
		XR_ERROR("XR probe exception: %s", error.what());
		app->onAppCmd = nullptr;
		app->userData = nullptr;
	}
	XR_LOG("NativeActivity finish requested");
	ANativeActivity_finish(app->activity);
	while (!app->destroyRequested) {
		int   events = 0;
		void *data   = nullptr;
		if (ALooper_pollOnce(100, nullptr, &events, &data) >= 0 && data)
			static_cast<android_poll_source *>(data)->process(
			    app, static_cast<android_poll_source *>(data));
	}
	XR_LOG("NativeActivity destroyed");
}
