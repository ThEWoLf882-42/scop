#pragma once

#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class Descriptors
	{
	public:
		Descriptors() = default;

		Descriptors(VkDevice device, VkBuffer uniformBuffer, VkDeviceSize range)
		{
			create(device, uniformBuffer, range);
		}

		~Descriptors() noexcept { reset(); }

		Descriptors(const Descriptors &) = delete;
		Descriptors &operator=(const Descriptors &) = delete;

		Descriptors(Descriptors &&other) noexcept { *this = std::move(other); }
		Descriptors &operator=(Descriptors &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				setLayout_ = other.setLayout_;
				pool_ = other.pool_;
				set_ = other.set_;

				other.device_ = VK_NULL_HANDLE;
				other.setLayout_ = VK_NULL_HANDLE;
				other.pool_ = VK_NULL_HANDLE;
				other.set_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device, VkBuffer uniformBuffer, VkDeviceSize range);
		void reset() noexcept;

		VkDescriptorSetLayout layout() const { return setLayout_; }
		VkDescriptorSet set() const { return set_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool pool_ = VK_NULL_HANDLE;
		VkDescriptorSet set_ = VK_NULL_HANDLE;
	};

}
