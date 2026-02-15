#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	class Commands
	{
	public:
		Commands() = default;
		Commands(VkDevice device, uint32_t queueFamilyIndex, size_t count) { create(device, queueFamilyIndex, count); }
		~Commands() noexcept { reset(); }

		Commands(const Commands &) = delete;
		Commands &operator=(const Commands &) = delete;

		Commands(Commands &&other) noexcept { *this = std::move(other); }
		Commands &operator=(Commands &&other) noexcept;

		void create(VkDevice device, uint32_t queueFamilyIndex, size_t count);
		void reset() noexcept;

		const std::vector<VkCommandBuffer> &buffers() const { return buffers_; }

		void recordScene(VkRenderPass renderPass,
						 const std::vector<VkFramebuffer> &framebuffers,
						 VkExtent2D extent,
						 VkPipeline modelPipeline,
						 VkPipelineLayout layout,
						 const std::vector<VkDescriptorSet> &sets,
						 VkBuffer modelVB,
						 VkBuffer modelIB,
						 uint32_t indexCount,
						 VkIndexType indexType,
						 VkPipeline linesPipeline,
						 VkBuffer linesVB,
						 uint32_t linesVertexCount);

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkCommandPool pool_ = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> buffers_;
	};

}
