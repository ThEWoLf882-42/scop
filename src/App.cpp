#include "App.hpp"

#include "FileUtils.hpp"
#include "ObjLoader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>

namespace scop
{
	namespace
	{
		struct ParsedMaterial
		{
			bool valid;
			scop::Vec3 kd;
			scop::Vec3 ks;
			float ns;

			ParsedMaterial()
				: valid(false), kd(0.64f, 0.64f, 0.64f), ks(0.50f, 0.50f, 0.50f), ns(96.078431f) {}
		};

		static std::string trim(const std::string &value)
		{
			std::size_t start = 0;
			while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
				++start;

			std::size_t end = value.size();
			while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
				--end;

			return value.substr(start, end - start);
		}

		static std::string directoryOf(const std::string &path)
		{
			const std::size_t slash = path.find_last_of("/\\");
			if (slash == std::string::npos)
				return ".";
			if (slash == 0)
				return "/";
			return path.substr(0, slash);
		}

		static std::string joinPath(const std::string &baseDir, const std::string &path)
		{
			if (path.empty())
				return path;
			if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
				return path;
			if (baseDir.empty() || baseDir == ".")
				return path;
			if (baseDir.back() == '/' || baseDir.back() == '\\')
				return baseDir + path;
			return baseDir + "/" + path;
		}

		static std::string findMtllibInObj(const std::string &objPath)
		{
			std::ifstream file(objPath.c_str());
			if (!file)
				return "";

			std::string line;
			while (std::getline(file, line))
			{
				const std::string s = trim(line);
				if (s.empty() || s[0] == '#')
					continue;
				if (s.rfind("mtllib ", 0) == 0)
					return trim(s.substr(7));
			}
			return "";
		}

		static ParsedMaterial loadMaterialFromObj(const std::string &objPath)
		{
			ParsedMaterial material;

			const std::string mtllib = findMtllibInObj(objPath);
			if (mtllib.empty())
				return material;

			const std::string mtlPath = joinPath(directoryOf(objPath), mtllib);
			std::ifstream file(mtlPath.c_str());
			if (!file)
				return material;

			std::string line;
			while (std::getline(file, line))
			{
				const std::string s = trim(line);
				if (s.empty() || s[0] == '#')
					continue;

				std::istringstream iss(s);
				std::string key;
				iss >> key;

				if (key == "Kd")
				{
					float r, g, b;
					if (iss >> r >> g >> b)
					{
						material.kd = scop::Vec3(r, g, b);
						material.valid = true;
					}
				}
				else if (key == "Ks")
				{
					float r, g, b;
					if (iss >> r >> g >> b)
					{
						material.ks = scop::Vec3(r, g, b);
						material.valid = true;
					}
				}
				else if (key == "Ns")
				{
					float ns;
					if (iss >> ns)
					{
						material.ns = ns;
						material.valid = true;
					}
				}
			}

			return material;
		}

		struct UniformBufferObject
		{
			Mat4 model;
			Mat4 view;
			Mat4 proj;
			float params[4];
			float kd[4];
			float ksNs[4];
		};

		const std::vector<const char *> kDeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME};

