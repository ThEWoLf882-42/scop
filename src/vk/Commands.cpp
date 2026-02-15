#include "scop/vk/Commands.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Commands::create(VkDevice device, uint32_t graphicsQueueFamilyIndex, size_t bufferCount)
	{
		reset();
		device_ = device;

		VkCommandPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pci.queueFamilyIndex = graphicsQueueFamilyIndex;

		if (vkCreateCommandPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateCommandPool failed");

		buffers_.resize(bufferCount);

		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool_;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = static_cast<uint32_t>(buffers_.size());

		if (vkAllocateCommandBuffers(device_, &ai, buffers_.data()) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateCommandBuffers failed");
	}

	void Commands::recordIndexed(
		VkRenderPass renderPass,
		const std::vector<VkFramebuffer> &framebuffers,
		VkExtent2D extent,
		VkPipeline pipeline,
		VkBuffer vertexBuffer,
		VkBuffer indexBuffer,
		uint32_t indexCount)
	{
		if (buffers_.size() != framebuffers.size())
			throw std::runtime_error("Commands: buffers count != framebuffers count");

		for (size_t i = 0; i < buffers_.size(); ++i)
		{
			VkCommandBuffer cmd = buffers_[i];

			VkCommandBufferBeginInfo bi{};
			bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
				throw std::runtime_error("vkBeginCommandBuffer failed");

			VkClearValue clear{};
			clear.color = {{0.05f, 0.08f, 0.12f, 1.0f}};

			VkRenderPassBeginInfo rp{};
			rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rp.renderPass = renderPass;
			rp.framebuffer = framebuffers[i];
			rp.renderArea.offset = {0, 0};
			rp.renderArea.extent = extent;
			rp.clearValueCount = 1;
			rp.pClearValues = &clear;

			vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkBuffer vbufs[] = {vertexBuffer};
			VkDeviceSize offs[] = {0};
			vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offs);

			vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(cmd);

			if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
				throw std::runtime_error("vkEndCommandBuffer failed");
		}
	}

	void Commands::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (!buffers_.empty() && pool_ != VK_NULL_HANDLE)
			{
				vkFreeCommandBuffers(device_, pool_,
									 static_cast<uint32_t>(buffers_.size()),
									 buffers_.data());
			}
			buffers_.clear();

			if (pool_)
				vkDestroyCommandPool(device_, pool_, nullptr);
		}

		pool_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

}
