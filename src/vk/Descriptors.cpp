#include "scop/vk/Descriptors.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Descriptors::create(VkDevice device,
							 const std::vector<VkBuffer> &uniformBuffers,
							 VkDeviceSize range)
	{
		reset();
		device_ = device;

		if (uniformBuffers.empty())
			throw std::runtime_error("Descriptors: uniformBuffers empty");

		VkDescriptorSetLayoutBinding ubo{};
		ubo.binding = 0;
		ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo.descriptorCount = 1;
		ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo lci{};
		lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		lci.bindingCount = 1;
		lci.pBindings = &ubo;

		if (vkCreateDescriptorSetLayout(device_, &lci, nullptr, &setLayout_) != VK_SUCCESS)
			throw std::runtime_error("Descriptors: vkCreateDescriptorSetLayout failed");

		VkDescriptorPoolSize ps{};
		ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ps.descriptorCount = static_cast<uint32_t>(uniformBuffers.size());

		VkDescriptorPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = static_cast<uint32_t>(uniformBuffers.size());
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;

		if (vkCreateDescriptorPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("Descriptors: vkCreateDescriptorPool failed");

		std::vector<VkDescriptorSetLayout> layouts(uniformBuffers.size(), setLayout_);

		VkDescriptorSetAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = pool_;
		ai.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		ai.pSetLayouts = layouts.data();

		sets_.resize(uniformBuffers.size());
		if (vkAllocateDescriptorSets(device_, &ai, sets_.data()) != VK_SUCCESS)
			throw std::runtime_error("Descriptors: vkAllocateDescriptorSets failed");

		std::vector<VkDescriptorBufferInfo> infos(uniformBuffers.size());
		std::vector<VkWriteDescriptorSet> writes(uniformBuffers.size());

		for (size_t i = 0; i < uniformBuffers.size(); ++i)
		{
			infos[i].buffer = uniformBuffers[i];
			infos[i].offset = 0;
			infos[i].range = range;

			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = sets_[i];
			writes[i].dstBinding = 0;
			writes[i].dstArrayElement = 0;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[i].pBufferInfo = &infos[i];
		}

		vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	void Descriptors::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (pool_)
				vkDestroyDescriptorPool(device_, pool_, nullptr);
			if (setLayout_)
				vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
		}
		device_ = VK_NULL_HANDLE;
		pool_ = VK_NULL_HANDLE;
		setLayout_ = VK_NULL_HANDLE;
		sets_.clear();
	}

}
