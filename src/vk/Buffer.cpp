#include "scop/vk/Buffer.hpp"
#include <cstring>
#include <stdexcept>

namespace scop::vk
{

	static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties mem{};
		vkGetPhysicalDeviceMemoryProperties(phys, &mem);
		for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
				return i;
		}
		throw std::runtime_error("findMemoryType failed");
	}

	static void createBuffer(VkDevice dev, VkPhysicalDevice phys,
							 VkDeviceSize size, VkBufferUsageFlags usage,
							 VkMemoryPropertyFlags props,
							 VkBuffer &outBuf, VkDeviceMemory &outMem)
	{
		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = usage;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(dev, &bci, nullptr, &outBuf) != VK_SUCCESS)
			throw std::runtime_error("vkCreateBuffer failed");

		VkMemoryRequirements req{};
		vkGetBufferMemoryRequirements(dev, outBuf, &req);

		VkMemoryAllocateInfo mai{};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, props);

		if (vkAllocateMemory(dev, &mai, nullptr, &outMem) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateMemory failed");

		vkBindBufferMemory(dev, outBuf, outMem, 0);
	}

	static VkCommandPool makePool(VkDevice dev, uint32_t qf)
	{
		VkCommandPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.queueFamilyIndex = qf;
		ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool{};
		if (vkCreateCommandPool(dev, &ci, nullptr, &pool) != VK_SUCCESS)
			throw std::runtime_error("vkCreateCommandPool failed");
		return pool;
	}

	static VkCommandBuffer beginCmd(VkDevice dev, VkCommandPool pool)
	{
		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;

		VkCommandBuffer cmd{};
		if (vkAllocateCommandBuffers(dev, &ai, &cmd) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateCommandBuffers failed");

		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
			throw std::runtime_error("vkBeginCommandBuffer failed");

		return cmd;
	}

	static void endCmd(VkDevice dev, VkQueue q, VkCommandPool pool, VkCommandBuffer cmd)
	{
		vkEndCommandBuffer(cmd);

		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;

		vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(q);

		vkFreeCommandBuffers(dev, pool, 1, &cmd);
		vkDestroyCommandPool(dev, pool, nullptr);
	}

	static void copyBuffer(VkDevice dev, VkQueue q, uint32_t qf, VkBuffer src, VkBuffer dst, VkDeviceSize size)
	{
		VkCommandPool pool = makePool(dev, qf);
		VkCommandBuffer cmd = beginCmd(dev, pool);

		VkBufferCopy region{};
		region.size = size;
		vkCmdCopyBuffer(cmd, src, dst, 1, &region);

		endCmd(dev, q, pool, cmd);
	}

	VertexBuffer::VertexBuffer(VkDevice device, VkPhysicalDevice phys,
							   uint32_t queueFamily, VkQueue queue,
							   const std::vector<Vertex> &verts)
	{
		device_ = device;
		count_ = static_cast<uint32_t>(verts.size());
		if (count_ == 0)
			return;

		const VkDeviceSize size = sizeof(Vertex) * static_cast<VkDeviceSize>(verts.size());

		VkBuffer staging{};
		VkDeviceMemory stagingMem{};

		createBuffer(device, phys, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
		std::memcpy(mapped, verts.data(), static_cast<size_t>(size));
		vkUnmapMemory(device, stagingMem);

		createBuffer(device, phys, size,
					 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 buffer_, memory_);

		copyBuffer(device, queue, queueFamily, staging, buffer_, size);

		vkDestroyBuffer(device, staging, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);
	}

	VertexBuffer::~VertexBuffer() noexcept { destroy(); }

	VertexBuffer::VertexBuffer(VertexBuffer &&o) noexcept { *this = std::move(o); }
	VertexBuffer &VertexBuffer::operator=(VertexBuffer &&o) noexcept
	{
		if (this != &o)
		{
			destroy();
			device_ = o.device_;
			buffer_ = o.buffer_;
			memory_ = o.memory_;
			count_ = o.count_;
			o.device_ = VK_NULL_HANDLE;
			o.buffer_ = VK_NULL_HANDLE;
			o.memory_ = VK_NULL_HANDLE;
			o.count_ = 0;
		}
		return *this;
	}

	void VertexBuffer::destroy() noexcept
	{
		if (device_ == VK_NULL_HANDLE)
			return;
		if (buffer_)
			vkDestroyBuffer(device_, buffer_, nullptr);
		if (memory_)
			vkFreeMemory(device_, memory_, nullptr);
		device_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		count_ = 0;
	}

	IndexBuffer::IndexBuffer(VkDevice device, VkPhysicalDevice phys,
							 uint32_t queueFamily, VkQueue queue,
							 const std::vector<uint32_t> &idx)
	{
		device_ = device;
		count_ = static_cast<uint32_t>(idx.size());
		if (count_ == 0)
			return;

		const VkDeviceSize size = sizeof(uint32_t) * static_cast<VkDeviceSize>(idx.size());

		VkBuffer staging{};
		VkDeviceMemory stagingMem{};

		createBuffer(device, phys, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
		std::memcpy(mapped, idx.data(), static_cast<size_t>(size));
		vkUnmapMemory(device, stagingMem);

		createBuffer(device, phys, size,
					 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 buffer_, memory_);

		copyBuffer(device, queue, queueFamily, staging, buffer_, size);

		vkDestroyBuffer(device, staging, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);
	}

	IndexBuffer::~IndexBuffer() noexcept { destroy(); }

	IndexBuffer::IndexBuffer(IndexBuffer &&o) noexcept { *this = std::move(o); }
	IndexBuffer &IndexBuffer::operator=(IndexBuffer &&o) noexcept
	{
		if (this != &o)
		{
			destroy();
			device_ = o.device_;
			buffer_ = o.buffer_;
			memory_ = o.memory_;
			count_ = o.count_;
			o.device_ = VK_NULL_HANDLE;
			o.buffer_ = VK_NULL_HANDLE;
			o.memory_ = VK_NULL_HANDLE;
			o.count_ = 0;
		}
		return *this;
	}

	void IndexBuffer::destroy() noexcept
	{
		if (device_ == VK_NULL_HANDLE)
			return;
		if (buffer_)
			vkDestroyBuffer(device_, buffer_, nullptr);
		if (memory_)
			vkFreeMemory(device_, memory_, nullptr);
		device_ = VK_NULL_HANDLE;
		buffer_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		count_ = 0;
	}

} // namespace scop::vk
