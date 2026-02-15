#pragma once

#include <optional>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace scop::vk
{

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
	};

	class VkContext
	{
	public:
		VkContext() = default;
		~VkContext() noexcept { destroy(); }

		VkContext(const VkContext &) = delete;
		VkContext &operator=(const VkContext &) = delete;

		VkContext(VkContext &&) = delete;
		VkContext &operator=(VkContext &&) = delete;

		void init(int width, int height, const char *title);
		void destroy() noexcept;

		// Getters
		GLFWwindow *window() const { return window_; }
		VkInstance instance() const { return instance_; }
		VkSurfaceKHR surface() const { return surface_; }
		VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
		VkDevice device() const { return device_; }
		VkQueue graphicsQueue() const { return graphicsQueue_; }
		VkQueue presentQueue() const { return presentQueue_; }
		QueueFamilyIndices indices() const { return indices_; }

		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;

	private:
		void initWindow_(int width, int height, const char *title);
		void initInstanceAndSurface_();
		void pickPhysicalDevice_();
		void createLogicalDevice_();

	private:
		bool glfwInited_ = false;
		GLFWwindow *window_ = nullptr;

		VkInstance instance_ = VK_NULL_HANDLE;
		VkSurfaceKHR surface_ = VK_NULL_HANDLE;

		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		VkDevice device_ = VK_NULL_HANDLE;

		VkQueue graphicsQueue_ = VK_NULL_HANDLE;
		VkQueue presentQueue_ = VK_NULL_HANDLE;

		QueueFamilyIndices indices_{};
	};

} // namespace scop::vk
