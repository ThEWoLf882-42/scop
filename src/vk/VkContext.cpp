#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/vk/VkContext.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

#ifdef NDEBUG
	constexpr bool kEnableValidation = false;
#else
	constexpr bool kEnableValidation = true;
#endif

	const std::vector<const char *> kValidationLayers = {
		"VK_LAYER_KHRONOS_validation"};

	const std::vector<const char *> kRequiredDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	constexpr const char *kPortabilitySubsetExtName = "VK_KHR_portability_subset";
	constexpr const char *kPhysDevProps2ExtName = "VK_KHR_get_physical_device_properties2";

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
		if (version > VK_API_VERSION_1_3)
			version = VK_API_VERSION_1_3;
		return version;
	}

	static std::vector<VkExtensionProperties> enumerateInstanceExtensions()
	{
		uint32_t count = 0;
		if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS)
			throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed");
		std::vector<VkExtensionProperties> props(count);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS)
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

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
		if (formatCount)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, details.formats.data());
		}

		uint32_t presentCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentCount, nullptr);
		if (presentCount)
		{
			details.presentModes.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentCount, details.presentModes.data());
		}
		return details;
	}

}
namespace scop::vk
{

	void VkContext::init(int width, int height, const char *title)
	{
		initWindow_(width, height, title);
		initInstanceAndSurface_();
		pickPhysicalDevice_();
		createLogicalDevice_();
	}

	void VkContext::initWindow_(int width, int height, const char *title)
	{
		if (!glfwInit())
			throw std::runtime_error("glfwInit failed");
		glfwInited_ = true;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);

		window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (!window_)
			throw std::runtime_error("glfwCreateWindow failed");
	}

	void VkContext::initInstanceAndSurface_()
	{
		const bool enableValidation = kEnableValidation && hasLayer(kValidationLayers[0]);

		const uint32_t api = bestApiVersionUpTo13();
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

		if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
			throw std::runtime_error("glfwCreateWindowSurface failed");
	}

	QueueFamilyIndices VkContext::findQueueFamilies(VkPhysicalDevice dev) const
	{
		QueueFamilyIndices out;

		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
		std::vector<VkQueueFamilyProperties> families(count);
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

		for (uint32_t i = 0; i < count; ++i)
		{
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				out.graphicsFamily = i;

			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport);
			if (presentSupport == VK_TRUE)
				out.presentFamily = i;

			if (out.isComplete())
				break;
		}

		return out;
	}

	static bool isDeviceSuitable(VkPhysicalDevice dev, VkSurfaceKHR surface, const VkContext &ctx)
	{
		const auto q = ctx.findQueueFamilies(dev);
		if (!q.isComplete())
			return false;
		if (!checkDeviceExtensionSupport(dev))
			return false;

		const auto sc = querySwapChainSupport(dev, surface);
		return !sc.formats.empty() && !sc.presentModes.empty();
	}

	void VkContext::pickPhysicalDevice_()
	{
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(instance_, &count, nullptr);
		if (count == 0)
			throw std::runtime_error("No Vulkan physical devices found");

		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(instance_, &count, devices.data());

		for (auto dev : devices)
		{
			if (isDeviceSuitable(dev, surface_, *this))
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

		indices_ = findQueueFamilies(physicalDevice_);
		std::cout << "Queue families: graphics=" << indices_.graphicsFamily.value()
				  << " present=" << indices_.presentFamily.value() << "\n";
	}

	void VkContext::createLogicalDevice_()
	{
		std::vector<uint32_t> uniqueFamilies;
		uniqueFamilies.push_back(indices_.graphicsFamily.value());
		if (indices_.presentFamily.value() != indices_.graphicsFamily.value())
			uniqueFamilies.push_back(indices_.presentFamily.value());

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

		VkPhysicalDeviceFeatures supported{};
		vkGetPhysicalDeviceFeatures(physicalDevice_, &supported);

		wireframeSupported_ = (supported.fillModeNonSolid == VK_TRUE);

		if (wireframeSupported_)
			features.fillModeNonSolid = VK_TRUE;

		VkDeviceCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
		ci.pQueueCreateInfos = queueInfos.data();
		ci.pEnabledFeatures = &features;
		ci.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
		ci.ppEnabledExtensionNames = deviceExts.data();

		if (vkCreateDevice(physicalDevice_, &ci, nullptr, &device_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateDevice failed");

		vkGetDeviceQueue(device_, indices_.graphicsFamily.value(), 0, &graphicsQueue_);
		vkGetDeviceQueue(device_, indices_.presentFamily.value(), 0, &presentQueue_);

		std::cout << "Logical device + queues created.\n";
	}

	void VkContext::destroy() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(device_);
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		if (surface_ != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
			surface_ = VK_NULL_HANDLE;
		}

		if (instance_ != VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance_, nullptr);
			instance_ = VK_NULL_HANDLE;
		}

		if (window_)
		{
			glfwDestroyWindow(window_);
			window_ = nullptr;
		}

		if (glfwInited_)
		{
			glfwTerminate();
			glfwInited_ = false;
		}

		physicalDevice_ = VK_NULL_HANDLE;
		graphicsQueue_ = VK_NULL_HANDLE;
		presentQueue_ = VK_NULL_HANDLE;
		indices_ = QueueFamilyIndices{};
	}

}
