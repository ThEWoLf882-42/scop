#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/vk/Swapchain.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

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

	static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
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

	static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &modes)
	{
		for (auto m : modes)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D chooseExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR &caps)
	{
		if (caps.currentExtent.width != UINT32_MAX)
			return caps.currentExtent;

		int w = 0, h = 0;
		glfwGetFramebufferSize(window, &w, &h);

		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(w);
		extent.height = static_cast<uint32_t>(h);

		extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
		extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
		return extent;
	}

}

namespace scop::vk
{

	void Swapchain::create(VkContext &ctx)
	{
		reset();

		device_ = ctx.device();

		const auto sc = querySwapChainSupport(ctx.physicalDevice(), ctx.surface());
		if (sc.formats.empty() || sc.presentModes.empty())
			throw std::runtime_error("Swapchain support incomplete");

		const auto surfaceFormat = chooseSurfaceFormat(sc.formats);
		const auto presentMode = choosePresentMode(sc.presentModes);
		const auto extent = chooseExtent(ctx.window(), sc.capabilities);

		uint32_t imageCount = sc.capabilities.minImageCount + 1;
		if (sc.capabilities.maxImageCount > 0 && imageCount > sc.capabilities.maxImageCount)
			imageCount = sc.capabilities.maxImageCount;

		const auto indices = ctx.indices();
		uint32_t qfi[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		VkSwapchainCreateInfoKHR ci{};
		ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		ci.surface = ctx.surface();
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

		if (vkCreateSwapchainKHR(ctx.device(), &ci, nullptr, &swapchain_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateSwapchainKHR failed");

		uint32_t realCount = 0;
		vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &realCount, nullptr);
		images_.resize(realCount);
		vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &realCount, images_.data());

		imageFormat_ = surfaceFormat.format;
		extent_ = extent;

		imageViews_.resize(images_.size());
		for (size_t i = 0; i < images_.size(); ++i)
		{
			VkImageViewCreateInfo iv{};
			iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			iv.image = images_[i];
			iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
			iv.format = imageFormat_;
			iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			iv.subresourceRange.baseMipLevel = 0;
			iv.subresourceRange.levelCount = 1;
			iv.subresourceRange.baseArrayLayer = 0;
			iv.subresourceRange.layerCount = 1;

			if (vkCreateImageView(ctx.device(), &iv, nullptr, &imageViews_[i]) != VK_SUCCESS)
				throw std::runtime_error("vkCreateImageView failed");
		}

		std::cout << "Swapchain created: extent=" << extent_.width << "x" << extent_.height
				  << " images=" << images_.size() << "\n";
	}

	void Swapchain::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			for (auto v : imageViews_)
			{
				if (v)
					vkDestroyImageView(device_, v, nullptr);
			}
			imageViews_.clear();
			images_.clear();

			if (swapchain_)
				vkDestroySwapchainKHR(device_, swapchain_, nullptr);
		}

		swapchain_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
		imageFormat_ = VkFormat{};
		extent_ = VkExtent2D{};
	}

}
