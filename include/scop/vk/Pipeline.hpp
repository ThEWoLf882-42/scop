#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace scop::vk
{

	class Pipeline
	{
	public:
		Pipeline() = default;

		Pipeline(VkDevice device,
				 VkFormat colorFormat,
				 VkFormat depthFormat,
				 VkExtent2D extent,
				 const std::string &vertSpv,
				 const std::string &fragSpv,
				 VkDescriptorSetLayout setLayout,
				 VkPolygonMode polygonMode);

		~Pipeline() noexcept;

		Pipeline(const Pipeline &) = delete;
		Pipeline &operator=(const Pipeline &) = delete;

		Pipeline(Pipeline &&) noexcept;
		Pipeline &operator=(Pipeline &&) noexcept;

		void recreate(VkExtent2D extent, VkPolygonMode polygonMode);

		VkRenderPass renderPass() const { return renderPass_; }
		VkPipelineLayout layout() const { return layout_; }
		VkPipeline pipeline() const { return pipeline_; }

	private:
		void destroy() noexcept;
		void create(VkExtent2D extent, VkPolygonMode polygonMode);

		VkDevice device_ = VK_NULL_HANDLE;
		VkFormat colorFormat_{};
		VkFormat depthFormat_{};
		VkExtent2D extent_{};

		std::string vertPath_;
		std::string fragPath_;
		VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;

		VkRenderPass renderPass_ = VK_NULL_HANDLE;
		VkPipelineLayout layout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;
	};

} // namespace scop::vk
