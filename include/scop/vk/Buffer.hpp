#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

#include "scop/vk/Vertex.hpp"

namespace scop::vk
{

	class Uploader;

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
					 Uploader &uploader,
					 const std::vector<Vertex> &vertices)
		{
			create(device, physicalDevice, uploader, vertices);
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
				physicalDevice_ = other.physicalDevice_;
				buf_ = other.buf_;
				count_ = other.count_;

				other.device_ = VK_NULL_HANDLE;
				other.physicalDevice_ = VK_NULL_HANDLE;
				other.buf_ = AllocatedBuffer{};
				other.count_ = 0;
			}
			return *this;
		}

		void create(VkDevice device, VkPhysicalDevice physicalDevice,
					Uploader &uploader,
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

	class IndexBuffer
	{
	public:
		IndexBuffer() = default;

		IndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
					Uploader &uploader,
					const std::vector<uint16_t> &indices)
		{
			create(device, physicalDevice, uploader, indices);
		}

		~IndexBuffer() noexcept { reset(); }

		IndexBuffer(const IndexBuffer &) = delete;
		IndexBuffer &operator=(const IndexBuffer &) = delete;

		IndexBuffer(IndexBuffer &&other) noexcept { *this = std::move(other); }
		IndexBuffer &operator=(IndexBuffer &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				physicalDevice_ = other.physicalDevice_;
				buf_ = other.buf_;
				count_ = other.count_;

				other.device_ = VK_NULL_HANDLE;
				other.physicalDevice_ = VK_NULL_HANDLE;
				other.buf_ = AllocatedBuffer{};
				other.count_ = 0;
			}
			return *this;
		}

		void create(VkDevice device, VkPhysicalDevice physicalDevice,
					Uploader &uploader,
					const std::vector<uint16_t> &indices);

		void reset() noexcept;

		VkBuffer buffer() const { return buf_.buffer; }
		uint32_t count() const { return count_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		AllocatedBuffer buf_{};
		uint32_t count_ = 0;
	};

}
