#include "scop/vk/Uploader.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Uploader::create(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue)
	{
		reset();

		device_ = device;
		queue_ = queue;

		VkCommandPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		pci.queueFamilyIndex = queueFamilyIndex;

		if (vkCreateCommandPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("Uploader: vkCreateCommandPool failed");
	}

	void Uploader::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
	{
		if (!device_ || !pool_ || !queue_)
			throw std::runtime_error("Uploader: not initialized");

		VkCommandBuffer cmd = VK_NULL_HANDLE;

		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool_;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(device_, &ai, &cmd) != VK_SUCCESS)
			throw std::runtime_error("Uploader: vkAllocateCommandBuffers failed");

		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
			throw std::runtime_error("Uploader: vkBeginCommandBuffer failed");

		VkBufferCopy region{};
		region.size = size;
		vkCmdCopyBuffer(cmd, src, dst, 1, &region);

		if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
			throw std::runtime_error("Uploader: vkEndCommandBuffer failed");

		VkSubmitInfo submit{};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;

		if (vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
			throw std::runtime_error("Uploader: vkQueueSubmit failed");

		vkQueueWaitIdle(queue_);

		vkFreeCommandBuffers(device_, pool_, 1, &cmd);
	}

	void Uploader::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE)
		{
			(void)vkQueueWaitIdle(queue_);
			vkDestroyCommandPool(device_, pool_, nullptr);
		}
		pool_ = VK_NULL_HANDLE;
		queue_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

}
