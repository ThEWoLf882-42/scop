#include "scop/vk/Depth.hpp"
#include <stdexcept>

namespace scop::vk
{

	static bool hasStencil(VkFormat f)
	{
		return f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	static VkFormat findSupportedFormat(
		VkPhysicalDevice physicalDevice,
		const VkFormat *candidates,
		size_t candidateCount,
		VkImageTiling tiling,
		VkFormatFeatureFlags features)
	{
		for (size_t i = 0; i < candidateCount; ++i)
		{
			VkFormatProperties props{};
			vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &props);

			if (tiling == VK_IMAGE_TILING_LINEAR)
			{
				if ((props.linearTilingFeatures & features) == features)
					return candidates[i];
			}
			else
			{
				if ((props.optimalTilingFeatures & features) == features)
					return candidates[i];
			}
		}
		throw std::runtime_error("DepthResources: no supported depth format");
	}

	static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
	{
		const VkFormat candidates[] = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT};
		return findSupportedFormat(
			physicalDevice,
			candidates, sizeof(candidates) / sizeof(candidates[0]),
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1u << i)) &&
				(memProps.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("DepthResources: findMemoryType failed");
	}

	void DepthResources::create(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent)
	{
		reset();

		device_ = device;
		physicalDevice_ = physicalDevice;
		extent_ = extent;

		format_ = findDepthFormat(physicalDevice_);

		VkImageCreateInfo ici{};
		ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.extent.width = extent_.width;
		ici.extent.height = extent_.height;
		ici.extent.depth = 1;
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.format = format_;
		ici.tiling = VK_IMAGE_TILING_OPTIMAL;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device_, &ici, nullptr, &image_) != VK_SUCCESS)
			throw std::runtime_error("DepthResources: vkCreateImage failed");

		VkMemoryRequirements req{};
		vkGetImageMemoryRequirements(device_, image_, &req);

		VkMemoryAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(physicalDevice_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(device_, &ai, nullptr, &memory_) != VK_SUCCESS)
			throw std::runtime_error("DepthResources: vkAllocateMemory failed");

		vkBindImageMemory(device_, image_, memory_, 0);

		VkImageViewCreateInfo vci{};
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = image_;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = format_;
		vci.subresourceRange.baseMipLevel = 0;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.baseArrayLayer = 0;
		vci.subresourceRange.layerCount = 1;

		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencil(format_))
			vci.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		if (vkCreateImageView(device_, &vci, nullptr, &view_) != VK_SUCCESS)
			throw std::runtime_error("DepthResources: vkCreateImageView failed");
	}

	void DepthResources::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (view_)
				vkDestroyImageView(device_, view_, nullptr);
			if (image_)
				vkDestroyImage(device_, image_, nullptr);
			if (memory_)
				vkFreeMemory(device_, memory_, nullptr);
		}

		view_ = VK_NULL_HANDLE;
		image_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		format_ = VkFormat{};
		extent_ = VkExtent2D{};

		device_ = VK_NULL_HANDLE;
		physicalDevice_ = VK_NULL_HANDLE;
	}

}
