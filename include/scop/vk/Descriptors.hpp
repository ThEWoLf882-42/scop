#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace scop::vk
{

	class Descriptors
	{
	public:
		Descriptors() = default;

		Descriptors(VkDevice device,
					const std::vector<VkBuffer> &uboBuffers,
					VkDeviceSize uboRange,
					VkImageView initialView,
					VkSampler initialSampler);

		~Descriptors() noexcept;

		Descriptors(const Descriptors &) = delete;
		Descriptors &operator=(const Descriptors &) = delete;

		Descriptors(Descriptors &&) noexcept;
		Descriptors &operator=(Descriptors &&) noexcept;

		VkDescriptorSetLayout layout() const { return layout_; }
		const std::vector<VkDescriptorSet> &sets() const { return sets_; }

		void updateTexture(VkImageView view, VkSampler sampler);

	private:
		void destroy() noexcept;

		VkDevice device_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
		VkDescriptorPool pool_ = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> sets_;
	};

} // namespace scop::vk
