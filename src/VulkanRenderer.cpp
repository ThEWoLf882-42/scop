// src/VulkanRenderer.cpp

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/VulkanRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

	constexpr int WIDTH = 900;
	constexpr int HEIGHT = 600;

#ifdef NDEBUG
	constexpr bool kEnableValidation = false;
#else
	constexpr bool kEnableValidation = true;
#endif

	const std::vector<const char *> kValidationLayers = {
		"VK_LAYER_KHRONOS_validation"};

	const std::vector<const char *> kRequiredDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	// macOS portability subset string (some headers don't define the macro)
	constexpr const char *kPortabilitySubsetExtName = "VK_KHR_portability_subset";
	constexpr const char *kPhysDevProps2ExtName = "VK_KHR_get_physical_device_properties2";

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	// ---- Step6: Vertex format ----
	struct Vertex
	{
		float pos[2];
		float color[3];

		static VkVertexInputBindingDescription bindingDescription()
		{
			VkVertexInputBindingDescription b{};
			b.binding = 0;
			b.stride = sizeof(Vertex);
			b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return b;
		}

		static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> a{};

			a[0].binding = 0;
			a[0].location = 0;
			a[0].format = VK_FORMAT_R32G32_SFLOAT;
			a[0].offset = offsetof(Vertex, pos);

			a[1].binding = 0;
			a[1].location = 1;
			a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[1].offset = offsetof(Vertex, color);

			return a;
		}
	};

	static const std::vector<Vertex> kVertices = {
		{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
		{{0.6f, 0.6f}, {0.2f, 1.0f, 0.2f}},
		{{-0.6f, 0.6f}, {0.2f, 0.4f, 1.0f}},
	};

	static bool hasLayer(const char *name)
	{
		uint32_t count = 0;
		vkEnumerateInstanceLayerProperties(&count, nullptr);
		std::vector<VkLayerProperties> props(count);
		vkEnumerateInstanceLayerProperties(&count, props.data());
		for (const auto &p : props)
		{
			if (std::strcmp(p.layerName, name) == 0)
				return true;
		}
		return false;
	}

	static uint32_t bestApiVersionUpTo13()
	{
		uint32_t version = VK_API_VERSION_1_0;
		auto fn = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
			vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
		if (fn)
			fn(&version);

		const uint32_t cap = VK_API_VERSION_1_3;
		if (version > cap)
			version = cap;
		return version;
	}

	static std::vector<VkExtensionProperties> enumerateInstanceExtensions()
	{
		uint32_t count = 0;
		VkResult r = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
		if (r != VK_SUCCESS)
			throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed");
		std::vector<VkExtensionProperties> props(count);
		r = vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
		if (r != VK_SUCCESS)
			throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed");
		return props;
	}

	static bool isSupported(const std::vector<VkExtensionProperties> &props, const char *name)
	{
		for (const auto &p : props)
		{
			if (std::strcmp(p.extensionName, name) == 0)
				return true;
		}
		return false;
	}

	static std::vector<std::string> getInstanceExtensionStrings(bool enableValidation)
	{
		uint32_t glfwCount = 0;
		const char **glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
		if (!glfwExt || glfwCount == 0)
			throw std::runtime_error("glfwGetRequiredInstanceExtensions returned nothing");

		const auto props = enumerateInstanceExtensions();

		std::vector<std::string> exts;
		exts.reserve(glfwCount + 6);

		for (uint32_t i = 0; i < glfwCount; ++i)
		{
			if (!isSupported(props, glfwExt[i]))
			{
				std::string msg = "Required GLFW instance extension not supported: ";
				msg += glfwExt[i];
				throw std::runtime_error(msg);
			}
			exts.emplace_back(glfwExt[i]);
		}

#ifdef __APPLE__
		if (!isSupported(props, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
			throw std::runtime_error("VK_KHR_portability_enumeration not supported");
		exts.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

		if (enableValidation && isSupported(props, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
			exts.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		std::sort(exts.begin(), exts.end());
		exts.erase(std::unique(exts.begin(), exts.end()), exts.end());
		return exts;
	}

	static bool deviceSupportsExtension(VkPhysicalDevice device, const char *extName)
	{
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
		std::vector<VkExtensionProperties> available(count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

		for (const auto &ext : available)
		{
			if (std::strcmp(extName, ext.extensionName) == 0)
				return true;
		}
		return false;
	}

	static bool checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		for (const char *req : kRequiredDeviceExtensions)
		{
			if (!deviceSupportsExtension(device, req))
				return false;
		}
		return true;
	}

	static std::vector<char> readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open file: " + filename);

		const std::streamsize size = file.tellg();
		std::vector<char> buffer(static_cast<size_t>(size));
		file.seekg(0);
		file.read(buffer.data(), size);
		file.close();
		return buffer;
	}

} // namespace

namespace scop
{

	class VulkanRendererImpl
	{
	public:
		void run()
		{
			initWindow();
			initVulkan();
			mainLoop();
			cleanup();
		}

	private:
		GLFWwindow *window_ = nullptr;

		VkInstance instance_ = VK_NULL_HANDLE;
		VkSurfaceKHR surface_ = VK_NULL_HANDLE;

		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		VkDevice device_ = VK_NULL_HANDLE;

		VkQueue graphicsQueue_ = VK_NULL_HANDLE;
		VkQueue presentQueue_ = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		std::vector<VkImage> swapchainImages_;
		std::vector<VkImageView> swapchainImageViews_;
		VkFormat swapchainImageFormat_{};
		VkExtent2D swapchainExtent_{};

		VkRenderPass renderPass_ = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> swapchainFramebuffers_;

		VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;

		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> commandBuffers_;

		VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
		VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
		VkFence inFlightFence_ = VK_NULL_HANDLE;

		// Step6
		VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
		VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;

		// ---------- Init ----------
		void initWindow()
		{
			if (!glfwInit())
				throw std::runtime_error("glfwInit failed");

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

			window_ = glfwCreateWindow(WIDTH, HEIGHT, "scop - step6 (vertex buffer)", nullptr, nullptr);
			if (!window_)
				throw std::runtime_error("glfwCreateWindow failed");
		}

		void initVulkan()
		{
			createInstance();
			createSurface();
			pickPhysicalDevice();
			createLogicalDevice();
			createSwapchain();
			createImageViews();

			createRenderPass();
			createGraphicsPipeline();

			createFramebuffers();
			createCommandPool();

			createVertexBuffer(); // <-- important: before command recording
			createCommandBuffers();

			createSyncObjects();

			std::cout << "Step6 ready (vertex buffer)\n";
		}

		// ---------- Instance / Surface ----------
		void createInstance()
		{
			bool enableValidation = kEnableValidation && hasLayer(kValidationLayers[0]);

			uint32_t api = bestApiVersionUpTo13();
			std::cout << "Requesting Vulkan API: "
					  << VK_VERSION_MAJOR(api) << "."
					  << VK_VERSION_MINOR(api) << "."
					  << VK_VERSION_PATCH(api) << "\n";

			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "scop";
			appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
			appInfo.pEngineName = "no_engine";
			appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
			appInfo.apiVersion = api;

			auto extStrs = getInstanceExtensionStrings(enableValidation);
			std::vector<const char *> extPtrs;
			extPtrs.reserve(extStrs.size());
			for (auto &s : extStrs)
				extPtrs.push_back(s.c_str());

			VkInstanceCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			ci.pApplicationInfo = &appInfo;
			ci.enabledExtensionCount = static_cast<uint32_t>(extPtrs.size());
			ci.ppEnabledExtensionNames = extPtrs.data();

#ifdef __APPLE__
			ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

			if (enableValidation)
			{
				ci.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
				ci.ppEnabledLayerNames = kValidationLayers.data();
			}

			if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateInstance failed");
		}

		void createSurface()
		{
			if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
				throw std::runtime_error("glfwCreateWindowSurface failed");
		}

		// ---------- Device selection ----------
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev)
		{
			QueueFamilyIndices indices;

			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
			std::vector<VkQueueFamilyProperties> families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

			for (uint32_t i = 0; i < count; ++i)
			{
				if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					indices.graphicsFamily = i;

				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport);
				if (presentSupport == VK_TRUE)
					indices.presentFamily = i;

				if (indices.isComplete())
					break;
			}

			return indices;
		}

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev)
		{
			SwapChainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface_, &details.capabilities);

			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &formatCount, nullptr);
			if (formatCount != 0)
			{
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &formatCount, details.formats.data());
			}

			uint32_t presentCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &presentCount, nullptr);
			if (presentCount != 0)
			{
				details.presentModes.resize(presentCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &presentCount, details.presentModes.data());
			}

			return details;
		}

		bool isDeviceSuitable(VkPhysicalDevice dev)
		{
			QueueFamilyIndices q = findQueueFamilies(dev);
			if (!q.isComplete())
				return false;
			if (!checkDeviceExtensionSupport(dev))
				return false;

			SwapChainSupportDetails sc = querySwapChainSupport(dev);
			return !sc.formats.empty() && !sc.presentModes.empty();
		}

		void pickPhysicalDevice()
		{
			uint32_t count = 0;
			vkEnumeratePhysicalDevices(instance_, &count, nullptr);
			if (count == 0)
				throw std::runtime_error("No Vulkan physical devices found");

			std::vector<VkPhysicalDevice> devices(count);
			vkEnumeratePhysicalDevices(instance_, &count, devices.data());

			for (auto dev : devices)
			{
				if (isDeviceSuitable(dev))
				{
					physicalDevice_ = dev;
					break;
				}
			}

			if (physicalDevice_ == VK_NULL_HANDLE)
				throw std::runtime_error("No suitable GPU found");

			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(physicalDevice_, &props);
			std::cout << "Selected GPU: " << props.deviceName << "\n";

			QueueFamilyIndices q = findQueueFamilies(physicalDevice_);
			std::cout << "Queue families: graphics=" << q.graphicsFamily.value()
					  << " present=" << q.presentFamily.value() << "\n";
		}

		void createLogicalDevice()
		{
			QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

			std::vector<uint32_t> uniqueFamilies;
			uniqueFamilies.push_back(indices.graphicsFamily.value());
			if (indices.presentFamily.value() != indices.graphicsFamily.value())
				uniqueFamilies.push_back(indices.presentFamily.value());

			float priority = 1.0f;
			std::vector<VkDeviceQueueCreateInfo> queueInfos;
			queueInfos.reserve(uniqueFamilies.size());

			for (uint32_t family : uniqueFamilies)
			{
				VkDeviceQueueCreateInfo qci{};
				qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				qci.queueFamilyIndex = family;
				qci.queueCount = 1;
				qci.pQueuePriorities = &priority;
				queueInfos.push_back(qci);
			}

			std::vector<const char *> deviceExts = kRequiredDeviceExtensions;

#ifdef __APPLE__
			if (deviceSupportsExtension(physicalDevice_, kPortabilitySubsetExtName))
			{
				if (deviceSupportsExtension(physicalDevice_, kPhysDevProps2ExtName))
					deviceExts.push_back(kPhysDevProps2ExtName);
				deviceExts.push_back(kPortabilitySubsetExtName);
			}
#endif

			VkPhysicalDeviceFeatures features{};

			VkDeviceCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
			ci.pQueueCreateInfos = queueInfos.data();
			ci.pEnabledFeatures = &features;
			ci.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
			ci.ppEnabledExtensionNames = deviceExts.data();

			if (vkCreateDevice(physicalDevice_, &ci, nullptr, &device_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateDevice failed");

			vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
			vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);

			std::cout << "Logical device + queues created.\n";
		}

		// ---------- Swapchain ----------
		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
		{
			for (const auto &f : formats)
			{
				if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
					f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return f;
				}
			}
			return formats[0];
		}

		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes)
		{
			for (auto m : modes)
			{
				if (m == VK_PRESENT_MODE_MAILBOX_KHR)
					return m;
			}
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps)
		{
			if (caps.currentExtent.width != UINT32_MAX)
				return caps.currentExtent;

			int w = 0, h = 0;
			glfwGetFramebufferSize(window_, &w, &h);

			VkExtent2D extent{};
			extent.width = static_cast<uint32_t>(w);
			extent.height = static_cast<uint32_t>(h);

			extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
			extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
			return extent;
		}

		void createSwapchain()
		{
			SwapChainSupportDetails sc = querySwapChainSupport(physicalDevice_);

			VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(sc.formats);
			VkPresentModeKHR presentMode = chooseSwapPresentMode(sc.presentModes);
			VkExtent2D extent = chooseSwapExtent(sc.capabilities);

			uint32_t imageCount = sc.capabilities.minImageCount + 1;
			if (sc.capabilities.maxImageCount > 0 && imageCount > sc.capabilities.maxImageCount)
				imageCount = sc.capabilities.maxImageCount;

			QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
			uint32_t qfi[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

			VkSwapchainCreateInfoKHR ci{};
			ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			ci.surface = surface_;
			ci.minImageCount = imageCount;
			ci.imageFormat = surfaceFormat.format;
			ci.imageColorSpace = surfaceFormat.colorSpace;
			ci.imageExtent = extent;
			ci.imageArrayLayers = 1;
			ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			if (indices.graphicsFamily.value() != indices.presentFamily.value())
			{
				ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				ci.queueFamilyIndexCount = 2;
				ci.pQueueFamilyIndices = qfi;
			}
			else
			{
				ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			}

			ci.preTransform = sc.capabilities.currentTransform;
			ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			ci.presentMode = presentMode;
			ci.clipped = VK_TRUE;

			if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateSwapchainKHR failed");

			uint32_t realCount = 0;
			vkGetSwapchainImagesKHR(device_, swapchain_, &realCount, nullptr);
			swapchainImages_.resize(realCount);
			vkGetSwapchainImagesKHR(device_, swapchain_, &realCount, swapchainImages_.data());

			swapchainImageFormat_ = surfaceFormat.format;
			swapchainExtent_ = extent;

			std::cout << "Swapchain created: extent=" << swapchainExtent_.width
					  << "x" << swapchainExtent_.height
					  << " images=" << swapchainImages_.size() << "\n";
		}

		void createImageViews()
		{
			swapchainImageViews_.resize(swapchainImages_.size());

			for (size_t i = 0; i < swapchainImages_.size(); ++i)
			{
				VkImageViewCreateInfo ci{};
				ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				ci.image = swapchainImages_[i];
				ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
				ci.format = swapchainImageFormat_;
				ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				ci.subresourceRange.baseMipLevel = 0;
				ci.subresourceRange.levelCount = 1;
				ci.subresourceRange.baseArrayLayer = 0;
				ci.subresourceRange.layerCount = 1;

				if (vkCreateImageView(device_, &ci, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS)
					throw std::runtime_error("vkCreateImageView failed");
			}
		}

		// ---------- Render pass ----------
		void createRenderPass()
		{
			VkAttachmentDescription color{};
			color.format = swapchainImageFormat_;
			color.samples = VK_SAMPLE_COUNT_1_BIT;
			color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorRef{};
			colorRef.attachment = 0;
			colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorRef;

			VkSubpassDependency dep{};
			dep.srcSubpass = VK_SUBPASS_EXTERNAL;
			dep.dstSubpass = 0;
			dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			ci.attachmentCount = 1;
			ci.pAttachments = &color;
			ci.subpassCount = 1;
			ci.pSubpasses = &subpass;
			ci.dependencyCount = 1;
			ci.pDependencies = &dep;

			if (vkCreateRenderPass(device_, &ci, nullptr, &renderPass_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateRenderPass failed");
		}

		// ---------- Pipeline ----------
		VkShaderModule createShaderModule(const std::vector<char> &code)
		{
			VkShaderModuleCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			ci.codeSize = code.size();
			ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

			VkShaderModule mod = VK_NULL_HANDLE;
			if (vkCreateShaderModule(device_, &ci, nullptr, &mod) != VK_SUCCESS)
				throw std::runtime_error("vkCreateShaderModule failed");
			return mod;
		}

		void createGraphicsPipeline()
		{
			auto vertCode = readFile("shaders/tri.vert.spv");
			auto fragCode = readFile("shaders/tri.frag.spv");

			VkShaderModule vert = createShaderModule(vertCode);
			VkShaderModule frag = createShaderModule(fragCode);

			VkPipelineShaderStageCreateInfo vertStage{};
			vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertStage.module = vert;
			vertStage.pName = "main";

			VkPipelineShaderStageCreateInfo fragStage{};
			fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragStage.module = frag;
			fragStage.pName = "main";

			VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

			auto binding = Vertex::bindingDescription();
			auto attrs = Vertex::attributeDescriptions();

			VkPipelineVertexInputStateCreateInfo vertexInput{};
			vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInput.vertexBindingDescriptionCount = 1;
			vertexInput.pVertexBindingDescriptions = &binding;
			vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
			vertexInput.pVertexAttributeDescriptions = attrs.data();

			VkPipelineInputAssemblyStateCreateInfo inputAsm{};
			inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAsm.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(swapchainExtent_.width);
			viewport.height = static_cast<float>(swapchainExtent_.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor{};
			scissor.offset = {0, 0};
			scissor.extent = swapchainExtent_;

			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rast{};
			rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rast.rasterizerDiscardEnable = VK_FALSE;
			rast.polygonMode = VK_POLYGON_MODE_FILL;
			rast.lineWidth = 1.0f;
			rast.cullMode = VK_CULL_MODE_BACK_BIT;
			rast.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rast.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo ms{};
			ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState blendAtt{};
			blendAtt.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			blendAtt.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo blend{};
			blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blend.attachmentCount = 1;
			blend.pAttachments = &blendAtt;

			VkPipelineLayoutCreateInfo layoutCI{};
			layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

			if (vkCreatePipelineLayout(device_, &layoutCI, nullptr, &pipelineLayout_) != VK_SUCCESS)
				throw std::runtime_error("vkCreatePipelineLayout failed");

			VkGraphicsPipelineCreateInfo pipeCI{};
			pipeCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipeCI.stageCount = 2;
			pipeCI.pStages = stages;
			pipeCI.pVertexInputState = &vertexInput;
			pipeCI.pInputAssemblyState = &inputAsm;
			pipeCI.pViewportState = &viewportState;
			pipeCI.pRasterizationState = &rast;
			pipeCI.pMultisampleState = &ms;
			pipeCI.pColorBlendState = &blend;
			pipeCI.layout = pipelineLayout_;
			pipeCI.renderPass = renderPass_;
			pipeCI.subpass = 0;

			if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeCI, nullptr, &graphicsPipeline_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateGraphicsPipelines failed");

			vkDestroyShaderModule(device_, frag, nullptr);
			vkDestroyShaderModule(device_, vert, nullptr);
		}

		// ---------- Framebuffers ----------
		void createFramebuffers()
		{
			swapchainFramebuffers_.resize(swapchainImageViews_.size());

			for (size_t i = 0; i < swapchainImageViews_.size(); ++i)
			{
				VkImageView attachments[] = {swapchainImageViews_[i]};

				VkFramebufferCreateInfo ci{};
				ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				ci.renderPass = renderPass_;
				ci.attachmentCount = 1;
				ci.pAttachments = attachments;
				ci.width = swapchainExtent_.width;
				ci.height = swapchainExtent_.height;
				ci.layers = 1;

				if (vkCreateFramebuffer(device_, &ci, nullptr, &swapchainFramebuffers_[i]) != VK_SUCCESS)
					throw std::runtime_error("vkCreateFramebuffer failed");
			}
		}

		// ---------- Command pool ----------
		void createCommandPool()
		{
			QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

			VkCommandPoolCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			ci.queueFamilyIndex = indices.graphicsFamily.value();

			if (vkCreateCommandPool(device_, &ci, nullptr, &commandPool_) != VK_SUCCESS)
				throw std::runtime_error("vkCreateCommandPool failed");
		}

		// ---------- Vertex buffer ----------
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProps{};
			vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

			for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
			{
				if ((typeFilter & (1u << i)) &&
					(memProps.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			throw std::runtime_error("findMemoryType failed");
		}

		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
						  VkBuffer &buffer, VkDeviceMemory &memory)
		{
			VkBufferCreateInfo bci{};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.size = size;
			bci.usage = usage;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(device_, &bci, nullptr, &buffer) != VK_SUCCESS)
				throw std::runtime_error("vkCreateBuffer failed");

			VkMemoryRequirements req{};
			vkGetBufferMemoryRequirements(device_, buffer, &req);

			VkMemoryAllocateInfo ai{};
			ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			ai.allocationSize = req.size;
			ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);

			if (vkAllocateMemory(device_, &ai, nullptr, &memory) != VK_SUCCESS)
				throw std::runtime_error("vkAllocateMemory failed");

			vkBindBufferMemory(device_, buffer, memory, 0);
		}

		void createVertexBuffer()
		{
			VkDeviceSize size = sizeof(kVertices[0]) * kVertices.size();

			createBuffer(
				size,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				vertexBuffer_,
				vertexBufferMemory_);

			void *data = nullptr;
			vkMapMemory(device_, vertexBufferMemory_, 0, size, 0, &data);
			std::memcpy(data, kVertices.data(), static_cast<size_t>(size));
			vkUnmapMemory(device_, vertexBufferMemory_);
		}

		// ---------- Command buffers ----------
		void createCommandBuffers()
		{
			commandBuffers_.resize(swapchainFramebuffers_.size());

			VkCommandBufferAllocateInfo ai{};
			ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			ai.commandPool = commandPool_;
			ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			ai.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

			if (vkAllocateCommandBuffers(device_, &ai, commandBuffers_.data()) != VK_SUCCESS)
				throw std::runtime_error("vkAllocateCommandBuffers failed");

			for (size_t i = 0; i < commandBuffers_.size(); ++i)
			{
				recordCommandBuffer(commandBuffers_[i], static_cast<uint32_t>(i));
			}
		}

		void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
		{
			VkCommandBufferBeginInfo bi{};
			bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
				throw std::runtime_error("vkBeginCommandBuffer failed");

			VkClearValue clear{};
			clear.color = {{0.05f, 0.08f, 0.12f, 1.0f}};

			VkRenderPassBeginInfo rp{};
			rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rp.renderPass = renderPass_;
			rp.framebuffer = swapchainFramebuffers_[imageIndex];
			rp.renderArea.offset = {0, 0};
			rp.renderArea.extent = swapchainExtent_;
			rp.clearValueCount = 1;
			rp.pClearValues = &clear;

			vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

			VkBuffer buffers[] = {vertexBuffer_};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

			vkCmdDraw(cmd, static_cast<uint32_t>(kVertices.size()), 1, 0, 0);

			vkCmdEndRenderPass(cmd);

			if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
				throw std::runtime_error("vkEndCommandBuffer failed");
		}

		// ---------- Sync ----------
		void createSyncObjects()
		{
			VkSemaphoreCreateInfo si{};
			si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fi{};
			fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			if (vkCreateSemaphore(device_, &si, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
				vkCreateSemaphore(device_, &si, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
				vkCreateFence(device_, &fi, nullptr, &inFlightFence_) != VK_SUCCESS)
			{
				throw std::runtime_error("createSyncObjects failed");
			}
		}

		// ---------- Loop ----------
		void mainLoop()
		{
			while (!glfwWindowShouldClose(window_))
			{
				glfwPollEvents();
				drawFrame();

				if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
					glfwSetWindowShouldClose(window_, GLFW_TRUE);
			}
			vkDeviceWaitIdle(device_);
		}

		void drawFrame()
		{
			vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
			vkResetFences(device_, 1, &inFlightFence_);

			uint32_t imageIndex = 0;
			VkResult res = vkAcquireNextImageKHR(
				device_, swapchain_, UINT64_MAX,
				imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
			if (res != VK_SUCCESS)
				throw std::runtime_error("vkAcquireNextImageKHR failed");

			VkSemaphore waitSemaphores[] = {imageAvailableSemaphore_};
			VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			VkSemaphore signalSemaphores[] = {renderFinishedSemaphore_};

			VkSubmitInfo submit{};
			submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit.waitSemaphoreCount = 1;
			submit.pWaitSemaphores = waitSemaphores;
			submit.pWaitDstStageMask = waitStages;
			submit.commandBufferCount = 1;
			submit.pCommandBuffers = &commandBuffers_[imageIndex];
			submit.signalSemaphoreCount = 1;
			submit.pSignalSemaphores = signalSemaphores;

			if (vkQueueSubmit(graphicsQueue_, 1, &submit, inFlightFence_) != VK_SUCCESS)
				throw std::runtime_error("vkQueueSubmit failed");

			VkPresentInfoKHR present{};
			present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present.waitSemaphoreCount = 1;
			present.pWaitSemaphores = signalSemaphores;
			present.swapchainCount = 1;
			present.pSwapchains = &swapchain_;
			present.pImageIndices = &imageIndex;

			if (vkQueuePresentKHR(presentQueue_, &present) != VK_SUCCESS)
				throw std::runtime_error("vkQueuePresentKHR failed");
		}

		// ---------- Cleanup ----------
		void cleanup()
		{
			if (device_ != VK_NULL_HANDLE)
				vkDeviceWaitIdle(device_);

			if (vertexBuffer_ != VK_NULL_HANDLE)
				vkDestroyBuffer(device_, vertexBuffer_, nullptr);
			if (vertexBufferMemory_ != VK_NULL_HANDLE)
				vkFreeMemory(device_, vertexBufferMemory_, nullptr);

			if (inFlightFence_ != VK_NULL_HANDLE)
				vkDestroyFence(device_, inFlightFence_, nullptr);
			if (renderFinishedSemaphore_ != VK_NULL_HANDLE)
				vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
			if (imageAvailableSemaphore_ != VK_NULL_HANDLE)
				vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);

			if (!commandBuffers_.empty())
				vkFreeCommandBuffers(device_, commandPool_, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
			if (commandPool_ != VK_NULL_HANDLE)
				vkDestroyCommandPool(device_, commandPool_, nullptr);

			for (auto fb : swapchainFramebuffers_)
				if (fb)
					vkDestroyFramebuffer(device_, fb, nullptr);
			if (graphicsPipeline_)
				vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
			if (pipelineLayout_)
				vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
			if (renderPass_)
				vkDestroyRenderPass(device_, renderPass_, nullptr);

			for (auto v : swapchainImageViews_)
				if (v)
					vkDestroyImageView(device_, v, nullptr);

			if (swapchain_)
				vkDestroySwapchainKHR(device_, swapchain_, nullptr);
			if (device_)
				vkDestroyDevice(device_, nullptr);
			if (surface_)
				vkDestroySurfaceKHR(instance_, surface_, nullptr);
			if (instance_)
				vkDestroyInstance(instance_, nullptr);

			if (window_)
				glfwDestroyWindow(window_);
			glfwTerminate();
		}
	};

	void VulkanRenderer::run()
	{
		VulkanRendererImpl impl;
		impl.run();
	}

} // namespace scop
