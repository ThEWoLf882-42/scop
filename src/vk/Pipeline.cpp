#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Vertex.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

	static std::vector<char> readFile(const char *filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error(std::string("Failed to open file: ") + filename);

		const std::streamsize size = file.tellg();
		std::vector<char> buffer(static_cast<size_t>(size));
		file.seekg(0);
		file.read(buffer.data(), size);
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
			throw std::runtime_error("vkCreateShaderModule failed");
		return mod;
	}

}

namespace scop::vk
{

	void Pipeline::create(VkDevice device, VkFormat swapchainFormat, VkExtent2D extent,
						  const char *vertSpvPath, const char *fragSpvPath)
	{
		reset();
		device_ = device;

		VkAttachmentDescription color{};
		color.format = swapchainFormat;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorRef{};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo rp{};
		rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rp.attachmentCount = 1;
		rp.pAttachments = &color;
		rp.subpassCount = 1;
		rp.pSubpasses = &subpass;
		rp.dependencyCount = 1;
		rp.pDependencies = &dep;

		if (vkCreateRenderPass(device_, &rp, nullptr, &renderPass_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateRenderPass failed");

		auto vertCode = readFile(vertSpvPath);
		auto fragCode = readFile(fragSpvPath);

		VkShaderModule vert = createShaderModule(device_, vertCode);
		VkShaderModule frag = createShaderModule(device_, fragCode);

		VkPipelineShaderStageCreateInfo vertStage{};
		vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStage.module = vert;
		vertStage.pName = "main";

		VkPipelineShaderStageCreateInfo fragStage{};
		fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.module = frag;
		fragStage.pName = "main";

		VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

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

		VkViewport viewport{};
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.extent = extent;

		VkPipelineViewportStateCreateInfo vp{};
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		vp.pViewports = &viewport;
		vp.scissorCount = 1;
		vp.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rs{};
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.lineWidth = 1.0f;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

		VkPipelineMultisampleStateCreateInfo ms{};
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState cba{};
		cba.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo cb{};
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.attachmentCount = 1;
		cb.pAttachments = &cba;

		VkPipelineLayoutCreateInfo pl{};
		pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		if (vkCreatePipelineLayout(device_, &pl, nullptr, &layout_) != VK_SUCCESS)
			throw std::runtime_error("vkCreatePipelineLayout failed");

		VkGraphicsPipelineCreateInfo gp{};
		gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		gp.stageCount = 2;
		gp.pStages = stages;
		gp.pVertexInputState = &vi;
		gp.pInputAssemblyState = &ia;
		gp.pViewportState = &vp;
		gp.pRasterizationState = &rs;
		gp.pMultisampleState = &ms;
		gp.pColorBlendState = &cb;
		gp.layout = layout_;
		gp.renderPass = renderPass_;
		gp.subpass = 0;

		if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateGraphicsPipelines failed");

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
		device_ = VK_NULL_HANDLE;
	}

}
