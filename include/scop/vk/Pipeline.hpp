#pragma once

#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class Pipeline
	{
	public:
		Pipeline() = default;

		Pipeline(VkDevice device, VkFormat swapchainFormat, VkExtent2D extent,
				 const char *vertSpvPath, const char *fragSpvPath)
		{
			create(device, swapchainFormat, extent, vertSpvPath, fragSpvPath);
		}

		~Pipeline() noexcept { reset(); }

		Pipeline(const Pipeline &) = delete;
		Pipeline &operator=(const Pipeline &) = delete;

		Pipeline(Pipeline &&other) noexcept { *this = std::move(other); }
		Pipeline &operator=(Pipeline &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				renderPass_ = other.renderPass_;
				layout_ = other.layout_;
				pipeline_ = other.pipeline_;

				other.device_ = VK_NULL_HANDLE;
				other.renderPass_ = VK_NULL_HANDLE;
				other.layout_ = VK_NULL_HANDLE;
				other.pipeline_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device, VkFormat swapchainFormat, VkExtent2D extent,
					const char *vertSpvPath, const char *fragSpvPath);

		void reset() noexcept;

		VkRenderPass renderPass() const { return renderPass_; }
		VkPipelineLayout layout() const { return layout_; }
		VkPipeline pipeline() const { return pipeline_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkRenderPass renderPass_ = VK_NULL_HANDLE;
		VkPipelineLayout layout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
	};

} // namespace scop::vk
