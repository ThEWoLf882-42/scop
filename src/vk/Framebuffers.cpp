#include "scop/vk/Framebuffers.hpp"
#include <stdexcept>

namespace scop::vk
{

	std::vector<VkFramebuffer> createFramebuffers(
		VkDevice device,
		VkRenderPass renderPass,
		const std::vector<VkImageView> &imageViews,
		VkExtent2D extent)
	{
		std::vector<VkFramebuffer> out;
		out.resize(imageViews.size());

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

			if (vkCreateFramebuffer(device, &ci, nullptr, &out[i]) != VK_SUCCESS)
				throw std::runtime_error("vkCreateFramebuffer failed");
		}

		return out;
	}

	void destroyFramebuffers(VkDevice device, std::vector<VkFramebuffer> &framebuffers)
	{
		for (auto fb : framebuffers)
		{
			if (fb)
				vkDestroyFramebuffer(device, fb, nullptr);
		}
		framebuffers.clear();
	}

} // namespace scop::vk
