#include "scop/vk/Pipeline.hpp"
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
			throw std::runtime_error("Pipeline: failed to open file: " + filename);

		const size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
		file.close();
		return buffer;
	}

	static VkShaderModule createShaderModule(VkDevice device, const std::vector<char> &code)
	{
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = code.size();
		ci.pCode = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule mod = VK_NULL_HANDLE;
		if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
			throw std::runtime_error("Pipeline: vkCreateShaderModule failed");
		return mod;
	}

	void Pipeline::create(VkDevice device,
						  VkFormat colorFormat,
						  VkFormat depthFormat,
						  VkExtent2D extent,
						  const std::string &vertSpvPath,
						  const std::string &fragSpvPath,
						  VkDescriptorSetLayout setLayout,
						  VkPolygonMode polygonMode)
	{
		reset();

		device_ = device;
		colorFormat_ = colorFormat;
		depthFormat_ = depthFormat;
		extent_ = extent;
		vertPath_ = vertSpvPath;
		fragPath_ = fragSpvPath;
		setLayout_ = setLayout;
		polygonMode_ = polygonMode;

		createRenderPass();

		VkPipelineLayoutCreateInfo lci{};
		lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		lci.setLayoutCount = 1;
		lci.pSetLayouts = &setLayout_;

		if (vkCreatePipelineLayout(device_, &lci, nullptr, &layout_) != VK_SUCCESS)
			throw std::runtime_error("Pipeline: vkCreatePipelineLayout failed");

		createPipeline();
	}

	void Pipeline::recreate(VkExtent2D extent, VkPolygonMode polygonMode)
	{
		extent_ = extent;
		polygonMode_ = polygonMode;

		if (pipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device_, pipeline_, nullptr);
			pipeline_ = VK_NULL_HANDLE;
		}
		createPipeline();
	}

	void Pipeline::createRenderPass()
	{
		VkAttachmentDescription color{};
		color.format = colorFormat_;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depth{};
		depth.format = depthFormat_;
		depth.samples = VK_SAMPLE_COUNT_1_BIT;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorRef{};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthRef{};
		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription sub{};
		sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub.colorAttachmentCount = 1;
		sub.pColorAttachments = &colorRef;
		sub.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription atts[2] = {color, depth};

		VkRenderPassCreateInfo rpci{};
		rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpci.attachmentCount = 2;
		rpci.pAttachments = atts;
		rpci.subpassCount = 1;
		rpci.pSubpasses = &sub;
		rpci.dependencyCount = 1;
		rpci.pDependencies = &dep;

		if (vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_) != VK_SUCCESS)
			throw std::runtime_error("Pipeline: vkCreateRenderPass failed");
	}

	void Pipeline::createPipeline()
	{
		const auto vertCode = readFile(vertPath_);
		const auto fragCode = readFile(fragPath_);

		VkShaderModule vert = createShaderModule(device_, vertCode);
		VkShaderModule frag = createShaderModule(device_, fragCode);

		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vert;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = frag;
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
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
		rs.depthClampEnable = VK_FALSE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.polygonMode = polygonMode_;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.depthBiasEnable = VK_FALSE;
		rs.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo ms{};
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo ds{};
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS;
		ds.depthBoundsTestEnable = VK_FALSE;
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
			throw std::runtime_error("Pipeline: vkCreateGraphicsPipelines failed");

		vkDestroyShaderModule(device_, frag, nullptr);
		vkDestroyShaderModule(device_, vert, nullptr);
	}

	void Pipeline::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (pipeline_)
				vkDestroyPipeline(device_, pipeline_, nullptr);
			if (layout_)
				vkDestroyPipelineLayout(device_, layout_, nullptr);
			if (renderPass_)
				vkDestroyRenderPass(device_, renderPass_, nullptr);
		}

		pipeline_ = VK_NULL_HANDLE;
		layout_ = VK_NULL_HANDLE;
		renderPass_ = VK_NULL_HANDLE;
		setLayout_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

}
