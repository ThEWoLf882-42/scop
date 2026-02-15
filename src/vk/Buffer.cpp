#include "scop/vk/Buffer.hpp"
#include "scop/vk/Uploader.hpp"

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

	static void destroyAllocated(VkDevice device, AllocatedBuffer &b)
	{
		if (b.buffer)
			vkDestroyBuffer(device, b.buffer, nullptr);
		if (b.memory)
			vkFreeMemory(device, b.memory, nullptr);
		b.buffer = VK_NULL_HANDLE;
		b.memory = VK_NULL_HANDLE;
	}

	void VertexBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice,
							  Uploader &uploader,
							  const std::vector<Vertex> &vertices)
	{
		reset();

		if (vertices.empty())
			throw std::runtime_error("VertexBuffer: vertices empty");

		device_ = device;
		physicalDevice_ = physicalDevice;
		count_ = static_cast<uint32_t>(vertices.size());

		const VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

		// staging
		AllocatedBuffer staging = createBuffer(
			device_, physicalDevice_, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void *data = nullptr;
		vkMapMemory(device_, staging.memory, 0, size, 0, &data);
		std::memcpy(data, vertices.data(), static_cast<size_t>(size));
		vkUnmapMemory(device_, staging.memory);

		// device local
		buf_ = createBuffer(
			device_, physicalDevice_, size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		uploader.copyBuffer(staging.buffer, buf_.buffer, size);
		destroyAllocated(device_, staging);
	}

	void VertexBuffer::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
			destroyAllocated(device_, buf_);
		buf_ = AllocatedBuffer{};
		count_ = 0;
		device_ = VK_NULL_HANDLE;
		physicalDevice_ = VK_NULL_HANDLE;
	}

	void IndexBuffer::create(VkDevice device, VkPhysicalDevice physicalDevice,
							 Uploader &uploader,
							 const std::vector<uint16_t> &indices)
	{
		reset();

		if (indices.empty())
			throw std::runtime_error("IndexBuffer: indices empty");

		device_ = device;
		physicalDevice_ = physicalDevice;
		count_ = static_cast<uint32_t>(indices.size());

		const VkDeviceSize size = sizeof(indices[0]) * indices.size();

		AllocatedBuffer staging = createBuffer(
			device_, physicalDevice_, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void *data = nullptr;
		vkMapMemory(device_, staging.memory, 0, size, 0, &data);
		std::memcpy(data, indices.data(), static_cast<size_t>(size));
		vkUnmapMemory(device_, staging.memory);

		buf_ = createBuffer(
			device_, physicalDevice_, size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		uploader.copyBuffer(staging.buffer, buf_.buffer, size);
		destroyAllocated(device_, staging);
	}

	void IndexBuffer::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
			destroyAllocated(device_, buf_);
		buf_ = AllocatedBuffer{};
		count_ = 0;
		device_ = VK_NULL_HANDLE;
		physicalDevice_ = VK_NULL_HANDLE;
	}

}
