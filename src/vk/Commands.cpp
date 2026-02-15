#include "scop/vk/Commands.hpp"
#include <stdexcept>
#include <utility>

namespace scop::vk
{

	Commands &Commands::operator=(Commands &&other) noexcept
	{
		if (this != &other)
		{
			reset();
			device_ = other.device_;
			pool_ = other.pool_;
			buffers_ = std::move(other.buffers_);
			other.device_ = VK_NULL_HANDLE;
			other.pool_ = VK_NULL_HANDLE;
		}
		return *this;
	}

	void Commands::create(VkDevice device, uint32_t queueFamilyIndex, size_t count)
	{
		reset();
		device_ = device;

		VkCommandPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.queueFamilyIndex = queueFamilyIndex;
		pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("Commands: vkCreateCommandPool failed");

		buffers_.resize(count);

		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool_;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = static_cast<uint32_t>(buffers_.size());

		if (vkAllocateCommandBuffers(device_, &ai, buffers_.data()) != VK_SUCCESS)
			throw std::runtime_error("Commands: vkAllocateCommandBuffers failed");
	}

	void Commands::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (!buffers_.empty() && pool_ != VK_NULL_HANDLE)
				vkFreeCommandBuffers(device_, pool_, static_cast<uint32_t>(buffers_.size()), buffers_.data());
			if (pool_ != VK_NULL_HANDLE)
				vkDestroyCommandPool(device_, pool_, nullptr);
		}
		buffers_.clear();
		pool_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

	void Commands::recordScene(VkRenderPass renderPass,
							   const std::vector<VkFramebuffer> &framebuffers,
							   VkExtent2D extent,
							   VkPipeline modelPipeline,
							   VkPipelineLayout layout,
							   const std::vector<VkDescriptorSet> &sets,
							   VkBuffer modelVB,
							   VkBuffer modelIB,
							   uint32_t indexCount,
							   VkIndexType indexType,
							   VkPipeline linesPipeline,
							   VkBuffer linesVB,
							   uint32_t linesVertexCount)
	{
		if (framebuffers.size() != buffers_.size())
			throw std::runtime_error("Commands::recordScene: framebuffer/buffer count mismatch");
		if (sets.size() != buffers_.size())
			throw std::runtime_error("Commands::recordScene: descriptor set count mismatch");

		for (size_t i = 0; i < buffers_.size(); ++i)
		{
			VkCommandBuffer cmd = buffers_[i];

			vkResetCommandBuffer(cmd, 0);

			VkCommandBufferBeginInfo bi{};
			bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
				throw std::runtime_error("Commands: vkBeginCommandBuffer failed");

			VkClearValue clears[2]{};
			clears[0].color = {{0.05f, 0.06f, 0.09f, 1.0f}};
			clears[1].depthStencil = {1.0f, 0};

			VkRenderPassBeginInfo rbi{};
			rbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			rbi.renderPass = renderPass;
			rbi.framebuffer = framebuffers[i];
			rbi.renderArea.offset = {0, 0};
			rbi.renderArea.extent = extent;
			rbi.clearValueCount = 2;
			rbi.pClearValues = clears;

			vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp{};
			vp.x = 0.f;
			vp.y = 0.f;
			vp.width = static_cast<float>(extent.width);
			vp.height = static_cast<float>(extent.height);
			vp.minDepth = 0.f;
			vp.maxDepth = 1.f;
			vkCmdSetViewport(cmd, 0, 1, &vp);

			VkRect2D sc{};
			sc.offset = {0, 0};
			sc.extent = extent;
			vkCmdSetScissor(cmd, 0, 1, &sc);

			// Bind descriptor once (same layout for both pipelines)
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &sets[i], 0, nullptr);

			// ---- Model ----
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);
			VkDeviceSize off0 = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &modelVB, &off0);
			vkCmdBindIndexBuffer(cmd, modelIB, 0, indexType);
			vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

			// ---- Grid/Axes lines ----
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linesPipeline);
			VkDeviceSize off1 = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &linesVB, &off1);
			vkCmdDraw(cmd, linesVertexCount, 1, 0, 0);

			vkCmdEndRenderPass(cmd);

			if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
				throw std::runtime_error("Commands: vkEndCommandBuffer failed");
		}
	}

}
