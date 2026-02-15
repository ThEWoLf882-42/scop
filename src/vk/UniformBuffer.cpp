#include "scop/vk/UniformBuffer.hpp"
#include <cstring>
#include <stdexcept>

namespace scop::vk
{

	static uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
								   uint32_t typeFilter,
								   VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1u << i)) &&
				(memProps.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("UniformBuffer: findMemoryType failed");
	}

	void UniformBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size)
	{
		reset();

		device_ = device;
		physicalDevice_ = physicalDevice;
		size_ = size;

		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size_;
		bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device_, &bci, nullptr, &buffer_) != VK_SUCCESS)
			throw std::runtime_error("UniformBuffer: vkCreateBuffer failed");

		VkMemoryRequirements req{};
		vkGetBufferMemoryRequirements(device_, buffer_, &req);

		VkMemoryAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(
			physicalDevice_,
			req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(device_, &ai, nullptr, &memory_) != VK_SUCCESS)
			throw std::runtime_error("UniformBuffer: vkAllocateMemory failed");

		vkBindBufferMemory(device_, buffer_, memory_, 0);

		if (vkMapMemory(device_, memory_, 0, size_, 0, &mapped_) != VK_SUCCESS)
			throw std::runtime_error("UniformBuffer: vkMapMemory failed");
	}

	void UniformBuffer::update(const void *data, VkDeviceSize bytes)
	{
		if (!mapped_)
			throw std::runtime_error("UniformBuffer: not mapped");
		if (bytes > size_)
			throw std::runtime_error("UniformBuffer: update too large");
		std::memcpy(mapped_, data, static_cast<size_t>(bytes));
	}

	void UniformBuffer::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (mapped_)
			{
				vkUnmapMemory(device_, memory_);
				mapped_ = nullptr;
			}
			if (buffer_)
				vkDestroyBuffer(device_, buffer_, nullptr);
			if (memory_)
				vkFreeMemory(device_, memory_, nullptr);
		}

		device_ = VK_NULL_HANDLE;
		physicalDevice_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		mapped_ = nullptr;
		size_ = 0;
	}

}
