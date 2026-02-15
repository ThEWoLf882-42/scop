#pragma once

#include <string>
#include <utility>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	class PipelineVariant
	{
	public:
		PipelineVariant() = default;

		PipelineVariant(VkDevice device,
						VkRenderPass renderPass,
						VkPipelineLayout layout,
						VkFormat depthFormat,
						VkExtent2D extent,
						const std::string &vertSpv,
						const std::string &fragSpv,
						VkPrimitiveTopology topology,
						bool depthWrite,
						VkCullModeFlags cullMode = VK_CULL_MODE_NONE)
		{
			create(device, renderPass, layout, depthFormat, extent, vertSpv, fragSpv,
				   topology, depthWrite, cullMode);
		}

		~PipelineVariant() noexcept { reset(); }

		PipelineVariant(const PipelineVariant &) = delete;
		PipelineVariant &operator=(const PipelineVariant &) = delete;

		PipelineVariant(PipelineVariant &&other) noexcept { *this = std::move(other); }
		PipelineVariant &operator=(PipelineVariant &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				renderPass_ = other.renderPass_;
				layout_ = other.layout_;
				pipeline_ = other.pipeline_;

				depthFormat_ = other.depthFormat_;
				extent_ = other.extent_;
				vertPath_ = std::move(other.vertPath_);
				fragPath_ = std::move(other.fragPath_);
				topology_ = other.topology_;
				depthWrite_ = other.depthWrite_;
				cullMode_ = other.cullMode_;

				other.device_ = VK_NULL_HANDLE;
				other.renderPass_ = VK_NULL_HANDLE;
				other.layout_ = VK_NULL_HANDLE;
				other.pipeline_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device,
					VkRenderPass renderPass,
					VkPipelineLayout layout,
					VkFormat depthFormat,
					VkExtent2D extent,
					const std::string &vertSpv,
					const std::string &fragSpv,
					VkPrimitiveTopology topology,
					bool depthWrite,
					VkCullModeFlags cullMode);

		void recreate(VkExtent2D extent);

		void reset() noexcept;

		VkPipeline pipeline() const { return pipeline_; }

	private:
		void createPipeline();

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkRenderPass renderPass_ = VK_NULL_HANDLE;
		VkPipelineLayout layout_ = VK_NULL_HANDLE;
		VkPipeline pipeline_ = VK_NULL_HANDLE;

		VkFormat depthFormat_{};
		VkExtent2D extent_{};

		std::string vertPath_;
		std::string fragPath_;
		VkPrimitiveTopology topology_ = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		bool depthWrite_ = true;
		VkCullModeFlags cullMode_ = VK_CULL_MODE_NONE;
	};

}