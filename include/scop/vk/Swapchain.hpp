#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "scop/vk/VkContext.hpp"

namespace scop::vk
{

	class Swapchain
	{
	public:
		Swapchain() = default;
		explicit Swapchain(VkContext &ctx) { create(ctx); }
		~Swapchain() noexcept { reset(); }

		Swapchain(const Swapchain &) = delete;
		Swapchain &operator=(const Swapchain &) = delete;

		Swapchain(Swapchain &&other) noexcept { *this = std::move(other); }
		Swapchain &operator=(Swapchain &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				swapchain_ = other.swapchain_;
				images_ = std::move(other.images_);
				imageViews_ = std::move(other.imageViews_);
				imageFormat_ = other.imageFormat_;
				extent_ = other.extent_;

				other.device_ = VK_NULL_HANDLE;
				other.swapchain_ = VK_NULL_HANDLE;
				other.imageFormat_ = VkFormat{};
				other.extent_ = VkExtent2D{};
			}
			return *this;
		}

		void create(VkContext &ctx);
		void reset() noexcept;

		VkSwapchainKHR handle() const { return swapchain_; }
		VkFormat imageFormat() const { return imageFormat_; }
		VkExtent2D extent() const { return extent_; }
		const std::vector<VkImageView> &imageViews() const { return imageViews_; }
		size_t size() const { return imageViews_.size(); }

	private:
		VkDevice device_ = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		std::vector<VkImage> images_;
		std::vector<VkImageView> imageViews_;
		VkFormat imageFormat_{};
		VkExtent2D extent_{};
	};

} // namespace scop::vk
