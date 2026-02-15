#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace scop::vk
{

	// IMPORTANT:
	// - For model rendering: nrm = normal, uv = texcoord
	// - For line rendering:  nrm = color,  uv = {0,0}
	struct Vertex
	{
		float pos[3];
		float nrm[3];
		float uv[2];
	};

	class VertexBuffer
	{
	public:
		VertexBuffer() = default;
		VertexBuffer(VkDevice device, VkPhysicalDevice phys,
					 uint32_t queueFamily, VkQueue queue,
					 const std::vector<Vertex> &verts);

		~VertexBuffer() noexcept;

		VertexBuffer(const VertexBuffer &) = delete;
		VertexBuffer &operator=(const VertexBuffer &) = delete;

		VertexBuffer(VertexBuffer &&) noexcept;
		VertexBuffer &operator=(VertexBuffer &&) noexcept;

		VkBuffer buffer() const { return buffer_; }
		uint32_t count() const { return count_; }

	private:
		void destroy() noexcept;

		VkDevice device_ = VK_NULL_HANDLE;
		VkBuffer buffer_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		uint32_t count_ = 0;
	};

	class IndexBuffer
	{
	public:
		IndexBuffer() = default;
		IndexBuffer(VkDevice device, VkPhysicalDevice phys,
					uint32_t queueFamily, VkQueue queue,
					const std::vector<uint32_t> &idx);

		~IndexBuffer() noexcept;

		IndexBuffer(const IndexBuffer &) = delete;
		IndexBuffer &operator=(const IndexBuffer &) = delete;

		IndexBuffer(IndexBuffer &&) noexcept;
		IndexBuffer &operator=(IndexBuffer &&) noexcept;

		VkBuffer buffer() const { return buffer_; }
		uint32_t count() const { return count_; }

	private:
		void destroy() noexcept;

		VkDevice device_ = VK_NULL_HANDLE;
		VkBuffer buffer_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		uint32_t count_ = 0;
	};

} // namespace scop::vk
