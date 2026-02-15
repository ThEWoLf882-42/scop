#include "scop/vk/PipelineVariant.hpp"
#include "scop/vk/Vertex.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace scop::vk
{

	static std::vector<char> readFile(const std::string &filename)
	{
		std::ifstream file(filename.c_str(), std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("PipelineVariant: failed to open file: " + filename);

		const size_t size = static_cast<size_t>(file.tellg());
		std::vector<char> buf(size);
		file.seekg(0);
		file.read(buf.data(), static_cast<std::streamsize>(size));
		return buf;
	}

	static VkShaderModule createShaderModule(VkDevice device, const std::vector<char> &code)
	{
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = code.size();
		ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule mod = VK_NULL_HANDLE;
		if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
			throw std::runtime_error("PipelineVariant: vkCreateShaderModule failed");
		return mod;
	}

	void PipelineVariant::create(VkDevice device,
								 VkRenderPass renderPass,
								 VkPipelineLayout layout,
								 VkFormat depthFormat,
								 VkExtent2D extent,
								 const std::string &vertSpv,
								 const std::string &fragSpv,
								 VkPrimitiveTopology topology,
								 bool depthWrite,
								 VkCullModeFlags cullMode)
	{
		reset();
		device_ = device;
		renderPass_ = renderPass;
		layout_ = layout;
		depthFormat_ = depthFormat;
		extent_ = extent;
		vertPath_ = vertSpv;
		fragPath_ = fragSpv;
		topology_ = topology;
		depthWrite_ = depthWrite;
		cullMode_ = cullMode;

		createPipeline();
	}

	void PipelineVariant::recreate(VkExtent2D extent)
	{
		extent_ = extent;
		if (pipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device_, pipeline_, nullptr);
			pipeline_ = VK_NULL_HANDLE;
		}
		createPipeline();
	}

	void PipelineVariant::createPipeline()
	{
		const auto vcode = readFile(vertPath_);
		const auto fcode = readFile(fragPath_);

		VkShaderModule vmod = createShaderModule(device_, vcode);
		VkShaderModule fmod = createShaderModule(device_, fcode);

		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vmod;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fmod;
		stages[1].pName = "main";

		const auto binding = Vertex::bindingDescription();
		const auto attrs = Vertex::attributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vi{};
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.vertexBindingDescriptionCount = 1;
		vi.pVertexBindingDescriptions = &binding;
		vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
		vi.pVertexAttributeDescriptions = attrs.data();

		VkPipelineInputAssemblyStateCreateInfo ia{};
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.topology = topology_;
		ia.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(extent_.width);
		viewport.height = static_cast<float>(extent_.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = extent_;

		VkPipelineViewportStateCreateInfo vp{};
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		vp.pViewports = &viewport;
		vp.scissorCount = 1;
		vp.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rs{};
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = cullMode_;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo ms{};
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo ds{};
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = depthWrite_ ? VK_TRUE : VK_FALSE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS;
		ds.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState cba{};
		cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
							 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		cba.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo cb{};
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.attachmentCount = 1;
		cb.pAttachments = &cba;

		VkGraphicsPipelineCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pci.stageCount = 2;
		pci.pStages = stages;
		pci.pVertexInputState = &vi;
		pci.pInputAssemblyState = &ia;
		pci.pViewportState = &vp;
		pci.pRasterizationState = &rs;
		pci.pMultisampleState = &ms;
		pci.pDepthStencilState = &ds;
		pci.pColorBlendState = &cb;
		pci.layout = layout_;
		pci.renderPass = renderPass_;
		pci.subpass = 0;

		if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) != VK_SUCCESS)
			throw std::runtime_error("PipelineVariant: vkCreateGraphicsPipelines failed");

		vkDestroyShaderModule(device_, fmod, nullptr);
		vkDestroyShaderModule(device_, vmod, nullptr);
	}

	void PipelineVariant::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device_, pipeline_, nullptr);
		}
		pipeline_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
		renderPass_ = VK_NULL_HANDLE;
		layout_ = VK_NULL_HANDLE;
	}

}
