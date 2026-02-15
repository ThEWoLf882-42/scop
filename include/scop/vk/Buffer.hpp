#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "scop/vk/Vertex.hpp"

namespace scop::vk
{

	struct AllocatedBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	class VertexBuffer
	{
	public:
		VertexBuffer() = default;

		VertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
					 const std::vector<Vertex> &vertices)
		{
			create(device, physicalDevice, vertices);
		}

		~VertexBuffer() noexcept { reset(); }

		VertexBuffer(const VertexBuffer &) = delete;
		VertexBuffer &operator=(const VertexBuffer &) = delete;

		VertexBuffer(VertexBuffer &&other) noexcept { *this = std::move(other); }
		VertexBuffer &operator=(VertexBuffer &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				buf_ = other.buf_;
				count_ = other.count_;

				other.device_ = VK_NULL_HANDLE;
				other.buf_ = AllocatedBuffer{};
				other.count_ = 0;
			}
			return *this;
		}

		void create(VkDevice device, VkPhysicalDevice physicalDevice,
					const std::vector<Vertex> &vertices);

		void reset() noexcept;

		VkBuffer buffer() const { return buf_.buffer; }
		uint32_t count() const { return count_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

		AllocatedBuffer buf_{};
		uint32_t count_ = 0;
	};

} // namespace scop::vk