		VkVertexInputBindingDescription getVertexBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		std::array<VkVertexInputAttributeDescription, 4> getVertexAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, uv);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(Vertex, normal);

			return attributeDescriptions;
		}

	} // namespace

	ScopApp::ScopApp()
		: window_(nullptr),
		  instance_(VK_NULL_HANDLE),
		  surface_(VK_NULL_HANDLE),
		  physicalDevice_(VK_NULL_HANDLE),
		  device_(VK_NULL_HANDLE),
		  graphicsQueue_(VK_NULL_HANDLE),
		  presentQueue_(VK_NULL_HANDLE),
		  swapChain_(VK_NULL_HANDLE),
		  swapChainImageFormat_(VK_FORMAT_UNDEFINED),
		  renderPass_(VK_NULL_HANDLE),
		  descriptorSetLayout_(VK_NULL_HANDLE),
		  pipelineLayout_(VK_NULL_HANDLE),
		  graphicsPipeline_(VK_NULL_HANDLE),
		  commandPool_(VK_NULL_HANDLE),
		  depthImage_(VK_NULL_HANDLE),
		  depthImageMemory_(VK_NULL_HANDLE),
		  depthImageView_(VK_NULL_HANDLE),
		  textureImage_(VK_NULL_HANDLE),
		  textureImageMemory_(VK_NULL_HANDLE),
		  textureImageView_(VK_NULL_HANDLE),
		  textureSampler_(VK_NULL_HANDLE),
		  vertexBuffer_(VK_NULL_HANDLE),
		  vertexBufferMemory_(VK_NULL_HANDLE),
		  indexBuffer_(VK_NULL_HANDLE),
		  indexBufferMemory_(VK_NULL_HANDLE),
		  descriptorPool_(VK_NULL_HANDLE),
		  currentFrame_(0U),
		  framebufferResized_(false),
		  rotationPaused_(false),
		  textureEnabled_(true),
		  hasRealTexture_(false),
		  rotationAngle_(0.0f),
		  rotationSpeed_(0.85f),
		  textureBlend_(0.0f),
		  targetTextureBlend_(1.0f),
		  translation_(0.0f, 0.0f, 0.0f),
		  prevEscape_(false),
		  prevT_(false),
		  prevSpace_(false),
		  prevR_(false),
		  hasMaterial_(false),
		  materialKd_(0.64f, 0.64f, 0.64f),
		  materialKs_(0.50f, 0.50f, 0.50f),
		  materialNs_(96.078431f) {}

	ScopApp::~ScopApp()
	{
		if (device_ != VK_NULL_HANDLE || instance_ != VK_NULL_HANDLE || window_ != nullptr)
		{
			try
			{
				cleanup();
			}
			catch (...)
			{
			}
		}
	}

	void ScopApp::framebufferResizeCallback(GLFWwindow *window, int, int)
	{
		ScopApp *app = static_cast<ScopApp *>(glfwGetWindowUserPointer(window));
		if (app != nullptr)
		{
			app->framebufferResized_ = true;
		}
	}

	void ScopApp::run(const std::string &modelPath, const std::string &texturePath)
	{
		initWindow();
		loadAssets(modelPath, texturePath);
		initVulkan();
		mainLoop();
		cleanup();
	}

	void ScopApp::initWindow()
	{
		if (glfwInit() != GLFW_TRUE)
		{
			throw std::runtime_error("glfwInit failed");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window_ = glfwCreateWindow(
			static_cast<int>(WIDTH),
			static_cast<int>(HEIGHT),
			"scop - Vulkan OBJ viewer",
			nullptr,
			nullptr);

		if (window_ == nullptr)
		{
			glfwTerminate();
			throw std::runtime_error("glfwCreateWindow failed");
		}

		glfwSetWindowUserPointer(window_, this);
		glfwSetFramebufferSizeCallback(window_, ScopApp::framebufferResizeCallback);
	}

	void ScopApp::loadAssets(const std::string &modelPath, const std::string &texturePath)
	{
		mesh_ = ObjLoader::loadFromFile(modelPath);
		centerAndScaleMesh();

		ParsedMaterial parsed = loadMaterialFromObj(modelPath);
		hasMaterial_ = parsed.valid;
		materialKd_ = parsed.kd;
		materialKs_ = parsed.ks;
		materialNs_ = parsed.ns;

		if (texturePath.empty())
		{
			textureData_ = TextureLoader::makeFallbackCheckerboard();
			hasRealTexture_ = false;
			textureEnabled_ = false;
			textureBlend_ = 0.0f;
			targetTextureBlend_ = 0.0f;
			return;
		}

		try
		{
			textureData_ = TextureLoader::loadPPM(texturePath);
			hasRealTexture_ = !textureData_.empty();
		}
		catch (const std::exception &e)
		{
			std::cerr << "Warning: " << e.what() << "\nUsing fallback checkerboard texture instead.\n";
			textureData_ = TextureLoader::makeFallbackCheckerboard();
			hasRealTexture_ = false;
		}

		if (hasRealTexture_)
		{
			textureEnabled_ = true;
			textureBlend_ = 1.0f;
			targetTextureBlend_ = 1.0f;
		}
		else
		{
			textureEnabled_ = false;
			textureBlend_ = 0.0f;
			targetTextureBlend_ = 0.0f;
		}
	}

	void ScopApp::centerAndScaleMesh()
	{
		Vec3 minBounds(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		Vec3 maxBounds(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

		for (const Vertex &vertex : mesh_.vertices)
		{
			minBounds = minVec(minBounds, vertex.position);
			maxBounds = maxVec(maxBounds, vertex.position);
		}

		const Vec3 center = (minBounds + maxBounds) * 0.5f;
		const Vec3 extent = maxBounds - minBounds;
		const float maxExtent = std::max(std::max(extent.x, extent.y), std::max(extent.z, 1e-4f));
		const float scale = 1.6f / maxExtent;

		for (Vertex &vertex : mesh_.vertices)
		{
			vertex.position = (vertex.position - center) * scale;
		}

		mesh_.bounds.min = (minBounds - center) * scale;
		mesh_.bounds.max = (maxBounds - center) * scale;
	}

	void ScopApp::initVulkan()
	{
		createInstance();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createDepthResources();
		createFramebuffers();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void ScopApp::mainLoop()
	{
		bool running = true;
		auto previous = std::chrono::high_resolution_clock::now();

		while (running)
		{
			auto current = std::chrono::high_resolution_clock::now();
			const float dt = std::chrono::duration<float>(current - previous).count();
			previous = current;

			processEvents(running, dt);
			drawFrame();
		}

		vkDeviceWaitIdle(device_);
	}

	void ScopApp::cleanupSwapChain()
	{
		if (!commandBuffers_.empty())
		{
			vkFreeCommandBuffers(device_, commandPool_, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
			commandBuffers_.clear();
		}

		for (std::size_t i = 0; i < uniformBuffers_.size(); ++i)
		{
			if (uniformBuffersMapped_[i] != nullptr)
			{
				vkUnmapMemory(device_, uniformBuffersMemory_[i]);
			}
			vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
			vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
		}
		uniformBuffers_.clear();
		uniformBuffersMemory_.clear();
		uniformBuffersMapped_.clear();

		if (descriptorPool_ != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
			descriptorPool_ = VK_NULL_HANDLE;
		}

		for (VkFramebuffer framebuffer : swapChainFramebuffers_)
		{
			vkDestroyFramebuffer(device_, framebuffer, nullptr);
		}
		swapChainFramebuffers_.clear();

		if (depthImageView_ != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device_, depthImageView_, nullptr);
			depthImageView_ = VK_NULL_HANDLE;
		}
		if (depthImage_ != VK_NULL_HANDLE)
		{
			vkDestroyImage(device_, depthImage_, nullptr);
			depthImage_ = VK_NULL_HANDLE;
		}
		if (depthImageMemory_ != VK_NULL_HANDLE)
		{
			vkFreeMemory(device_, depthImageMemory_, nullptr);
			depthImageMemory_ = VK_NULL_HANDLE;
		}

		if (graphicsPipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
			graphicsPipeline_ = VK_NULL_HANDLE;
		}
		if (pipelineLayout_ != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
			pipelineLayout_ = VK_NULL_HANDLE;
		}
		if (renderPass_ != VK_NULL_HANDLE)
		{
			vkDestroyRenderPass(device_, renderPass_, nullptr);
			renderPass_ = VK_NULL_HANDLE;
		}

		for (VkImageView imageView : swapChainImageViews_)
		{
			vkDestroyImageView(device_, imageView, nullptr);
		}
		swapChainImageViews_.clear();

		if (swapChain_ != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device_, swapChain_, nullptr);
			swapChain_ = VK_NULL_HANDLE;
		}
	}

	void ScopApp::cleanup()
	{
		if (device_ != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(device_);
			cleanupSwapChain();

			if (textureSampler_ != VK_NULL_HANDLE)
			{
				vkDestroySampler(device_, textureSampler_, nullptr);
			}
			if (textureImageView_ != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device_, textureImageView_, nullptr);
			}
			if (textureImage_ != VK_NULL_HANDLE)
			{
				vkDestroyImage(device_, textureImage_, nullptr);
			}
			if (textureImageMemory_ != VK_NULL_HANDLE)
			{
				vkFreeMemory(device_, textureImageMemory_, nullptr);
			}

			if (descriptorSetLayout_ != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
			}
			if (indexBuffer_ != VK_NULL_HANDLE)
			{
				vkDestroyBuffer(device_, indexBuffer_, nullptr);
			}
			if (indexBufferMemory_ != VK_NULL_HANDLE)
			{
				vkFreeMemory(device_, indexBufferMemory_, nullptr);
			}
			if (vertexBuffer_ != VK_NULL_HANDLE)
			{
				vkDestroyBuffer(device_, vertexBuffer_, nullptr);
			}
			if (vertexBufferMemory_ != VK_NULL_HANDLE)
			{
				vkFreeMemory(device_, vertexBufferMemory_, nullptr);
			}

			for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
			{
				if (imageAvailableSemaphores_.size() > i)
				{
					vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
				}
				if (renderFinishedSemaphores_.size() > i)
				{
					vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
				}
				if (inFlightFences_.size() > i)
				{
					vkDestroyFence(device_, inFlightFences_[i], nullptr);
				}
			}
			imageAvailableSemaphores_.clear();
			renderFinishedSemaphores_.clear();
			inFlightFences_.clear();
			imagesInFlight_.clear();

			if (commandPool_ != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(device_, commandPool_, nullptr);
			}
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}
		if (instance_ != VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}
		if (window_ != nullptr)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}
		glfwTerminate();
	}

	void ScopApp::createInstance()
	{
		uint32_t extensionCount = 0U;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
		if (glfwExtensions == nullptr || extensionCount == 0U)
		{
			throw std::runtime_error("glfwGetRequiredInstanceExtensions failed");
		}

		std::vector<const char *> extensions(glfwExtensions, glfwExtensions + extensionCount);

#ifdef __APPLE__
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "scop";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "scop";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		createInfo.enabledLayerCount = 0;

#ifdef __APPLE__
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("vkCreateInstance failed with code: " + std::to_string(result));
		}
	}

	void ScopApp::createSurface()
	{
		if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
		{
			throw std::runtime_error("glfwCreateWindowSurface failed");
		}
	}

	QueueFamilyIndices ScopApp::findQueueFamilies(VkPhysicalDevice device) const
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0U;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0U; i < queueFamilyCount; ++i)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
			if (presentSupport == VK_TRUE)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}
		}

		return indices;
	}

	bool ScopApp::checkDeviceExtensionSupport(VkPhysicalDevice device) const
	{
		uint32_t extensionCount = 0U;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
		for (const VkExtensionProperties &extension : availableExtensions)
		{
			required.erase(extension.extensionName);
		}
		return required.empty();
	}

	SwapChainSupportDetails ScopApp::querySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

		uint32_t formatCount = 0U;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
		if (formatCount != 0U)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount = 0U;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
		if (presentModeCount != 0U)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	bool ScopApp::isDeviceSuitable(VkPhysicalDevice device) const
	{
		const QueueFamilyIndices indices = findQueueFamilies(device);
		const bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(device, &features);

		return indices.isComplete() && extensionsSupported && swapChainAdequate && features.samplerAnisotropy == VK_TRUE;
	}

	void ScopApp::pickPhysicalDevice()
	{
		uint32_t deviceCount = 0U;
		vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
		if (deviceCount == 0U)
		{
			throw std::runtime_error("No Vulkan-capable GPU found");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

		for (VkPhysicalDevice device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice_ = device;
				break;
			}
		}

		if (physicalDevice_ == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU");
		}
	}

	void ScopApp::createLogicalDevice()
	{
		const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
		const std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		const float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1U;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

		if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device");
		}

		vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0U, &graphicsQueue_);
		vkGetDeviceQueue(device_, indices.presentFamily.value(), 0U, &presentQueue_);
	}

	VkSurfaceFormatKHR ScopApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) const
	{
		for (const VkSurfaceFormatKHR &availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}
		return availableFormats.front();
	}

	VkPresentModeKHR ScopApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const
	{
		for (VkPresentModeKHR presentMode : availablePresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return presentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ScopApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window_, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}

	void ScopApp::createSwapChain()
	{
		const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);
		const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1U;
		if (swapChainSupport.capabilities.maxImageCount > 0U && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface_;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1U;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
		const uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2U;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain");
		}

		vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
		swapChainImages_.resize(imageCount);
		vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

		swapChainImageFormat_ = surfaceFormat.format;
		swapChainExtent_ = extent;
	}

	void ScopApp::createImageViews()
	{
		swapChainImageViews_.resize(swapChainImages_.size());
		for (std::size_t i = 0; i < swapChainImages_.size(); ++i)
		{
			swapChainImageViews_[i] = createImageView(swapChainImages_[i], swapChainImageFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	void ScopApp::createRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat_;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0U;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1U;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1U;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0U;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1U;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1U;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass");
		}
	}

	void ScopApp::createDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0U;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1U;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1U;
		samplerLayoutBinding.descriptorCount = 1U;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor set layout");
		}
	}

	VkShaderModule ScopApp::createShaderModule(const std::vector<std::uint8_t> &code) const
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module");
		}
		return shaderModule;
	}

	void ScopApp::createGraphicsPipeline()
	{
		const std::vector<std::uint8_t> vertShaderCode = readBinaryFile("shaders/mesh.vert.spv");
		const std::vector<std::uint8_t> fragShaderCode = readBinaryFile("shaders/mesh.frag.spv");

		const VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		const VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		const VkVertexInputBindingDescription bindingDescription = getVertexBindingDescription();
		const auto attributeDescriptions = getVertexAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1U;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent_.width);
		viewport.height = static_cast<float>(swapChainExtent_.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swapChainExtent_;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1U;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1U;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1U;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1U;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;

		if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2U;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout_;
		pipelineInfo.renderPass = renderPass_;
		pipelineInfo.subpass = 0U;

		if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline");
		}

		vkDestroyShaderModule(device_, fragShaderModule, nullptr);
		vkDestroyShaderModule(device_, vertShaderModule, nullptr);
	}

	void ScopApp::createFramebuffers()
	{
		swapChainFramebuffers_.resize(swapChainImageViews_.size());
		for (std::size_t i = 0; i < swapChainImageViews_.size(); ++i)
		{
			const std::array<VkImageView, 2> attachments = {swapChainImageViews_[i], depthImageView_};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass_;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent_.width;
			framebufferInfo.height = swapChainExtent_.height;
			framebufferInfo.layers = 1U;

			if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer");
			}
		}
	}

	void ScopApp::createCommandPool()
	{
		const QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool");
		}
	}

	uint32_t ScopApp::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

		for (uint32_t i = 0U; i < memProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1U << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("Failed to find suitable memory type");
	}

	void ScopApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
							   VkBuffer &buffer, VkDeviceMemory &bufferMemory)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create buffer");
		}

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate buffer memory");
		}

		vkBindBufferMemory(device_, buffer, bufferMemory, 0);
	}

	VkCommandBuffer ScopApp::beginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool_;
		allocInfo.commandBufferCount = 1U;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void ScopApp::endSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1U;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue_, 1U, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue_);

		vkFreeCommandBuffers(device_, commandPool_, 1U, &commandBuffer);
	}

	void ScopApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1U, &copyRegion);

		endSingleTimeCommands(commandBuffer);
	}

	void ScopApp::createVertexBuffer()
	{
		const VkDeviceSize bufferSize = sizeof(mesh_.vertices[0]) * mesh_.vertices.size();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 stagingBuffer, stagingBufferMemory);

		void *data = nullptr;
		vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
		std::memcpy(data, mesh_.vertices.data(), static_cast<std::size_t>(bufferSize));
		vkUnmapMemory(device_, stagingBufferMemory);

		createBuffer(bufferSize,
					 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 vertexBuffer_, vertexBufferMemory_);

		copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

		vkDestroyBuffer(device_, stagingBuffer, nullptr);
		vkFreeMemory(device_, stagingBufferMemory, nullptr);
	}

	void ScopApp::createIndexBuffer()
	{
		const VkDeviceSize bufferSize = sizeof(mesh_.indices[0]) * mesh_.indices.size();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 stagingBuffer, stagingBufferMemory);

		void *data = nullptr;
		vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
		std::memcpy(data, mesh_.indices.data(), static_cast<std::size_t>(bufferSize));
		vkUnmapMemory(device_, stagingBufferMemory);

		createBuffer(bufferSize,
					 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 indexBuffer_, indexBufferMemory_);

		copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

		vkDestroyBuffer(device_, stagingBuffer, nullptr);
		vkFreeMemory(device_, stagingBufferMemory, nullptr);
	}

	void ScopApp::createUniformBuffers()
	{
		const VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		uniformBuffers_.resize(swapChainImages_.size());
		uniformBuffersMemory_.resize(swapChainImages_.size());
		uniformBuffersMapped_.resize(swapChainImages_.size());

		for (std::size_t i = 0; i < swapChainImages_.size(); ++i)
		{
			createBuffer(bufferSize,
						 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						 uniformBuffers_[i], uniformBuffersMemory_[i]);
			vkMapMemory(device_, uniformBuffersMemory_[i], 0, bufferSize, 0, &uniformBuffersMapped_[i]);
		}
	}

	void ScopApp::createDescriptorPool()
	{
		const std::array<VkDescriptorPoolSize, 2> poolSizes = {{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(swapChainImages_.size())},
																{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(swapChainImages_.size())}}};

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(swapChainImages_.size());

		if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor pool");
		}
	}

	void ScopApp::createDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(swapChainImages_.size(), descriptorSetLayout_);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool_;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages_.size());
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets_.resize(swapChainImages_.size());
		if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate descriptor sets");
		}

		for (std::size_t i = 0; i < swapChainImages_.size(); ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers_[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView_;
			imageInfo.sampler = textureSampler_;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets_[i];
			descriptorWrites[0].dstBinding = 0U;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1U;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets_[i];
			descriptorWrites[1].dstBinding = 1U;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1U;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0U, nullptr);
		}
	}

	VkImageView ScopApp::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0U;
		viewInfo.subresourceRange.levelCount = 1U;
		viewInfo.subresourceRange.baseArrayLayer = 0U;
		viewInfo.subresourceRange.layerCount = 1U;

		VkImageView imageView = VK_NULL_HANDLE;
		if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view");
		}
		return imageView;
	}

	VkFormat ScopApp::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props{};
			vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}
		throw std::runtime_error("Failed to find supported format");
	}

	VkFormat ScopApp::findDepthFormat() const
	{
		return findSupportedFormat(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	bool ScopApp::hasStencilComponent(VkFormat format) const
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void ScopApp::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
							  VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1U;
		imageInfo.mipLevels = 1U;
		imageInfo.arrayLayers = 1U;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image");
		}

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device_, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate image memory");
		}

		vkBindImageMemory(device_, image, imageMemory, 0);
	}

	void ScopApp::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (hasStencilComponent(format))
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		barrier.subresourceRange.baseMipLevel = 0U;
		barrier.subresourceRange.levelCount = 1U;
		barrier.subresourceRange.baseArrayLayer = 0U;
		barrier.subresourceRange.layerCount = 1U;

		VkPipelineStageFlags sourceStage = 0;
		VkPipelineStageFlags destinationStage = 0;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			throw std::runtime_error("Unsupported layout transition");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	void ScopApp::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0U;
		region.imageSubresource.baseArrayLayer = 0U;
		region.imageSubresource.layerCount = 1U;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1U};

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &region);
		endSingleTimeCommands(commandBuffer);
	}

	void ScopApp::createDepthResources()
	{
		const VkFormat depthFormat = findDepthFormat();
		createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					depthImage_, depthImageMemory_);
		depthImageView_ = createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		transitionImageLayout(depthImage_, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	void ScopApp::createTextureImage()
	{
		TextureImage image = textureData_.empty() ? TextureLoader::makeFallbackCheckerboard() : textureData_;

		const VkDeviceSize imageSize = static_cast<VkDeviceSize>(image.width) * static_cast<VkDeviceSize>(image.height) * 4U;
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 stagingBuffer, stagingBufferMemory);

		void *data = nullptr;
		vkMapMemory(device_, stagingBufferMemory, 0, imageSize, 0, &data);
		std::memcpy(data, image.pixels.data(), static_cast<std::size_t>(imageSize));
		vkUnmapMemory(device_, stagingBufferMemory);

		createImage(image.width, image.height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					textureImage_, textureImageMemory_);

		transitionImageLayout(textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, textureImage_, image.width, image.height);
		transitionImageLayout(textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device_, stagingBuffer, nullptr);
		vkFreeMemory(device_, stagingBufferMemory, nullptr);
	}

	void ScopApp::createTextureImageView()
	{
		textureImageView_ = createImageView(textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void ScopApp::createTextureSampler()
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice_, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = std::min(8.0f, properties.limits.maxSamplerAnisotropy);
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create texture sampler");
		}
	}

	void ScopApp::createCommandBuffers()
	{
		commandBuffers_.resize(swapChainFramebuffers_.size());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool_;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

		if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers");
		}

		for (std::size_t i = 0; i < commandBuffers_.size(); ++i)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(commandBuffers_[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin command buffer recording");
			}

			std::array<VkClearValue, 2> clearValues{};
			clearValues[0].color = {{0.06f, 0.06f, 0.08f, 1.0f}};
			clearValues[1].depthStencil = {1.0f, 0U};

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass_;
			renderPassInfo.framebuffer = swapChainFramebuffers_[i];
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = swapChainExtent_;
			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			vkCmdBeginRenderPass(commandBuffers_[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

			VkBuffer vertexBuffers[] = {vertexBuffer_};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(commandBuffers_[i], 0U, 1U, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffers_[i], indexBuffer_, 0U, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0U, 1U,
									&descriptorSets_[i], 0U, nullptr);
			vkCmdDrawIndexed(commandBuffers_[i], static_cast<uint32_t>(mesh_.indices.size()), 1U, 0U, 0, 0U);
			vkCmdEndRenderPass(commandBuffers_[i]);

			if (vkEndCommandBuffer(commandBuffers_[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to record command buffer");
			}
		}
	}

	void ScopApp::createSyncObjects()
	{
		imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);
		imagesInFlight_.assign(swapChainImages_.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
				vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create synchronization objects");
			}
		}
	}

	void ScopApp::recreateSwapChain()
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window_, &width, &height);

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window_, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device_);

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createDepthResources();
		createFramebuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		imagesInFlight_.assign(swapChainImages_.size(), VK_NULL_HANDLE);
	}

	void ScopApp::updateUniformBuffer(uint32_t imageIndex, float dt)
	{
		const float blendSpeed = 2.5f;

		if (textureBlend_ < targetTextureBlend_)
		{
			textureBlend_ += blendSpeed * dt;
			if (textureBlend_ > targetTextureBlend_)
				textureBlend_ = targetTextureBlend_;
		}
		else if (textureBlend_ > targetTextureBlend_)
		{
			textureBlend_ -= blendSpeed * dt;
			if (textureBlend_ < targetTextureBlend_)
				textureBlend_ = targetTextureBlend_;
		}

		if (!rotationPaused_)
		{
			rotationAngle_ += rotationSpeed_ * dt;
		}

		UniformBufferObject ubo{};

		Mat4 model = Mat4::translation(translation_) * Mat4::rotationY(rotationAngle_);
		Mat4 view = Mat4::translation(Vec3(0.0f, 0.0f, -3.0f));
		Mat4 proj = Mat4::perspective(45.0f, static_cast<float>(swapChainExtent_.width) / static_cast<float>(swapChainExtent_.height), 0.1f, 100.0f);

		ubo.model = model;
		ubo.view = view;
		ubo.proj = proj;
		ubo.proj(1, 1) *= -1.0f;

		ubo.params[0] = textureBlend_;
		ubo.params[1] = hasRealTexture_ ? 1.0f : 0.0f;
		ubo.params[2] = hasMaterial_ ? 1.0f : 0.0f;
		ubo.params[3] = 0.0f;

		ubo.kd[0] = materialKd_.x;
		ubo.kd[1] = materialKd_.y;
		ubo.kd[2] = materialKd_.z;
		ubo.kd[3] = 1.0f;

		ubo.ksNs[0] = materialKs_.x;
		ubo.ksNs[1] = materialKs_.y;
		ubo.ksNs[2] = materialKs_.z;
		ubo.ksNs[3] = materialNs_;

		std::memcpy(uniformBuffersMapped_[imageIndex], &ubo, sizeof(ubo));
	}

	void ScopApp::processEvents(bool &running, float dt)
	{
		const float moveSpeed = 1.8f * dt;

		glfwPollEvents();

		if (glfwWindowShouldClose(window_))
		{
			running = false;
		}

		const bool escNow = (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS);
		const bool tNow = (glfwGetKey(window_, GLFW_KEY_T) == GLFW_PRESS);
		const bool spaceNow = (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS);
		const bool rNow = (glfwGetKey(window_, GLFW_KEY_R) == GLFW_PRESS);

		if (escNow && !prevEscape_)
		{
			running = false;
		}
		if (tNow && !prevT_)
		{
			if (hasRealTexture_ || hasMaterial_)
			{
				textureEnabled_ = !textureEnabled_;
				targetTextureBlend_ = textureEnabled_ ? 1.0f : 0.0f;
			}
			else
			{
				textureEnabled_ = false;
				textureBlend_ = 0.0f;
				targetTextureBlend_ = 0.0f;
			}
		}
		if (spaceNow && !prevSpace_)
		{
			rotationPaused_ = !rotationPaused_;
		}
		if (rNow && !prevR_)
		{
			translation_ = Vec3(0.0f, 0.0f, 0.0f);
			rotationAngle_ = 0.0f;
			targetTextureBlend_ = textureEnabled_ && !textureData_.empty() ? 1.0f : 0.0f;
		}

		prevEscape_ = escNow;
		prevT_ = tNow;
		prevSpace_ = spaceNow;
		prevR_ = rNow;

		if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS)
		{
			translation_.x -= moveSpeed;
		}
		if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS)
		{
			translation_.x += moveSpeed;
		}
		if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS)
		{
			translation_.y += moveSpeed;
		}
		if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS)
		{
			translation_.y -= moveSpeed;
		}
		if (glfwGetKey(window_, GLFW_KEY_PAGE_UP) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS)
		{
			translation_.z += moveSpeed;
		}
		if (glfwGetKey(window_, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS)
		{
			translation_.z -= moveSpeed;
		}
	}

	void ScopApp::drawFrame()
	{
		static auto previousFrameTime = std::chrono::high_resolution_clock::now();
		const auto now = std::chrono::high_resolution_clock::now();
		const float dt = std::chrono::duration<float>(now - previousFrameTime).count();
		previousFrameTime = now;

		vkWaitForFences(device_, 1U, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex = 0U;
		VkResult result = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
												imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swap chain image");
		}

		if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device_, 1U, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
		}
		imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

		updateUniformBuffer(imageIndex, dt);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		const VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
		const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1U;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1U;
		submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];

		const VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
		submitInfo.signalSemaphoreCount = 1U;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device_, 1U, &inFlightFences_[currentFrame_]);
		if (vkQueueSubmit(graphicsQueue_, 1U, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffer");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1U;
		presentInfo.pWaitSemaphores = signalSemaphores;

		const VkSwapchainKHR swapChains[] = {swapChain_};
		presentInfo.swapchainCount = 1U;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(presentQueue_, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_)
		{
			framebufferResized_ = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image");
		}

		currentFrame_ = (currentFrame_ + 1U) % MAX_FRAMES_IN_FLIGHT;
	}

} // namespace scop
