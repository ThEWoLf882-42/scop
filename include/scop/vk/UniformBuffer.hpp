#pragma once

#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class UniformBuffer
	{
	public:
		UniformBuffer() = default;

		UniformBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size)
		{
			create(device, physicalDevice, size);
		}

		~UniformBuffer() noexcept { reset(); }

		UniformBuffer(const UniformBuffer &) = delete;
		UniformBuffer &operator=(const UniformBuffer &) = delete;

		UniformBuffer(UniformBuffer &&other) noexcept { *this = std::move(other); }
		UniformBuffer &operator=(UniformBuffer &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				physicalDevice_ = other.physicalDevice_;
				buffer_ = other.buffer_;
				memory_ = other.memory_;
				mapped_ = other.mapped_;
				size_ = other.size_;

				other.device_ = VK_NULL_HANDLE;
				other.physicalDevice_ = VK_NULL_HANDLE;
				other.buffer_ = VK_NULL_HANDLE;
				other.memory_ = VK_NULL_HANDLE;
				other.mapped_ = nullptr;
				other.size_ = 0;
			}
			return *this;
		}

		void create(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size);
		void reset() noexcept;

		VkBuffer buffer() const { return buffer_; }
		VkDeviceSize size() const { return size_; }

		void update(const void *data, VkDeviceSize bytes);

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

		VkBuffer buffer_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		void *mapped_ = nullptr;
		VkDeviceSize size_ = 0;
	};

}
