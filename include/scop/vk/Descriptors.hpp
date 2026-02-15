#pragma once

#include <vulkan/vulkan.h>
#include <utility>
#include <vector>

namespace scop::vk
{

	class Descriptors
	{
	public:
		Descriptors() = default;

		Descriptors(VkDevice device,
					const std::vector<VkBuffer> &uniformBuffers,
					VkDeviceSize range)
		{
			create(device, uniformBuffers, range);
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
				sets_ = std::move(other.sets_);

				other.device_ = VK_NULL_HANDLE;
				other.setLayout_ = VK_NULL_HANDLE;
				other.pool_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device,
					const std::vector<VkBuffer> &uniformBuffers,
					VkDeviceSize range);

		void reset() noexcept;

		VkDescriptorSetLayout layout() const { return setLayout_; }
		VkDescriptorSet set(size_t i) const { return sets_.at(i); }
		const std::vector<VkDescriptorSet> &sets() const { return sets_; }
		size_t count() const { return sets_.size(); }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool pool_ = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> sets_;
	};

}
