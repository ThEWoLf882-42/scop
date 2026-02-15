#pragma once

#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class DepthResources
	{
	public:
		DepthResources() = default;

		DepthResources(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent)
		{
			create(device, physicalDevice, extent);
		}

		~DepthResources() noexcept { reset(); }

		DepthResources(const DepthResources &) = delete;
		DepthResources &operator=(const DepthResources &) = delete;

		DepthResources(DepthResources &&other) noexcept { *this = std::move(other); }
		DepthResources &operator=(DepthResources &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				physicalDevice_ = other.physicalDevice_;
				format_ = other.format_;
				image_ = other.image_;
				memory_ = other.memory_;
				view_ = other.view_;
				extent_ = other.extent_;

				other.device_ = VK_NULL_HANDLE;
				other.physicalDevice_ = VK_NULL_HANDLE;
				other.format_ = VkFormat{};
				other.image_ = VK_NULL_HANDLE;
				other.memory_ = VK_NULL_HANDLE;
				other.view_ = VK_NULL_HANDLE;
				other.extent_ = VkExtent2D{};
			}
			return *this;
		}

		void create(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent);
		void reset() noexcept;

		VkFormat format() const { return format_; }
		VkImageView view() const { return view_; }
		VkExtent2D extent() const { return extent_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

		VkFormat format_{};
		VkImage image_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkImageView view_ = VK_NULL_HANDLE;
		VkExtent2D extent_{};
	};

}
