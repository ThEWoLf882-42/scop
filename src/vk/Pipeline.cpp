#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Buffer.hpp"
#include <vector>
#include <fstream>
#include <stdexcept>
#include <array>
#include <utility>

namespace scop::vk
{

	static std::vector<char> readFile(const std::string &path)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			throw std::runtime_error("readFile failed: " + path);
		f.seekg(0, std::ios::end);
		const std::streamsize n = f.tellg();
		f.seekg(0, std::ios::beg);
		std::vector<char> out((size_t)n);
		f.read(out.data(), n);
		return out;
	}

	static VkShaderModule makeShader(VkDevice dev, const std::string &path)
	{
		const auto bytes = readFile(path);
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = bytes.size();
		ci.pCode = reinterpret_cast<const uint32_t *>(bytes.data());
		VkShaderModule m{};
		if (vkCreateShaderModule(dev, &ci, nullptr, &m) != VK_SUCCESS)
			throw std::runtime_error("vkCreateShaderModule failed: " + path);
		return m;
	}

	Pipeline::Pipeline(VkDevice device,
					   VkFormat colorFormat,
					   VkFormat depthFormat,
					   VkExtent2D extent,
					   const std::string &vertSpv,
					   const std::string &fragSpv,
					   VkDescriptorSetLayout setLayout,
					   VkPolygonMode polygonMode)
	{
		device_ = device;
		colorFormat_ = colorFormat;
		depthFormat_ = depthFormat;
		extent_ = extent;
		vertPath_ = vertSpv;
		fragPath_ = fragSpv;
		setLayout_ = setLayout;

		// render pass
		VkAttachmentDescription color{};
		color.format = colorFormat_;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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

		VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkSubpassDescription sub{};
		sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub.colorAttachmentCount = 1;
		sub.pColorAttachments = &colorRef;
		sub.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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
			throw std::runtime_error("vkCreateRenderPass failed");

		VkPipelineLayoutCreateInfo lci{};
		lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		lci.setLayoutCount = 1;
		lci.pSetLayouts = &setLayout_;

		if (vkCreatePipelineLayout(device_, &lci, nullptr, &layout_) != VK_SUCCESS)
			throw std::runtime_error("vkCreatePipelineLayout failed");

		create(extent_, polygonMode);
	}

	Pipeline::~Pipeline() noexcept { destroy(); }

	Pipeline::Pipeline(Pipeline &&o) noexcept { *this = std::move(o); }
	Pipeline &Pipeline::operator=(Pipeline &&o) noexcept
	{
		if (this != &o)
		{
			destroy();
			device_ = o.device_;
			colorFormat_ = o.colorFormat_;
			depthFormat_ = o.depthFormat_;
			extent_ = o.extent_;
			vertPath_ = std::move(o.vertPath_);
			fragPath_ = std::move(o.fragPath_);
			setLayout_ = o.setLayout_;
			renderPass_ = o.renderPass_;
			layout_ = o.layout_;
			pipeline_ = o.pipeline_;
			o.device_ = VK_NULL_HANDLE;
			o.renderPass_ = VK_NULL_HANDLE;
			o.layout_ = VK_NULL_HANDLE;
			o.pipeline_ = VK_NULL_HANDLE;
		}
		return *this;
	}

	void Pipeline::destroy() noexcept
	{
		if (device_ == VK_NULL_HANDLE)
			return;
		if (pipeline_)
			vkDestroyPipeline(device_, pipeline_, nullptr);
		if (layout_)
			vkDestroyPipelineLayout(device_, layout_, nullptr);
		if (renderPass_)
			vkDestroyRenderPass(device_, renderPass_, nullptr);
		device_ = VK_NULL_HANDLE;
		pipeline_ = VK_NULL_HANDLE;
		layout_ = VK_NULL_HANDLE;
		renderPass_ = VK_NULL_HANDLE;
	}

	void Pipeline::recreate(VkExtent2D extent, VkPolygonMode polygonMode)
	{
		extent_ = extent;
		if (pipeline_)
		{
			vkDestroyPipeline(device_, pipeline_, nullptr);
			pipeline_ = VK_NULL_HANDLE;
		}
		create(extent_, polygonMode);
	}

	void Pipeline::create(VkExtent2D extent, VkPolygonMode polygonMode)
	{
		VkShaderModule vs = makeShader(device_, vertPath_);
		VkShaderModule fs = makeShader(device_, fragPath_);

		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vs;
		stages[0].pName = "main";

		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fs;
		stages[1].pName = "main";

		VkVertexInputBindingDescription bind{};
		bind.binding = 0;
		bind.stride = sizeof(Vertex);
		bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::array<VkVertexInputAttributeDescription, 3> attrs{};

		attrs[0].binding = 0;
		attrs[0].location = 0;
		attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrs[0].offset = offsetof(Vertex, pos);

		attrs[1].binding = 0;
		attrs[1].location = 1;
		attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrs[1].offset = offsetof(Vertex, nrm);

		attrs[2].binding = 0;
		attrs[2].location = 2;
		attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
		attrs[2].offset = offsetof(Vertex, uv);

		VkPipelineVertexInputStateCreateInfo vin{};
		vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vin.vertexBindingDescriptionCount = 1;
		vin.pVertexBindingDescriptions = &bind;
		vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
		vin.pVertexAttributeDescriptions = attrs.data();

		VkPipelineInputAssemblyStateCreateInfo ia{};
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkViewport vp{};
		vp.width = (float)extent.width;
		vp.height = (float)extent.height;
		vp.maxDepth = 1.f;

		VkRect2D sc{};
		sc.extent = extent;

		VkPipelineViewportStateCreateInfo vps{};
		vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vps.viewportCount = 1;
		vps.pViewports = &vp;
		vps.scissorCount = 1;
		vps.pScissors = &sc;

		VkPipelineRasterizationStateCreateInfo rs{};
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.polygonMode = polygonMode;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rs.lineWidth = 1.f;

		VkPipelineMultisampleStateCreateInfo ms{};
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo ds{};
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		VkPipelineColorBlendAttachmentState cba{};
		cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo cb{};
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.attachmentCount = 1;
		cb.pAttachments = &cba;

		VkGraphicsPipelineCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pci.stageCount = 2;
		pci.pStages = stages;
		pci.pVertexInputState = &vin;
		pci.pInputAssemblyState = &ia;
		pci.pViewportState = &vps;
		pci.pRasterizationState = &rs;
		pci.pMultisampleState = &ms;
		pci.pDepthStencilState = &ds;
		pci.pColorBlendState = &cb;
		pci.layout = layout_;
		pci.renderPass = renderPass_;

		if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateGraphicsPipelines failed");

		vkDestroyShaderModule(device_, vs, nullptr);
		vkDestroyShaderModule(device_, fs, nullptr);
	}

} // namespace scop::vk
