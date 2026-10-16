#include "host.h"
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
#include "ice/audio/android_capture.h"
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
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
using namespace ice::examples::xr;
ice::examples::xr::debug_trace trace;

__attribute__((format(printf, 2, 3))) void log_message(int priority, const char *format,
                                                       ...) noexcept {
	std::array<char, 1024> message{};
	va_list                arguments;
	va_start(arguments, format);
	std::vsnprintf(message.data(), message.size(), format, arguments);
	va_end(arguments);
	__android_log_write(priority, "ICE-XR", message.data());
	try {
		trace.write(priority == ANDROID_LOG_ERROR ? 'E' : 'I', message.data());
	} catch (...) {
		// Diagnostics must not interrupt native retirement or an already committed command.
	}
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
	bool    valid = false, primary = false;
};
struct eye_target {
	XrSwapchain                                                     swapchain = XR_NULL_HANDLE;
	std::vector<XrSwapchainImageOpenGLESKHR>                        images;
	std::uint32_t                                                   width = 0, height = 0;
	std::vector<std::unique_ptr<ice::render::borrowed_destination>> destinations;
	std::optional<ice::render::acquisition_lease>                   lease;
	std::uint64_t                                                   generation     = 0;
	std::uint32_t                                                   acquired_image = 0;
	bool acquired = false, waited = false;
};
enum class image_availability { ready, waiting, failed };

