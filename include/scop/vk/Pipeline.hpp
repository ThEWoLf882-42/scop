#pragma once

#include <string>
#include <utility>
#include <vulkan/vulkan.h>

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
				 const std::string &vertSpvPath,
				 const std::string &fragSpvPath,
				 VkDescriptorSetLayout setLayout,
				 VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL)
		{
			create(device, colorFormat, depthFormat, extent, vertSpvPath, fragSpvPath, setLayout, polygonMode);
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

				colorFormat_ = other.colorFormat_;
				depthFormat_ = other.depthFormat_;
				extent_ = other.extent_;
				vertPath_ = std::move(other.vertPath_);
				fragPath_ = std::move(other.fragPath_);
				setLayout_ = other.setLayout_;
				polygonMode_ = other.polygonMode_;

				other.device_ = VK_NULL_HANDLE;
				other.renderPass_ = VK_NULL_HANDLE;
				other.layout_ = VK_NULL_HANDLE;
				other.pipeline_ = VK_NULL_HANDLE;
				other.setLayout_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device,
					VkFormat colorFormat,
					VkFormat depthFormat,
					VkExtent2D extent,
					const std::string &vertSpvPath,
					const std::string &fragSpvPath,
					VkDescriptorSetLayout setLayout,
					VkPolygonMode polygonMode);

		void recreate(VkExtent2D extent, VkPolygonMode polygonMode);

		void reset() noexcept;

		VkRenderPass renderPass() const { return renderPass_; }
		VkPipelineLayout layout() const { return layout_; }
		VkPipeline pipeline() const { return pipeline_; }

	private:
		void createRenderPass();
		void createPipeline();

	private:
		VkDevice device_ = VK_NULL_HANDLE;

		VkRenderPass renderPass_ = VK_NULL_HANDLE;
		VkPipelineLayout layout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		VkFormat colorFormat_{};
		VkFormat depthFormat_{};
		VkExtent2D extent_{};

		std::string vertPath_;
		std::string fragPath_;
		VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
		VkPolygonMode polygonMode_ = VK_POLYGON_MODE_FILL;
	};

}
