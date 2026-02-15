#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	class Commands
	{
	public:
		Commands() = default;

		Commands(VkDevice device, uint32_t graphicsQueueFamilyIndex, size_t bufferCount)
		{
			create(device, graphicsQueueFamilyIndex, bufferCount);
		}

		~Commands() noexcept { reset(); }

		Commands(const Commands &) = delete;
		Commands &operator=(const Commands &) = delete;

		Commands(Commands &&other) noexcept { *this = std::move(other); }
		Commands &operator=(Commands &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				pool_ = other.pool_;
				buffers_ = std::move(other.buffers_);

				other.device_ = VK_NULL_HANDLE;
				other.pool_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device, uint32_t graphicsQueueFamilyIndex, size_t bufferCount);
		void reset() noexcept;

		void recordTriangle(
			VkRenderPass renderPass,
			const std::vector<VkFramebuffer> &framebuffers,
			VkExtent2D extent,
			VkPipeline pipeline,
			VkBuffer vertexBuffer,
			uint32_t vertexCount);

		const std::vector<VkCommandBuffer> &buffers() const { return buffers_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkCommandPool pool_ = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> buffers_;
	};

}