// Shared native ownership/lifecycle adapter. Application scene rendering remains in its client.
class android_session {
public:
	android_session(android_app &app, ice::render::device &device, android_client &client,
	                std::string_view name)
	    : app_(app), render_device_(device), application_(client), name_(name), host_(client_) {}
	~android_session() {
		// Retire client dispatch before the last suspend; STOPPING events still drain below.
		shutting_down_      = true;
		application_active_ = false;
		try {
			application_.lifecycle(false);
		} catch (...) {
			XR_ERROR("Application suspend failed during teardown");
		}
		cancel_voice("shutdown");
		XR_LOG("Probe teardown begins: running=%d session_state=%d frames=%llu", running_,
		       static_cast<int>(state_), static_cast<unsigned long long>(submitted_frames_));
		shutdown_session();
		host_.stop();
		auto drained = render_device_.wait_idle();
		if (!drained)
			XR_ERROR("Final RHI drain failed: %s", drained.error().message.c_str());
		// Local FBO/depth/leases retire before runtime color storage and the borrowed EGL root.
		for (auto &eye : eyes_) {
			eye.lease.reset();
			eye.destinations.clear();
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
		              name_.c_str());
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
		binding.display         = binding_.display;
		binding.config          = binding_.config;
		binding.context         = binding_.context;
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
		if (command == APP_CMD_PAUSE || command == APP_CMD_STOP || command == APP_CMD_DESTROY)
			resumed_ = false;
		if (command == APP_CMD_GAINED_FOCUS)
			android_focused_ = true;
		if (command == APP_CMD_LOST_FOCUS || command == APP_CMD_DESTROY)
			android_focused_ = false;
		if (command == APP_CMD_INIT_WINDOW)
			window_available_ = app_.window != nullptr;
		// TERM_WINDOW is dispatched before native-app-glue clears its old window pointer.
		if (command == APP_CMD_TERM_WINDOW || command == APP_CMD_DESTROY)
			window_available_ = false;
		if (command == APP_CMD_PAUSE || command == APP_CMD_STOP || command == APP_CMD_LOST_FOCUS ||
		    command == APP_CMD_TERM_WINDOW || command == APP_CMD_DESTROY)
			cancel_voice(command_name(command));
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
				cancel_voice("instance loss");
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
			if (state_ != XR_SESSION_STATE_FOCUSED)
				cancel_voice("session not focused");
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
			       "continuity unavailable",
			       origin_changes.unknown);
		bool render_ok = update_controllers(timing.predictedDisplayTime), submitted = false,
		     tracked = false;
		std::array<XrView, 2> views{xr_structure<XrView>(XR_TYPE_VIEW),
		                            xr_structure<XrView>(XR_TYPE_VIEW)};
		if (timing.shouldRender && render_ok) {
			auto locate                  = xr_structure<XrViewLocateInfo>(XR_TYPE_VIEW_LOCATE_INFO);
			locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			locate.displayTime           = timing.predictedDisplayTime;
			locate.space                 = space_;
			auto          tracking       = xr_structure<XrViewState>(XR_TYPE_VIEW_STATE);
			std::uint32_t count          = 0;
			render_ok = check(xrLocateViews(session_, &locate, &tracking, 2, &count, views.data()),
			                  "locate stereo views") &&
			            count == 2;
			constexpr XrViewStateFlags valid =
			    XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
			constexpr XrViewStateFlags fully_tracked =
			    valid | XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
			tracked = render_ok && (tracking.viewStateFlags & valid) == valid;
			if (tracked && !anchor_.ready() &&
			    (tracking.viewStateFlags & fully_tracked) == fully_tracked)
				anchor_scene(views);
		}
		frame_observation observations;
		observations.index             = frames_;
		observations.predicted_seconds = double(timing.predictedDisplayTime) * 1e-9;
		const auto now                 = std::chrono::steady_clock::now();
		observations.elapsed_seconds =
		    previous_frame_ ? std::chrono::duration<double>(now - *previous_frame_).count() : 0;
		previous_frame_                = now;
		observations.focused           = application_active_;
		observations.tracked           = tracked && anchor_.ready();
		observations.recording         = voice_active_;
		observations.permission_needed = voice_permission_needed_;
		observations.clip_saved        = voice_saved_;
		if (observations.tracked) {
			const auto &eye  = views[0];
			const auto &pose = eye.pose;
			const auto  reference_from_head =
			    glm::translate(
			        ice::math::mat4(1),
			        ice::math::vec3((pose.position.x + views[1].pose.position.x) * .5f,
			                        (pose.position.y + views[1].pose.position.y) * .5f,
			                        (pose.position.z + views[1].pose.position.z) * .5f)) *
			    glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y,
			                             pose.orientation.z));
			auto head_camera = make_eye_camera(
			    reference_from_head, reference_from_world(),
			    {eye.fov.angleLeft, eye.fov.angleRight, eye.fov.angleDown, eye.fov.angleUp},
			    int(eyes_[0].width), int(eyes_[0].height));
			if (!head_camera) {
				fail(head_camera.error().message);
				render_ok            = false;
				observations.tracked = false;
			} else {
				observations.head.position = head_camera->eye;
				observations.head.orientation =
				    glm::normalize(glm::quat_cast(glm::mat3(glm::inverse(head_camera->view))));
			}
		}
		if (anchor_.ready()) {
			const auto world_from_reference = glm::inverse(reference_from_world());
			for (std::size_t hand = 0; hand < controllers_.size(); ++hand) {
				const auto &source = controllers_[hand];
				const auto &pose   = source.pose;
				auto       &output = observations.controllers[hand];
				output.valid       = source.valid && observations.focused && observations.tracked;
				output.trigger     = output.valid ? source.trigger : 0;
				output.primary     = output.valid && source.primary;
				output.transform =
				    world_from_reference *
				    glm::translate(
				        ice::math::mat4(1),
				        ice::math::vec3(pose.position.x, pose.position.y, pose.position.z)) *
				    glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x,
				                             pose.orientation.y, pose.orientation.z));
			}
		}
		// The authority is updated once here, never in either eye callback.
		auto updated = application_.update(observations);
		if (!updated) {
			fail(updated.error().message);
			render_ok = false;
		}
		std::array<XrCompositionLayerProjectionView, 2> projection_views{};
		submitted = render_ok && timing.shouldRender && observations.tracked;
		if (submitted) {
			// Both images must be waited before drawing; retain exact unwaited leases on timeout.
			for (auto &eye : eyes_) {
				const auto availability = prepare_eye(eye);
				if (availability == image_availability::failed) {
					render_ok = false;
					submitted = false;
					break;
				}
				if (availability != image_availability::ready)
					submitted = false;
			}
			for (std::size_t index = 0; index < eyes_.size() && submitted; ++index) {
				auto &eye = eyes_[index];
				if (!render_eye(eye, views[index], observations)) {
					submitted = false;
					render_ok = false;
					break;
				}
				auto &projection = projection_views[index];
				projection       = xr_structure<XrCompositionLayerProjectionView>(
                    XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
				projection.pose                      = views[index].pose;
				projection.fov                       = views[index].fov;
				projection.subImage.swapchain        = eye.swapchain;
				projection.subImage.imageRect.extent = {std::int32_t(eye.width),
				                                        std::int32_t(eye.height)};
			}
		}
		if (submitted) {
			if (submitted_frames_ == 0 || submitted_frames_ % 120 == 0)
				XR_LOG("Stereo frame=%llu views=2 focused=%d left=%d right=%d",
				       static_cast<unsigned long long>(frames_), application_active_,
				       controllers_[0].valid, controllers_[1].valid);
			++submitted_frames_;
		}
		auto layer =
		    xr_structure<XrCompositionLayerProjection>(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
		layer.space              = space_;
		layer.viewCount          = 2;
		layer.views              = projection_views.data();
		const auto *header       = reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);
		auto        end          = xr_structure<XrFrameEndInfo>(XR_TYPE_FRAME_END_INFO);
		end.displayTime          = timing.predictedDisplayTime;
		end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		end.layerCount           = submitted ? 1 : 0;
		end.layers               = submitted ? &header : nullptr;
		const bool ended         = !runtime_lost_ && check(xrEndFrame(session_, &end), "end frame");
		++frames_;
		host_.frame(std::chrono::duration<double>(now.time_since_epoch()).count());
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
			if (eye.lease)
				eye.lease->expire();
			auto release =
			    xr_structure<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
			if (XR_SUCCEEDED(xrReleaseSwapchainImage(eye.swapchain, &release))) {
				eye.lease.reset();
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
			if (eye.generation == std::numeric_limits<std::uint64_t>::max()) {
				fail("Native image acquisition generations exhausted");
				return image_availability::failed;
			}
			auto lease = eye.destinations[eye.acquired_image]->begin_acquisition(++eye.generation);
			if (!lease) {
				fail(lease.error().message);
				return image_availability::failed;
			}
			eye.lease = std::move(*lease);
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
			eye.waited  = true;
			auto waited = eye.lease->mark_waited();
			if (!waited) {
				fail(waited.error().message);
				return image_availability::failed;
			}
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
		if (shutting_down_)
			return;
		using ice::platform::app_state;
		const bool active =
		    resumed_ && android_focused_ && window_available_ && state_ == XR_SESSION_STATE_FOCUSED;
		if (active != application_active_) {
			application_active_ = active;
			XR_LOG("Application active=%d android_resumed=%d focused=%d window=%d xr_state=%d",
			       active, resumed_, android_focused_, window_available_, int(state_));
			application_.lifecycle(active);
			previous_frame_.reset();
		}
		host_.set_state(!resumed_ ? app_state::background
		                : active  ? app_state::active
		                          : app_state::inactive);
	}
	bool initialize_graphics() {
		auto current = render_device_.egl_context();
		if (!current)
			return fail(current.error().message);
		binding_    = *current;
		GLint major = 0, minor = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
		if (major < 3 || (major == 3 && minor < 1))
			return fail("Borrowed OpenXR image validation requires OpenGL ES3.1 or newer");
		return true;
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
		action.actionType          = XR_ACTION_TYPE_BOOLEAN_INPUT;
		action.countSubactionPaths = 1;
		std::snprintf(action.actionName, XR_MAX_ACTION_NAME_SIZE, "%s", "push_to_talk");
		std::snprintf(action.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%s",
		              "Hold X to record microphone");
		if (!check(xrCreateAction(controller_actions_, &action, &voice_action_),
		           "create push-to-talk action"))
			return false;
		action.countSubactionPaths = 2;
		std::snprintf(action.actionName, XR_MAX_ACTION_NAME_SIZE, "%s", "primary_button");
		std::snprintf(action.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, "%s",
		              "Y or A primary button");
		if (!check(xrCreateAction(controller_actions_, &action, &primary_action_),
		           "create primary button action"))
			return false;
		std::array<XrActionSuggestedBinding, 7> bindings{};
		const std::array<const char *, 7>       paths{
            "/user/hand/left/input/grip/pose",     "/user/hand/right/input/grip/pose",
            "/user/hand/left/input/trigger/value", "/user/hand/right/input/trigger/value",
            "/user/hand/left/input/x/click",       "/user/hand/left/input/y/click",
            "/user/hand/right/input/a/click"};
		for (std::size_t index = 0; index < bindings.size(); ++index) {
			bindings[index].action = index < 2    ? grip_action_
			                         : index < 4  ? trigger_action_
			                         : index == 4 ? voice_action_
			                                      : primary_action_;
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
			controller.primary = false;
		}
		if (!application_active_) {
			cancel_voice("controller focus lost");
			return true;
		}
		XrActiveActionSet active{};
		active.actionSet           = controller_actions_;
		auto sync                  = xr_structure<XrActionsSyncInfo>(XR_TYPE_ACTIONS_SYNC_INFO);
		sync.countActiveActionSets = 1;
		sync.activeActionSets      = &active;
		auto synchronized          = xrSyncActions(session_, &sync);
		if (synchronized == XR_SESSION_NOT_FOCUSED) {
			cancel_voice("action sync not focused");
			return true;
		}
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
			get.action   = primary_action_;
			auto primary = xr_structure<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
			if (!check(xrGetActionStateBoolean(session_, &get, &primary), "read primary button"))
				return false;
			controller.primary = primary.isActive && primary.currentState;
		}
		auto get_voice          = xr_structure<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
		get_voice.action        = voice_action_;
		get_voice.subactionPath = controllers_[0].hand;
		auto voice              = xr_structure<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
		if (!check(xrGetActionStateBoolean(session_, &get_voice, &voice),
		           "read push-to-talk action"))
			return false;
		if (!voice.isActive)
			cancel_voice("PTT action inactive");
		else
			update_voice(voice.currentState);
		return true;
	}
	bool microphone_permission(bool request) {
		JNIEnv *environment = nullptr;
		if (app_.activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
		    JNI_OK)
			return false;
		jclass    activity = environment->GetObjectClass(app_.activity->clazz);
		jmethodID has      = activity
		                         ? environment->GetMethodID(activity, "hasMicrophonePermission", "()Z")
		                         : nullptr;
		if (environment->ExceptionCheck() || !has) {
			environment->ExceptionClear();
			if (activity)
				environment->DeleteLocalRef(activity);
			XR_ERROR("Voice permission host unavailable");
			return false;
		}
		bool granted = environment->CallBooleanMethod(app_.activity->clazz, has) == JNI_TRUE;
		if (environment->ExceptionCheck()) {
			environment->ExceptionClear();
			granted = false;
		}
		if (request && !granted) {
			jmethodID ask =
			    environment->GetMethodID(activity, "requestMicrophonePermission", "()V");
			if (ask)
				environment->CallVoidMethod(app_.activity->clazz, ask);
			if (environment->ExceptionCheck()) {
				environment->ExceptionClear();
				XR_ERROR("Voice permission request failed");
			}
			XR_LOG("Voice permission requested; release X and press again after granting");
		}
		environment->DeleteLocalRef(activity);
		return granted;
	}
	void cancel_voice(const char *reason) {
		if (voice_active_ || microphone_.recording())
			XR_LOG("Voice cancelled reason=%s; unfinished PCM discarded", reason);
		auto stopped = microphone_.stop();
		if (!stopped)
			XR_ERROR("Voice cancellation stop failed: %s", stopped.error().message.c_str());
		microphone_.discard();
		voice_active_ = false;
		voice_armed_  = false; // A release is required after focus/permission changes.
	}
	void update_voice(bool down) {
		if (!down) {
			if (voice_active_)
				finish_voice("release");
			voice_armed_ = true;
			return;
		}
		if (voice_armed_) {
			voice_armed_ = false;
			voice_saved_ = false;
			if (!microphone_permission(true)) {
				voice_permission_needed_ = true;
				return;
			}
			voice_permission_needed_ = false;
			auto started             = microphone_.start();
			if (!started) {
				XR_ERROR("Voice start rejected: %s native=%d", started.error().message.c_str(),
				         started.error().native_code);
				return;
			}
			voice_active_    = true;
			voice_first_pcm_ = false;
			++voice_sequence_;
			voice_started_ms_ =
			    static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
			                                   std::chrono::system_clock::now().time_since_epoch())
			                                   .count());
			XR_LOG("Voice started id=%d-%llu rate=%u cap_frames=%zu input=X held", getpid(),
			       static_cast<unsigned long long>(voice_sequence_),
			       microphone_.buffer()->sample_rate(), microphone_.buffer()->capacity_frames());
		}
		if (!voice_active_)
			return;
		if (!microphone_permission(false)) {
			cancel_voice("permission lost");
			voice_permission_needed_ = true;
			return;
		}
		auto captured = microphone_.poll();
		if (!captured) {
			XR_ERROR("Voice capture failed: %s native=%d", captured.error().message.c_str(),
			         captured.error().native_code);
			cancel_voice("capture error");
			return;
		}
		if (!voice_first_pcm_ && microphone_.buffer() && !microphone_.buffer()->samples().empty()) {
			voice_first_pcm_ = true;
			XR_LOG("Voice first PCM id=%d-%llu frames=%zu rate=%u", getpid(),
			       static_cast<unsigned long long>(voice_sequence_),
			       microphone_.buffer()->samples().size(), microphone_.buffer()->sample_rate());
		}
		if (!microphone_.recording())
			finish_voice("duration_limit");
	}
	static bool write_private_file(const std::string &path, std::span<const std::byte> bytes) {
		FILE *file = std::fopen(path.c_str(), "wb");
		if (!file)
			return false;
		const bool wrote   = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
		const bool flushed = std::fflush(file) == 0;
		const bool closed  = std::fclose(file) == 0;
		return wrote && flushed && closed;
	}
	void finish_voice(const char *reason) {
		voice_active_ = false;
		auto stopped  = microphone_.stop();
		if (!stopped) {
			XR_ERROR("Voice stop failed: %s", stopped.error().message.c_str());
			microphone_.discard();
			return;
		}
		const auto *buffer = microphone_.buffer();
		if (!buffer || buffer->samples().empty() || !app_.activity->internalDataPath) {
			XR_LOG("Voice finished with no PCM frames reason=%s; no file published", reason);
			microphone_.discard();
			return;
		}
		auto wav = ice::audio::encode_wav(*buffer);
		if (!wav) {
			XR_ERROR("Voice WAV failed: %s", wav.error().message.c_str());
			microphone_.discard();
			return;
		}
		const std::string directory = std::string(app_.activity->internalDataPath) + "/";
		const std::string manifest  = directory + "voice-latest.json";
		// Two fixed WAV slots bound disk usage across launches. Publish the metadata
		// pointer only after the inactive slot is complete; readers start at that file.
		if (voice_slot_ < 0) {
			std::array<char, 2048> prior{};
			FILE                  *file = std::fopen(manifest.c_str(), "rb");
			if (file) {
				std::fread(prior.data(), 1, prior.size() - 1, file);
				std::fclose(file);
			}
			voice_slot_ = std::strstr(prior.data(), "\"slot\":0") ? 0 : 1;
		}
		const int         slot    = 1 - voice_slot_;
		const std::string name    = slot ? "voice-1.wav" : "voice-0.wav";
		const std::string path    = directory + name;
		std::int32_t      peak    = 0;
		double            squared = 0;
		for (auto sample : buffer->samples()) {
			const auto value = std::int32_t(sample);
			peak             = std::max(peak, value < 0 ? -value : value);
			squared += double(value) * value;
		}
		const double           rms = std::sqrt(squared / double(buffer->samples().size()));
		std::array<char, 2048> metadata{};
		const int              size = std::snprintf(
            metadata.data(), metadata.size(),
            "{\"version\":1,\"slot\":%d,\"file\":\"%s\",\"source\":\"aaudio_microphone\","
		                 "\"consent\":\"user_push_to_talk_X\",\"build\":\"space-anchor-v2+voice-v2\","
		                 "\"session\":\"xr-%d-%llu\",\"recording_id\":\"%d-%llu\",\"started_unix_ms\":%llu,"
		                 "\"format\":\"pcm_s16le\",\"channels\":1,\"sample_rate\":%u,\"frames\":%zu,"
		                 "\"dropped_frames\":%llu,\"xruns\":%d,\"peak_pcm16\":%d,\"rms_pcm16\":%u,"
		                 "\"stop_reason\":\"%s\",\"automatic_sharing\":false}\n",
            slot, name.c_str(), getpid(), static_cast<unsigned long long>(voice_session_ms_),
            getpid(), static_cast<unsigned long long>(voice_sequence_),
            static_cast<unsigned long long>(voice_started_ms_), buffer->sample_rate(),
            buffer->samples().size(), static_cast<unsigned long long>(buffer->dropped_frames()),
            microphone_.xruns(), peak, static_cast<unsigned>(std::lround(rms)), reason);
		const bool valid_size = size > 0 && std::size_t(size) < metadata.size();
		const auto manifest_bytes =
		    std::as_bytes(std::span(metadata.data(), valid_size ? std::size_t(size) : 0));
		bool saved = valid_size && write_private_file(path + ".tmp", *wav) &&
		             write_private_file(manifest + ".tmp", manifest_bytes) &&
		             std::rename((path + ".tmp").c_str(), path.c_str()) == 0 &&
		             std::rename((manifest + ".tmp").c_str(), manifest.c_str()) == 0;
		if (saved) {
			voice_slot_  = slot;
			voice_saved_ = true;
			XR_LOG("Voice saved id=%d-%llu file=%s rate=%u frames=%zu dropped=%llu xruns=%d "
			       "peak=%d rms=%.1f reason=%s",
			       getpid(), static_cast<unsigned long long>(voice_sequence_), name.c_str(),
			       buffer->sample_rate(), buffer->samples().size(),
			       static_cast<unsigned long long>(buffer->dropped_frames()), microphone_.xruns(),
			       peak, rms, reason);
		} else
			XR_ERROR("Voice private WAV/metadata publication failed");
		std::remove((path + ".tmp").c_str());
		std::remove((manifest + ".tmp").c_str());
		microphone_.discard();
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
		// OpenXR interprets non-sRGB formats as linear. Desktop UNORM display encoding
		// cannot be substituted here without a separate output policy.
		const std::int64_t format = GL_SRGB8_ALPHA8;
		if (std::find(formats.begin(), formats.end(), format) == formats.end())
			return fail("This native XR profile requires an sRGB8-alpha8 swapchain format");
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
			for (const auto &image : eye.images) {
				auto imported = render_device_.import_destination(
				    {binding_, image.image, int(eye.width), int(eye.height),
				     format == GL_SRGB8_ALPHA8 ? ice::render::color_format::rgba8_srgb
				                               : ice::render::color_format::rgba8_unorm});
				if (!imported)
					return fail(imported.error().message);
				eye.destinations.push_back(std::move(*imported));
			}
			XR_LOG("Eye %zu: %ux%u images=%u format=%lld", index, eye.width, eye.height,
			       image_count, static_cast<long long>(format));
		}
		auto prepared = application_.prepare(format == GL_SRGB8_ALPHA8
		                                         ? ice::render::color_format::rgba8_srgb
		                                         : ice::render::color_format::rgba8_unorm);
		if (!prepared)
			return fail(prepared.error().message);
		XR_LOG("Shared RHI eye destinations prepared; application resources ready");
		return true;
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

	ice::math::mat4 reference_from_world() const {
		return anchor_.transform() *
		       glm::translate(ice::math::mat4(1), ice::math::vec3(0, floor_height_, 0));
	}
	bool render_eye(eye_target &eye, const XrView &view, const frame_observation &observations) {
		const auto &pose = view.pose;
		const auto  reference_from_eye =
		    glm::translate(ice::math::mat4(1),
		                   ice::math::vec3(pose.position.x, pose.position.y, pose.position.z)) *
		    glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y,
		                             pose.orientation.z));
		auto camera = make_eye_camera(
		    reference_from_eye, reference_from_world(),
		    {view.fov.angleLeft, view.fov.angleRight, view.fov.angleDown, view.fov.angleUp},
		    int(eye.width), int(eye.height));
		bool ready = bool(camera);
		if (!camera)
			fail(camera.error().message);
		if (camera) {
			auto batch = application_.frame(*camera, observations);
			if (!batch) {
				fail(batch.error().message);
				ready = false;
			} else {
				auto submitted = render_device_.render_into(*eye.destinations[eye.acquired_image],
				                                            *eye.lease, *batch);
				ready = bool(submitted) && *submitted == ice::render::frame_status::submitted;
				if (!submitted)
					fail(submitted.error().message);
			}
		}
		// Submit possible native commands even after an encoding error, then expire BEFORE release.
		auto flushed = render_device_.flush();
		if (!flushed) {
			fail(flushed.error().message);
			ready = false;
		}
		eye.lease->expire();
		auto release =
		    xr_structure<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
		const bool released =
		    check(xrReleaseSwapchainImage(eye.swapchain, &release), "release eye image");
		if (released) {
			eye.acquired = false;
			eye.waited   = false;
			eye.lease.reset();
		}
		return ready && released;
	}
	android_app                                         &app_;
	ice::render::device                                 &render_device_;
	android_client                                      &application_;
	std::string                                          name_;
	bool                                                 application_active_ = false;
	std::optional<std::chrono::steady_clock::time_point> previous_frame_;
	lifecycle_client                                     client_;
	ice::platform::host                                  host_;
	bool           attached_thread_ = false, resumed_ = false, running_ = false, finished_ = false;
	bool           shutting_down_ = false, runtime_lost_ = false;
	bool           android_focused_ = false, window_available_ = false;
	XrInstance     instance_           = XR_NULL_HANDLE;
	XrSystemId     system_             = XR_NULL_SYSTEM_ID;
	XrSession      session_            = XR_NULL_HANDLE;
	XrSessionState state_              = XR_SESSION_STATE_UNKNOWN;
	XrSpace        space_              = XR_NULL_HANDLE;
	XrActionSet    controller_actions_ = XR_NULL_HANDLE;
	XrAction       grip_action_ = XR_NULL_HANDLE, trigger_action_ = XR_NULL_HANDLE;
	XrAction       voice_action_ = XR_NULL_HANDLE, primary_action_ = XR_NULL_HANDLE;
	std::array<controller_state, 2> controllers_;
	ice::audio::android_capture     microphone_;
	bool voice_active_ = false, voice_armed_ = false, voice_saved_ = false,
	     voice_first_pcm_ = false, voice_permission_needed_ = false;
	int                 voice_slot_     = -1;
	std::uint64_t       voice_sequence_ = 0, voice_started_ms_ = 0;
	const std::uint64_t voice_session_ms_ =
	    static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
	                                   std::chrono::system_clock::now().time_since_epoch())
	                                   .count());

	XrReferenceSpaceType            space_type_ = XR_REFERENCE_SPACE_TYPE_LOCAL;
	ice::examples::xr::space_anchor anchor_;

	std::array<eye_target, 2>             eyes_;
	ice::render::gl::egl_binding          binding_;
	float                                 floor_height_     = 0;
	std::uint64_t                         frames_           = 0;
	std::uint64_t                         submitted_frames_ = 0;
	std::chrono::steady_clock::time_point next_heartbeat_{};
};
} // namespace

