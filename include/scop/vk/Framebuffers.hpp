#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	class Framebuffers
	{
	public:
		Framebuffers() = default;

		Framebuffers(VkDevice device, VkRenderPass renderPass,
					 const std::vector<VkImageView> &imageViews,
					 VkExtent2D extent)
		{
			create(device, renderPass, imageViews, extent);
		}

		~Framebuffers() noexcept { reset(); }

		Framebuffers(const Framebuffers &) = delete;
		Framebuffers &operator=(const Framebuffers &) = delete;

		Framebuffers(Framebuffers &&other) noexcept { *this = std::move(other); }
		Framebuffers &operator=(Framebuffers &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				fbs_ = std::move(other.fbs_);
				other.device_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device, VkRenderPass renderPass,
					const std::vector<VkImageView> &imageViews,
					VkExtent2D extent);

		void reset() noexcept;

		const std::vector<VkFramebuffer> &get() const { return fbs_; }
		size_t size() const { return fbs_.size(); }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> fbs_;
	};

} // namespace scop::vk
