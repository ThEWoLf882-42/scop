#include "scop/vk/Buffer.hpp"

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
		throw std::runtime_error("findMemoryType failed");
	}

	static AllocatedBuffer createBuffer(VkDevice device,
										VkPhysicalDevice physicalDevice,
										VkDeviceSize size,
										VkBufferUsageFlags usage,
										VkMemoryPropertyFlags properties)
	{
		AllocatedBuffer out{};

		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = usage;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bci, nullptr, &out.buffer) != VK_SUCCESS)
			throw std::runtime_error("vkCreateBuffer failed");

		VkMemoryRequirements req{};
		vkGetBufferMemoryRequirements(device, out.buffer, &req);

		VkMemoryAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &ai, nullptr, &out.memory) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateMemory failed");

		vkBindBufferMemory(device, out.buffer, out.memory, 0);
		return out;
	}

	void VertexBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice,
							  const std::vector<Vertex> &vertices)
	{
		reset();

		if (vertices.empty())
			throw std::runtime_error("VertexBuffer: vertices is empty");

		device_ = device;
		physicalDevice_ = physicalDevice;
		count_ = static_cast<uint32_t>(vertices.size());

		const VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

		buf_ = createBuffer(
			device_,
			physicalDevice_,
			size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void *data = nullptr;
		vkMapMemory(device_, buf_.memory, 0, size, 0, &data);
		std::memcpy(data, vertices.data(), static_cast<size_t>(size));
		vkUnmapMemory(device_, buf_.memory);
	}

	void VertexBuffer::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (buf_.buffer)
				vkDestroyBuffer(device_, buf_.buffer, nullptr);
			if (buf_.memory)
				vkFreeMemory(device_, buf_.memory, nullptr);
		}
		buf_ = AllocatedBuffer{};
		count_ = 0;
		device_ = VK_NULL_HANDLE;
		physicalDevice_ = VK_NULL_HANDLE;
	}

}
