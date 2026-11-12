#include "ice/render/vulkan/device.h"
#include "../batch_validation.h"
#include "profile.h"
#include "submission.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vulkan/vulkan.h>
#include <windows.h>

namespace ice::render::vulkan {
namespace {
template <typename T> T info(VkStructureType kind) {
	T v{};
	v.sType = kind;
	return v;
}
auto failure(const char *what, VkResult result) {
	return ice::unexpected(
	    diagnostic{std::string(what) + " (VkResult " + std::to_string(result) + ")"});
}
#define VK_CHECK(expression)                                                                       \
	do {                                                                                           \
		const auto r = (expression);                                                               \
		if (r != VK_SUCCESS)                                                                       \
			return failure(#expression, r);                                                        \
	} while (false)
template <typename T, typename F> ice::expected<std::vector<T>, diagnostic> enumerate(F get) {
	std::uint32_t count = 0;
	VK_CHECK(get(&count, nullptr));
	if (count > 65536)
		return detail::reject("Vulkan enumeration exceeds bounded profile");
	if (!count)
		return std::vector<T>{};
	std::vector<T> result(count);
	VK_CHECK(get(&count, result.data()));
	result.resize(count);
	return result;
}
bool has(const std::vector<VkExtensionProperties> &extensions, const char *name) {
	return std::ranges::any_of(
	    extensions, [&](const auto &e) { return std::strcmp(e.extensionName, name) == 0; });
}
} // namespace
struct context {
	VkInstance               instance  = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
	VkPhysicalDevice         physical  = VK_NULL_HANDLE;
	VkDevice                 device    = VK_NULL_HANDLE;
	VkQueue                  queue     = VK_NULL_HANDLE;
	std::uint32_t            family    = 0;
	VkRenderPass             pass      = VK_NULL_HANDLE;
	std::string              name;
	std::mutex               mutex;
	std::string              validation_error;
	bool                     lost = false;
	~context() {
		if (device) {
			// Surface owners wait their explicit presentation fences before releasing this owner.
			(void)vkDeviceWaitIdle(device);
			if (pass)
				vkDestroyRenderPass(device, pass, nullptr);
			vkDestroyDevice(device, nullptr);
		}
		if (messenger) {
			auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (destroy)
				destroy(instance, messenger, nullptr);
		}
		if (instance)
			vkDestroyInstance(instance, nullptr);
	}
};
namespace {
#ifdef ICE_VULKAN_VALIDATION
VKAPI_ATTR VkBool32 VKAPI_CALL validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                          VkDebugUtilsMessageTypeFlagsEXT,
                                          const VkDebugUtilsMessengerCallbackDataEXT *data,
                                          void                                       *user) {
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		try {
			auto           &c = *static_cast<context *>(user);
			std::lock_guard lock(c.mutex);
			if (c.validation_error.empty()) {
				c.validation_error = std::string(data->pMessage).substr(0, 4096);
				std::fprintf(stderr, "ICE Vulkan validation: %s\n", c.validation_error.c_str());
			}
		} catch (...) { /* Never unwind into the loader. */
		}
	return VK_FALSE;
}
#endif
ice::expected<std::uint32_t, diagnostic> memory_type(context &c, std::uint32_t bits,
                                                     VkMemoryPropertyFlags flags) {
	VkPhysicalDeviceMemoryProperties p{};
	vkGetPhysicalDeviceMemoryProperties(c.physical, &p);
	for (std::uint32_t i = 0; i < p.memoryTypeCount; ++i)
		if ((bits & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags)
			return i;
	return detail::reject("Vulkan memory type is unavailable for the first profile");
}
struct buffer {
	std::shared_ptr<context> c;
	VkBuffer                 handle = VK_NULL_HANDLE;
	VkDeviceMemory           memory = VK_NULL_HANDLE;
	~buffer() {
		if (handle)
			vkDestroyBuffer(c->device, handle, nullptr);
		if (memory)
			vkFreeMemory(c->device, memory, nullptr);
	}
};
ice::expected<std::unique_ptr<buffer>, diagnostic>
upload(std::shared_ptr<context> c, const void *data, std::size_t bytes, VkBufferUsageFlags usage) {
	auto b             = std::make_unique<buffer>();
	b->c               = std::move(c);
	auto create        = info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
	create.size        = bytes;
	create.usage       = usage;
	create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(b->c->device, &create, nullptr, &b->handle));
	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(b->c->device, b->handle, &requirements);
	auto type =
	    memory_type(*b->c, requirements.memoryTypeBits,
	                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (!type)
		return ice::unexpected(type.error());
	auto allocate            = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
	allocate.allocationSize  = requirements.size;
	allocate.memoryTypeIndex = *type;
	VK_CHECK(vkAllocateMemory(b->c->device, &allocate, nullptr, &b->memory));
	VK_CHECK(vkBindBufferMemory(b->c->device, b->handle, b->memory, 0));
	void *mapped = nullptr;
	VK_CHECK(vkMapMemory(b->c->device, b->memory, 0, bytes, 0, &mapped));
	std::memcpy(mapped, data, bytes);
	vkUnmapMemory(b->c->device, b->memory);
	return b;
}
} // namespace
struct mesh_state {
	std::unique_ptr<buffer> vertices, indices;
	std::uint32_t           count = 0;
};
struct pipeline_state {
	std::shared_ptr<context> c;
	VkPipeline               pipeline = VK_NULL_HANDLE;
	VkPipelineLayout         layout   = VK_NULL_HANDLE;
	VkShaderModule           vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
	std::size_t              uniforms = 0;
	vertex_layout            vertices = vertex_layout::procedural;
	~pipeline_state() {
		if (pipeline)
			vkDestroyPipeline(c->device, pipeline, nullptr);
		if (layout)
			vkDestroyPipelineLayout(c->device, layout, nullptr);
		if (vertex)
			vkDestroyShaderModule(c->device, vertex, nullptr);
		if (fragment)
			vkDestroyShaderModule(c->device, fragment, nullptr);
	}
};
ice::expected<std::unique_ptr<device>, diagnostic> device::create() {
	auto result      = std::make_unique<device>();
	result->state_   = std::make_shared<context>();
	auto &c          = *result->state_;
	auto  extensions = enumerate<VkExtensionProperties>(
        [](auto n, auto p) { return vkEnumerateInstanceExtensionProperties(nullptr, n, p); });
	if (!extensions)
		return ice::unexpected(extensions.error());
	std::vector<const char *> required{VK_KHR_SURFACE_EXTENSION_NAME,
	                                   VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	                                   VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
	                                   VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME};
	std::vector<const char *> layers;
#ifdef ICE_VULKAN_VALIDATION
	required.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	auto available = enumerate<VkLayerProperties>(
	    [](auto n, auto p) { return vkEnumerateInstanceLayerProperties(n, p); });
	if (!available)
		return ice::unexpected(available.error());
	if (!std::ranges::any_of(*available, [](const auto &l) {
		    return std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0;
	    }))
		return detail::reject("Requested Vulkan validation layer is unavailable");
	layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
	for (auto e : required)
		if (!has(*extensions, e))
			return ice::unexpected(
			    diagnostic{std::string("Required Vulkan instance extension unavailable: ") + e});
	auto app                = info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
	app.pApplicationName    = "ICE";
	app.apiVersion          = VK_API_VERSION_1_1;
	auto create             = info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
	create.pApplicationInfo = &app;
	create.enabledExtensionCount   = std::uint32_t(required.size());
	create.ppEnabledExtensionNames = required.data();
	create.enabledLayerCount       = std::uint32_t(layers.size());
	create.ppEnabledLayerNames     = layers.data();
	VK_CHECK(vkCreateInstance(&create, nullptr, &c.instance));
#ifdef ICE_VULKAN_VALIDATION
	auto debug = info<VkDebugUtilsMessengerCreateInfoEXT>(
	    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
	debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug.pfnUserCallback = validation;
	debug.pUserData       = &c;
	auto make_debug       = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(c.instance, "vkCreateDebugUtilsMessengerEXT"));
	if (!make_debug)
		return detail::reject("Vulkan debug-utils entry is unavailable");
	VK_CHECK(make_debug(c.instance, &debug, nullptr, &c.messenger));
#endif
	auto devices = enumerate<VkPhysicalDevice>(
	    [&](auto n, auto p) { return vkEnumeratePhysicalDevices(c.instance, n, p); });
	if (!devices)
		return ice::unexpected(devices.error());
	for (auto physical : *devices) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physical, &properties);
		if (properties.apiVersion < VK_API_VERSION_1_1)
			continue;
		auto exts = enumerate<VkExtensionProperties>([&](auto n, auto p) {
			return vkEnumerateDeviceExtensionProperties(physical, nullptr, n, p);
		});
		if (!exts)
			return ice::unexpected(exts.error());
		if (!has(*exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ||
		    !has(*exts, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME))
			continue;
		auto maintenance = info<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
		    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
		auto features =
		    info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
		features.pNext = &maintenance;
		vkGetPhysicalDeviceFeatures2(physical, &features);
		if (!maintenance.swapchainMaintenance1)
			continue;
		std::uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
		if (count > 1024)
			continue;
		std::vector<VkQueueFamilyProperties> families(count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
		for (std::uint32_t i = 0; i < count; ++i)
			if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			    vkGetPhysicalDeviceWin32PresentationSupportKHR(physical, i)) {
				c.physical = physical;
				c.family   = i;
				c.name     = properties.deviceName;
				break;
			}
		if (c.physical)
			break;
	}
	if (!c.physical)
		return detail::reject(
		    "No Vulkan1.1 Win32 graphics/present device with swapchain-maintenance1");
	VkFormatProperties depth{};
	vkGetPhysicalDeviceFormatProperties(c.physical, VK_FORMAT_D32_SFLOAT, &depth);
	if (!(depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		return detail::reject("Vulkan Depth32Float attachment unsupported");
	const float priority = 1;
	auto        queue = info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
	queue.queueFamilyIndex = c.family;
	queue.queueCount       = 1;
	queue.pQueuePriorities = &priority;
	auto maintenance       = info<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
	maintenance.swapchainMaintenance1 = VK_TRUE;
	const char *device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	                                VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
	auto        device_info = info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
	device_info.pNext       = &maintenance;
	device_info.queueCreateInfoCount    = 1;
	device_info.pQueueCreateInfos       = &queue;
	device_info.enabledExtensionCount   = 2;
	device_info.ppEnabledExtensionNames = device_extensions;
	VK_CHECK(vkCreateDevice(c.physical, &device_info, nullptr, &c.device));
	vkGetDeviceQueue(c.device, c.family, 0, &c.queue);
	std::array<VkAttachmentDescription, 2> attachments{};
	attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	attachments[1].format = VK_FORMAT_D32_SFLOAT;
	for (auto &a : attachments) {
		a.samples        = VK_SAMPLE_COUNT_1_BIT;
		a.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		a.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		a.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		a.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	}
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	    z{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount    = 1;
	subpass.pColorAttachments       = &color;
	subpass.pDepthStencilAttachment = &z;
	VkSubpassDependency dependency{};
	dependency.srcSubpass   = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass   = 0;
	dependency.srcStageMask = dependency.dstStageMask =
	    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
	    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask =
	    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	auto pass            = info<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
	pass.attachmentCount = 2;
	pass.pAttachments    = attachments.data();
	pass.subpassCount    = 1;
	pass.pSubpasses      = &subpass;
	pass.dependencyCount = 1;
	pass.pDependencies   = &dependency;
	VK_CHECK(vkCreateRenderPass(c.device, &pass, nullptr, &c.pass));
	return result;
}
std::string device::name() const { return state_->name; }
std::string device::take_error() {
	std::lock_guard lock(state_->mutex);
	std::string     error;
	error.swap(state_->validation_error);
	return error;
}
ice::expected<void, diagnostic> device::wait_idle() {
	VK_CHECK(vkDeviceWaitIdle(state_->device));
	return {};
}
ice::expected<std::unique_ptr<mesh>, diagnostic> device::create_mesh(const mesh_data &data) {
	auto valid = ice::render::detail::validate_mesh_data(data);
	if (!valid)
		return ice::unexpected(valid.error());
	auto state    = std::make_shared<mesh_state>();
	auto vertices = upload(state_, data.vertices.data(), data.vertices.size() * sizeof(mesh_vertex),
	                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	if (!vertices)
		return ice::unexpected(vertices.error());
	state->vertices = std::move(*vertices);
	auto indices = upload(state_, data.indices.data(), data.indices.size() * sizeof(std::uint32_t),
	                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	if (!indices)
		return ice::unexpected(indices.error());
	state->indices = std::move(*indices);
	state->count   = std::uint32_t(data.indices.size());
	auto result    = std::make_unique<mesh>();
	result->state  = std::move(state);
	return result;
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const shader_snapshot &source, const std::string &vertex_entry,
                         const std::string &fragment_entry, pipeline_options options) {
	if (!detail::supported(options))
		return detail::reject(
		    "Vulkan first slice requires untextured procedural/position_normal RGBA8 pipeline");
	auto parsed = detail::parse(source.source(), vertex_entry, fragment_entry, options.vertices);
	if (!parsed)
		return ice::unexpected(parsed.error());
	auto p      = std::make_shared<pipeline_state>();
	p->c        = state_;
	p->vertices = options.vertices;
	p->uniforms = parsed->pushes ? detail::push_bytes : 0;
	for (auto vertex : {true, false}) {
		const auto &words = vertex ? parsed->vertex : parsed->fragment;
		auto module = info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
		module.codeSize = words.size() * 4;
		module.pCode    = words.data();
		VK_CHECK(vkCreateShaderModule(state_->device, &module, nullptr,
		                              vertex ? &p->vertex : &p->fragment));
	}
	VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
	                          std::uint32_t(detail::push_bytes)};
	auto layout = info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
	layout.pushConstantRangeCount = parsed->pushes ? 1 : 0;
	layout.pPushConstantRanges    = &range;
	VK_CHECK(vkCreatePipelineLayout(state_->device, &layout, nullptr, &p->layout));
	std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
	for (auto &s : stages)
		s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = p->vertex;
	stages[0].pName  = vertex_entry.c_str();
	stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = p->fragment;
	stages[1].pName  = fragment_entry.c_str();
	VkVertexInputBindingDescription binding{0, sizeof(mesh_vertex), VK_VERTEX_INPUT_RATE_VERTEX};
	std::array<VkVertexInputAttributeDescription, 2> attributes{
	    {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}}};
	auto vertex = info<VkPipelineVertexInputStateCreateInfo>(
	    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
	if (options.vertices == vertex_layout::position_normal) {
		vertex.vertexBindingDescriptionCount   = 1;
		vertex.pVertexBindingDescriptions      = &binding;
		vertex.vertexAttributeDescriptionCount = 2;
		vertex.pVertexAttributeDescriptions    = attributes.data();
	}
	auto assembly = info<VkPipelineInputAssemblyStateCreateInfo>(
	    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	auto viewport     = info<VkPipelineViewportStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
	viewport.viewportCount = viewport.scissorCount = 1;
	auto raster                                    = info<VkPipelineRasterizationStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode    = VK_CULL_MODE_NONE;
	raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth   = 1;
	auto sample        = info<VkPipelineMultisampleStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
	sample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	auto depth                  = info<VkPipelineDepthStencilStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
	depth.depthTestEnable  = options.depth_test;
	depth.depthWriteEnable = options.depth_write;
	depth.depthCompareOp   = VK_COMPARE_OP_LESS;
	VkPipelineColorBlendAttachmentState attachment{};
	attachment.colorWriteMask      = 15;
	attachment.blendEnable         = options.alpha_blend;
	attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.colorBlendOp        = VK_BLEND_OP_ADD;
	attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
	auto blend                     = info<VkPipelineColorBlendStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
	blend.attachmentCount = 1;
	blend.pAttachments    = &attachment;
	const VkDynamicState dynamics[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	auto                 dynamic = info<VkPipelineDynamicStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates    = dynamics;
	auto create =
	    info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
	create.stageCount          = 2;
	create.pStages             = stages.data();
	create.pVertexInputState   = &vertex;
	create.pInputAssemblyState = &assembly;
	create.pViewportState      = &viewport;
	create.pRasterizationState = &raster;
	create.pMultisampleState   = &sample;
	create.pDepthStencilState  = &depth;
	create.pColorBlendState    = &blend;
	create.pDynamicState       = &dynamic;
	create.layout              = p->layout;
	create.renderPass          = state_->pass;
	VK_CHECK(vkCreateGraphicsPipelines(state_->device, VK_NULL_HANDLE, 1, &create, nullptr,
	                                   &p->pipeline));
	auto result   = std::make_unique<pipeline>();
	result->state = std::move(p);
	return result;
}

namespace {
struct swapchain {
	std::shared_ptr<context>   c;
	VkSwapchainKHR             handle = VK_NULL_HANDLE;
	VkExtent2D                 extent{};
	std::vector<VkImageView>   views;
	std::vector<VkFramebuffer> frames;
	VkImage                    depth      = VK_NULL_HANDLE;
	VkDeviceMemory             memory     = VK_NULL_HANDLE;
	VkImageView                depth_view = VK_NULL_HANDLE;
	~swapchain() {
		for (auto f : frames)
			if (f)
				vkDestroyFramebuffer(c->device, f, nullptr);
		if (depth_view)
			vkDestroyImageView(c->device, depth_view, nullptr);
		if (depth)
			vkDestroyImage(c->device, depth, nullptr);
		if (memory)
			vkFreeMemory(c->device, memory, nullptr);
		for (auto v : views)
			if (v)
				vkDestroyImageView(c->device, v, nullptr);
		if (handle)
			vkDestroySwapchainKHR(c->device, handle, nullptr);
	}
};
} // namespace
struct presentation_surface::implementation {
	std::shared_ptr<context>   c;
	VkSurfaceKHR               surface = VK_NULL_HANDLE;
	std::unique_ptr<swapchain> swap;
	VkCommandPool              pool     = VK_NULL_HANDLE;
	VkCommandBuffer            commands = VK_NULL_HANDLE;
	VkSemaphore                acquired = VK_NULL_HANDLE, rendered = VK_NULL_HANDLE;
	VkFence acquisition = VK_NULL_HANDLE, submitted = VK_NULL_HANDLE, presented = VK_NULL_HANDLE;
	bool    acquire_pending = false, gpu_pending = false, present_pending = false, dirty = true,
	     poisoned                                      = false;
	int                                          width = 0, height = 0;
	std::vector<std::shared_ptr<pipeline_state>> pipelines;
	std::vector<std::shared_ptr<mesh_state>>     meshes;
	ice::expected<void, diagnostic>              drain() {
        for (auto pending :
             {std::pair{&acquire_pending, acquisition}, std::pair{&gpu_pending, submitted},
		                   std::pair{&present_pending, presented}}) {
            if (!*pending.first)
                continue;
            const auto r = vkWaitForFences(c->device, 1, &pending.second, VK_TRUE, UINT64_MAX);
            if (r == VK_ERROR_DEVICE_LOST)
                c->lost = true;
            if (r != VK_SUCCESS)
                return failure("Vulkan owned fence retirement", r);
            *pending.first = false;
        }
        pipelines.clear();
        meshes.clear();
        return {};
	}
	~implementation() {
		// Never infer present retirement from DeviceWaitIdle. An unexpected wait error
		// retains ownership and retries; teardown has no bounded driver latency promise.
		while (!drain() && !c->lost)
			std::this_thread::yield();
		(void)vkDeviceWaitIdle(c->device);
		swap.reset();
		if (pool)
			vkDestroyCommandPool(c->device, pool, nullptr);
		if (acquisition)
			vkDestroyFence(c->device, acquisition, nullptr);
		if (submitted)
			vkDestroyFence(c->device, submitted, nullptr);
		if (presented)
			vkDestroyFence(c->device, presented, nullptr);
		if (acquired)
			vkDestroySemaphore(c->device, acquired, nullptr);
		if (rendered)
			vkDestroySemaphore(c->device, rendered, nullptr);
		if (surface)
			vkDestroySurfaceKHR(c->instance, surface, nullptr);
	}
	ice::expected<bool, diagnostic> recreate() {
		auto next = std::make_unique<swapchain>();
		next->c   = c;
		VkSurfaceCapabilitiesKHR caps{};
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c->physical, surface, &caps));
		const auto size =
		    detail::choose_extent({std::uint32_t(width), std::uint32_t(height)},
		                          {caps.currentExtent.width, caps.currentExtent.height},
		                          {caps.minImageExtent.width, caps.minImageExtent.height},
		                          {caps.maxImageExtent.width, caps.maxImageExtent.height});
		if (!size.width || !size.height)
			return false; // Native minimize can race the host's preceding extent sample.
		if (size.width > 16384 || size.height > 16384)
			return detail::reject("Vulkan surface exceeds16384 pixels");
		if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ||
		    !(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
			return detail::reject("Vulkan surface lacks opaque color presentation");
		auto formats = enumerate<VkSurfaceFormatKHR>([&](auto n, auto p) {
			return vkGetPhysicalDeviceSurfaceFormatsKHR(c->physical, surface, n, p);
		});
		if (!formats)
			return ice::unexpected(formats.error());
		if (!std::ranges::any_of(*formats, [](auto f) {
			    return f.format == VK_FORMAT_B8G8R8A8_UNORM &&
			           f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		    }))
			return detail::reject("Vulkan first slice requires BGRA8 UNORM/SRGB_NONLINEAR surface");
		auto create = info<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
		create.surface = surface;
		create.minImageCount =
		    caps.minImageCount + (caps.maxImageCount != caps.minImageCount ? 1 : 0);
		create.imageFormat      = VK_FORMAT_B8G8R8A8_UNORM;
		create.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		create.imageExtent      = {size.width, size.height};
		next->extent            = create.imageExtent;
		create.imageArrayLayers = 1;
		create.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create.preTransform     = caps.currentTransform;
		create.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
		create.clipped          = VK_TRUE;
		create.oldSwapchain     = swap ? swap->handle : VK_NULL_HANDLE;
		// A successful create retires oldSwapchain even if later attachment preparation fails.
		// In that case draw() poisons this owner rather than using the retired old chain.
		VK_CHECK(vkCreateSwapchainKHR(c->device, &create, nullptr, &next->handle));
		auto images = enumerate<VkImage>(
		    [&](auto n, auto p) { return vkGetSwapchainImagesKHR(c->device, next->handle, n, p); });
		if (!images)
			return ice::unexpected(images.error());
		next->views.resize(images->size());
		next->frames.resize(images->size());
		auto image      = info<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format    = VK_FORMAT_D32_SFLOAT;
		image.extent    = {size.width, size.height, 1};
		image.mipLevels = image.arrayLayers = 1;
		image.samples                       = VK_SAMPLE_COUNT_1_BIT;
		image.tiling                        = VK_IMAGE_TILING_OPTIMAL;
		image.usage                         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VK_CHECK(vkCreateImage(c->device, &image, nullptr, &next->depth));
		VkMemoryRequirements req{};
		vkGetImageMemoryRequirements(c->device, next->depth, &req);
		auto type = memory_type(*c, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!type)
			return ice::unexpected(type.error());
		auto memory            = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
		memory.allocationSize  = req.size;
		memory.memoryTypeIndex = *type;
		VK_CHECK(vkAllocateMemory(c->device, &memory, nullptr, &next->memory));
		VK_CHECK(vkBindImageMemory(c->device, next->depth, next->memory, 0));
		auto view     = info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
		view.image    = next->depth;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format   = VK_FORMAT_D32_SFLOAT;
		view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
		VK_CHECK(vkCreateImageView(c->device, &view, nullptr, &next->depth_view));
		for (std::size_t i = 0; i < images->size(); ++i) {
			view.image                       = (*images)[i];
			view.format                      = VK_FORMAT_B8G8R8A8_UNORM;
			view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			VK_CHECK(vkCreateImageView(c->device, &view, nullptr, &next->views[i]));
			const VkImageView attachments[]{next->views[i], next->depth_view};
			auto frame = info<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
			frame.renderPass      = c->pass;
			frame.attachmentCount = 2;
			frame.pAttachments    = attachments;
			frame.width           = size.width;
			frame.height          = size.height;
			frame.layers          = 1;
			VK_CHECK(vkCreateFramebuffer(c->device, &frame, nullptr, &next->frames[i]));
		}
		swap  = std::move(next);
		dirty = false;
		return true;
	}
};
presentation_surface::presentation_surface(std::unique_ptr<implementation> p)
    : state_(std::move(p)) {}
presentation_surface::~presentation_surface() = default;
bool presentation_surface::resize(int width, int height) {
	if (width < 0 || height < 0 || width > 16384 || height > 16384)
		return false;
	if (width != state_->width || height != state_->height)
		state_->dirty = true;
	state_->width  = width;
	state_->height = height;
	return true;
}
ice::expected<void, diagnostic> presentation_surface::wait_idle() { return state_->drain(); }
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach(ice::platform::desktop_window &window) {
	const auto native = window.native_handle();
	if (native.kind != ice::platform::native_window_kind::win32 || !native.window)
		return detail::reject("Vulkan first slice requires an owned live Win32 window");
	auto p            = std::make_unique<presentation_surface::implementation>();
	p->c              = state_;
	const auto extent = window.extent();
	p->width          = extent.pixel_width;
	p->height         = extent.pixel_height;
	auto surface =
	    info<VkWin32SurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
	surface.hwnd      = reinterpret_cast<HWND>(native.window);
	surface.hinstance = GetModuleHandleW(nullptr);
	VK_CHECK(vkCreateWin32SurfaceKHR(state_->instance, &surface, nullptr, &p->surface));
	VkBool32 supported = VK_FALSE;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(state_->physical, state_->family, p->surface,
	                                              &supported));
	if (!supported)
		return detail::reject("Selected Vulkan queue cannot present to this Win32 surface");
	auto pool  = info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
	pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool.queueFamilyIndex = state_->family;
	VK_CHECK(vkCreateCommandPool(state_->device, &pool, nullptr, &p->pool));
	auto commands =
	    info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
	commands.commandPool        = p->pool;
	commands.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commands.commandBufferCount = 1;
	VK_CHECK(vkAllocateCommandBuffers(state_->device, &commands, &p->commands));
	auto semaphore = info<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
	VK_CHECK(vkCreateSemaphore(state_->device, &semaphore, nullptr, &p->acquired));
	VK_CHECK(vkCreateSemaphore(state_->device, &semaphore, nullptr, &p->rendered));
	auto fence = info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
	VK_CHECK(vkCreateFence(state_->device, &fence, nullptr, &p->acquisition));
	VK_CHECK(vkCreateFence(state_->device, &fence, nullptr, &p->submitted));
	VK_CHECK(vkCreateFence(state_->device, &fence, nullptr, &p->presented));
	return std::unique_ptr<presentation_surface>(new presentation_surface(std::move(p)));
}
ice::expected<frame_status, diagnostic>
presentation_surface::draw_batch(std::span<const draw_item> draws, std::array<float, 4> clear) {
	auto &p = *state_;
	auto &c = *p.c;
	if (p.poisoned)
		return detail::reject("Vulkan surface failed; drain and recreate its owner");
	// Validate and retain all resources, and allocate all host storage BEFORE acquiring an image.
	std::vector<std::shared_ptr<pipeline_state>> pipelines;
	pipelines.reserve(draws.size());
	std::vector<std::shared_ptr<mesh_state>> meshes;
	meshes.reserve(draws.size());
	for (const auto &draw : draws) {
		if (!draw.pipeline || !draw.pipeline->state || draw.pipeline->state->c != p.c ||
		    draw.uniforms.size() != draw.pipeline->state->uniforms ||
		    (draw.pipeline->state->vertices == vertex_layout::position_normal) !=
		        bool(draw.geometry))
			return detail::reject("Invalid Vulkan draw ownership, layout or exact uniform size");
		if (draw.geometry && (!draw.geometry->state || draw.geometry->state->vertices->c != p.c))
			return detail::reject("Foreign Vulkan mesh");
		pipelines.push_back(draw.pipeline->state);
		if (draw.geometry)
			meshes.push_back(draw.geometry->state);
	}
	for (const auto &pending :
	     {std::pair{p.acquire_pending, p.acquisition}, std::pair{p.gpu_pending, p.submitted},
	      std::pair{p.present_pending, p.presented}}) {
		if (!pending.first)
			continue;
		const auto status = vkGetFenceStatus(c.device, pending.second);
		if (status == VK_NOT_READY)
			return frame_status::busy;
		if (status != VK_SUCCESS) {
			p.poisoned = true;
			return failure("Vulkan frame retirement", status);
		}
	}
	p.acquire_pending = p.gpu_pending = p.present_pending = false;
	p.pipelines.clear();
	p.meshes.clear();
	if (!p.width || !p.height)
		return frame_status::unavailable;
	if (p.dirty || !p.swap) {
		p.poisoned     = true; // Also survives an allocation exception after oldSwapchain retires.
		auto recreated = p.recreate();
		if (!recreated) {
			p.poisoned = true;
			return ice::unexpected(recreated.error());
		}
		p.poisoned = false;
		if (!*recreated)
			return frame_status::unavailable;
	}
	VK_CHECK(vkResetCommandPool(c.device, p.pool, 0));
	VK_CHECK(vkResetFences(c.device, 1, &p.acquisition));
	VK_CHECK(vkResetFences(c.device, 1, &p.submitted));
	VK_CHECK(vkResetFences(c.device, 1, &p.presented));
	std::uint32_t index    = 0;
	const auto    acquired = detail::acquire_with_obligation(
        p.acquire_pending,
        [&] {
            return vkAcquireNextImageKHR(c.device, p.swap->handle, 0, p.acquired, p.acquisition,
		                                    &index);
        },
        VK_SUCCESS, VK_SUBOPTIMAL_KHR);
	if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT) {
		p.acquire_pending = false;
		return frame_status::busy;
	}
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
		p.acquire_pending = false;
		p.dirty           = true;
		return frame_status::unavailable;
	}
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
		if (acquired == VK_ERROR_DEVICE_LOST)
			c.lost = true;
		p.poisoned = true;
		return failure("Vulkan acquire; owned retirement required", acquired);
	}
	// Every post-acquire failure is terminal for this surface; never reuse an uncertain semaphore.
	p.poisoned = true;
	if (index >= p.swap->frames.size())
		return detail::reject("Vulkan acquire returned an invalid image index");
	if (acquired == VK_SUBOPTIMAL_KHR)
		p.dirty = true;
	auto begin  = info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(p.commands, &begin));
	std::array<VkClearValue, 2> values{};
	std::copy(clear.begin(), clear.end(), values[0].color.float32);
	values[1].depthStencil.depth = 1;
	auto pass              = info<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
	pass.renderPass        = c.pass;
	pass.framebuffer       = p.swap->frames[index];
	pass.renderArea.extent = p.swap->extent;
	pass.clearValueCount   = 2;
	pass.pClearValues      = values.data();
	vkCmdBeginRenderPass(p.commands, &pass, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport{0,
	                    float(p.swap->extent.height),
	                    float(p.swap->extent.width),
	                    -float(p.swap->extent.height),
	                    0,
	                    1};
	VkRect2D   scissor{{0, 0}, p.swap->extent};
	vkCmdSetViewport(p.commands, 0, 1, &viewport);
	vkCmdSetScissor(p.commands, 0, 1, &scissor);
	for (const auto &draw : draws) {
		const auto &pipeline = *draw.pipeline->state;
		vkCmdBindPipeline(p.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
		if (!draw.uniforms.empty())
			vkCmdPushConstants(p.commands, pipeline.layout,
			                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			                   std::uint32_t(draw.uniforms.size()), draw.uniforms.data());
		if (draw.geometry) {
			const auto        &mesh   = *draw.geometry->state;
			const VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(p.commands, 0, 1, &mesh.vertices->handle, &offset);
			vkCmdBindIndexBuffer(p.commands, mesh.indices->handle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(p.commands, mesh.count, 1, 0, 0, 0);
		} else
			vkCmdDraw(p.commands, 3, 1, 0, 0);
	}
	vkCmdEndRenderPass(p.commands);
	VK_CHECK(vkEndCommandBuffer(p.commands));
	const VkPipelineStageFlags wait   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	auto                       submit = info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
	submit.waitSemaphoreCount         = 1;
	submit.pWaitSemaphores            = &p.acquired;
	submit.pWaitDstStageMask          = &wait;
	submit.commandBufferCount         = 1;
	submit.pCommandBuffers            = &p.commands;
	submit.signalSemaphoreCount       = 1;
	submit.pSignalSemaphores          = &p.rendered;
	p.pipelines                       = std::move(pipelines);
	p.meshes                          = std::move(meshes);
	// Retain owners before entering the native submission. An ambiguous error cannot
	// establish that the command buffer/resources are safe to destroy or reuse.
	const auto submitted = detail::queue_with_obligation(
	    p.gpu_pending, [&] { return vkQueueSubmit(c.queue, 1, &submit, p.submitted); },
	    VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY);
	if (submitted != VK_SUCCESS) {
		if (submitted == VK_ERROR_DEVICE_LOST)
			c.lost = true;
		return failure("Vulkan queue submit; owned retirement required", submitted);
	}
	auto fence =
	    info<VkSwapchainPresentFenceInfoEXT>(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT);
	fence.swapchainCount       = 1;
	fence.pFences              = &p.presented;
	auto present               = info<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
	present.pNext              = &fence;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores    = &p.rendered;
	present.swapchainCount     = 1;
	present.pSwapchains        = &p.swap->handle;
	present.pImageIndices      = &index;
	const auto status          = vkQueuePresentKHR(c.queue, &present);
	// OUT_OF_DATE/SURFACE_LOST still enqueue the wait and presentation fence per WSI.
	// OOM explicitly guarantees no queue operation was enqueued. Unknown errors
	// retain a pending fence instead of treating an uncertain native call as retired.
	p.present_pending =
	    status != VK_ERROR_OUT_OF_HOST_MEMORY && status != VK_ERROR_OUT_OF_DEVICE_MEMORY;
	if (status == VK_ERROR_DEVICE_LOST)
		c.lost = true;
	if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR)
		p.dirty = true;
	else if (status != VK_SUCCESS)
		return failure("Vulkan present", status);
	p.poisoned = false;
	return frame_status::submitted;
}
#undef VK_CHECK
} // namespace ice::render::vulkan
