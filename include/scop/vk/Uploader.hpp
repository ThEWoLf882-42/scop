#pragma once
#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class Uploader
	{
	public:
		Uploader() = default;

		Uploader(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue)
		{
			create(device, queueFamilyIndex, queue);
		}

		~Uploader() noexcept { reset(); }

		Uploader(const Uploader &) = delete;
		Uploader &operator=(const Uploader &) = delete;

		Uploader(Uploader &&other) noexcept { *this = std::move(other); }
		Uploader &operator=(Uploader &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				queue_ = other.queue_;
				pool_ = other.pool_;

				other.device_ = VK_NULL_HANDLE;
				other.queue_ = VK_NULL_HANDLE;
				other.pool_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device, uint32_t queueFamilyIndex, VkQueue queue);
		void reset() noexcept;

		void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkQueue queue_ = VK_NULL_HANDLE;
		VkCommandPool pool_ = VK_NULL_HANDLE;
	};

}
