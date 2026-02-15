#include "scop/vk/Commands.hpp"
#include <stdexcept>

namespace scop::vk
{

	VkCommandPool createCommandPool(VkDevice device, uint32_t graphicsQueueFamilyIndex)
	{
		VkCommandPool pool = VK_NULL_HANDLE;

		VkCommandPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		ci.queueFamilyIndex = graphicsQueueFamilyIndex;

		if (vkCreateCommandPool(device, &ci, nullptr, &pool) != VK_SUCCESS)
			throw std::runtime_error("vkCreateCommandPool failed");

		return pool;
	}

	void destroyCommandPool(VkDevice device, VkCommandPool &pool)
	{
		if (pool)
			vkDestroyCommandPool(device, pool, nullptr);
		pool = VK_NULL_HANDLE;
	}

	std::vector<VkCommandBuffer> allocateCommandBuffers(
		VkDevice device,
		VkCommandPool pool,
		size_t count)
	{
		std::vector<VkCommandBuffer> out(count);

		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = static_cast<uint32_t>(count);

		if (vkAllocateCommandBuffers(device, &ai, out.data()) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateCommandBuffers failed");

		return out;
	}

	void freeCommandBuffers(
		VkDevice device,
		VkCommandPool pool,
		std::vector<VkCommandBuffer> &buffers)
	{
		if (!buffers.empty())
		{
			vkFreeCommandBuffers(device, pool,
								 static_cast<uint32_t>(buffers.size()),
								 buffers.data());
			buffers.clear();
		}
	}

	void recordTriangleCommandBuffers(
		const std::vector<VkCommandBuffer> &commandBuffers,
		VkRenderPass renderPass,
		const std::vector<VkFramebuffer> &framebuffers,
		VkExtent2D extent,
		VkPipeline pipeline,
		VkBuffer vertexBuffer,
		uint32_t vertexCount)
	{
		if (commandBuffers.size() != framebuffers.size())
			throw std::runtime_error("Command buffers count != framebuffers count");

		for (size_t i = 0; i < commandBuffers.size(); ++i)
		{
			VkCommandBuffer cmd = commandBuffers[i];

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

			VkBuffer buffers[] = {vertexBuffer};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

			vkCmdDraw(cmd, vertexCount, 1, 0, 0);
			vkCmdEndRenderPass(cmd);

			if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
				throw std::runtime_error("vkEndCommandBuffer failed");
		}
	}

} // namespace scop::vk
