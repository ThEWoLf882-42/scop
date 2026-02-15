#include "scop/vk/Framebuffers.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Framebuffers::create(VkDevice device, VkRenderPass renderPass,
							  const std::vector<VkImageView> &imageViews,
							  VkExtent2D extent)
	{
		reset();
		device_ = device;

		fbs_.resize(imageViews.size());

		for (size_t i = 0; i < imageViews.size(); ++i)
		{
			VkImageView attachments[] = {imageViews[i]};

			VkFramebufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			ci.renderPass = renderPass;
			ci.attachmentCount = 1;
			ci.pAttachments = attachments;
			ci.width = extent.width;
			ci.height = extent.height;
			ci.layers = 1;

			if (vkCreateFramebuffer(device_, &ci, nullptr, &fbs_[i]) != VK_SUCCESS)
				throw std::runtime_error("vkCreateFramebuffer failed");
		}
	}

	void Framebuffers::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			for (auto fb : fbs_)
			{
				if (fb)
					vkDestroyFramebuffer(device_, fb, nullptr);
			}
		}
		fbs_.clear();
		device_ = VK_NULL_HANDLE;
	}

} // namespace scop::vk
