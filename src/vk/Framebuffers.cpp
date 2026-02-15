#include "scop/vk/Framebuffers.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Framebuffers::create(VkDevice device, VkRenderPass renderPass,
							  const std::vector<VkImageView> &colorImageViews,
							  VkImageView depthView,
							  VkExtent2D extent)
	{
		reset();
		device_ = device;

		if (colorImageViews.empty())
			throw std::runtime_error("Framebuffers: no swapchain image views");

		fbs_.resize(colorImageViews.size());

		for (size_t i = 0; i < colorImageViews.size(); ++i)
		{
			VkImageView attachments[] = {colorImageViews[i], depthView};

			VkFramebufferCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			ci.renderPass = renderPass;
			ci.attachmentCount = 2;
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

}