void ice::examples::xr::run_android(android_app &owner, ice::render::device &device,
                                    android_client &client, std::string_view application_name) {
	auto *app = &owner;
	try {
		const bool trace_ready =
		    app->activity->internalDataPath && trace.initialize(app->activity->internalDataPath);
		XR_LOG("Build marker=shared-rhi-v1+space-anchor-v2 cplusplus=%ld pid=%d private_trace=%d",
		       __cplusplus, static_cast<int>(getpid()), trace_ready);
		XR_LOG("Voice marker=voice-v2-state-pump explicit_PTT=left_X max_seconds=10; no automatic "
		       "ASR/sharing");
		android_session probe(*app, device, client, application_name);
		app->userData = &probe;
		app->onAppCmd = [](android_app *owner, std::int32_t command) {
			static_cast<android_session *>(owner->userData)->application_command(command);
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
	} catch (...) {
		XR_ERROR("Unknown XR client exception");
		app->onAppCmd = nullptr;
		app->userData = nullptr;
	}
	finish_android_activity(*app);
}

void ice::examples::xr::finish_android_activity(android_app &owner) noexcept {
	auto *app     = &owner;
	app->onAppCmd = nullptr;
	app->userData = nullptr;
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

void ice::examples::xr::write_diagnostic(std::string_view message) noexcept {
	if (message.empty())
		return;
	try {
		XR_LOG("Application: %.*s", int(std::min(message.size(), std::size_t(960))),
		       message.data());
	} catch (...) {
		// Logging is observational, including after a command has already committed.
	}
}
