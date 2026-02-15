#include "scop/vk/Descriptors.hpp"
#include <stdexcept>

namespace scop::vk
{

	void Descriptors::create(VkDevice device, VkBuffer uniformBuffer, VkDeviceSize range)
	{
		reset();
		device_ = device;

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
		ps.descriptorCount = 1;

		VkDescriptorPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;

		if (vkCreateDescriptorPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("Descriptors: vkCreateDescriptorPool failed");

		VkDescriptorSetAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = pool_;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &setLayout_;

		if (vkAllocateDescriptorSets(device_, &ai, &set_) != VK_SUCCESS)
			throw std::runtime_error("Descriptors: vkAllocateDescriptorSets failed");

		VkDescriptorBufferInfo bi{};
		bi.buffer = uniformBuffer;
		bi.offset = 0;
		bi.range = range;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set_;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.pBufferInfo = &bi;

		vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
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
		set_ = VK_NULL_HANDLE;
	}

}
