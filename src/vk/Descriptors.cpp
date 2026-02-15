#include "scop/vk/Descriptors.hpp"
#include <stdexcept>

namespace scop::vk
{

	Descriptors::Descriptors(VkDevice device,
							 const std::vector<VkBuffer> &uboBuffers,
							 VkDeviceSize uboRange,
							 VkImageView initialView,
							 VkSampler initialSampler)
	{
		device_ = device;
		const uint32_t n = static_cast<uint32_t>(uboBuffers.size());
		if (n == 0)
			throw std::runtime_error("Descriptors: uboBuffers empty");

		VkDescriptorSetLayoutBinding b0{};
		b0.binding = 0;
		b0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		b0.descriptorCount = 1;
		b0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding b1{};
		b1.binding = 1;
		b1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		b1.descriptorCount = 1;
		b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding bindings[2] = {b0, b1};

		VkDescriptorSetLayoutCreateInfo lci{};
		lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		lci.bindingCount = 2;
		lci.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(device_, &lci, nullptr, &layout_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateDescriptorSetLayout failed");

		VkDescriptorPoolSize sizes[2]{};
		sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sizes[0].descriptorCount = n;
		sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sizes[1].descriptorCount = n;

		VkDescriptorPoolCreateInfo pci{};
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.poolSizeCount = 2;
		pci.pPoolSizes = sizes;
		pci.maxSets = n;

		if (vkCreateDescriptorPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS)
			throw std::runtime_error("vkCreateDescriptorPool failed");

		std::vector<VkDescriptorSetLayout> layouts(n, layout_);
		sets_.resize(n);

		VkDescriptorSetAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = pool_;
		ai.descriptorSetCount = n;
		ai.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(device_, &ai, sets_.data()) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateDescriptorSets failed");

		for (uint32_t i = 0; i < n; ++i)
		{
			VkDescriptorBufferInfo dbi{};
			dbi.buffer = uboBuffers[i];
			dbi.offset = 0;
			dbi.range = uboRange;

			VkDescriptorImageInfo dii{};
			dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			dii.imageView = initialView;
			dii.sampler = initialSampler;

			VkWriteDescriptorSet writes[2]{};

			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = sets_[i];
			writes[0].dstBinding = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].pBufferInfo = &dbi;

			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet = sets_[i];
			writes[1].dstBinding = 1;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].pImageInfo = &dii;

			vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
		}
	}

	Descriptors::~Descriptors() noexcept { destroy(); }

	Descriptors::Descriptors(Descriptors &&o) noexcept { *this = std::move(o); }
	Descriptors &Descriptors::operator=(Descriptors &&o) noexcept
	{
		if (this != &o)
		{
			destroy();
			device_ = o.device_;
			layout_ = o.layout_;
			pool_ = o.pool_;
			sets_ = std::move(o.sets_);
			o.device_ = VK_NULL_HANDLE;
			o.layout_ = VK_NULL_HANDLE;
			o.pool_ = VK_NULL_HANDLE;
		}
		return *this;
	}

	void Descriptors::destroy() noexcept
	{
		if (device_ == VK_NULL_HANDLE)
			return;
		if (pool_)
			vkDestroyDescriptorPool(device_, pool_, nullptr);
		if (layout_)
			vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
		device_ = VK_NULL_HANDLE;
		pool_ = VK_NULL_HANDLE;
		layout_ = VK_NULL_HANDLE;
		sets_.clear();
	}

	void Descriptors::updateTexture(VkImageView view, VkSampler sampler)
	{
		if (device_ == VK_NULL_HANDLE || sets_.empty())
			return;

		VkDescriptorImageInfo dii{};
		dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dii.imageView = view;
		dii.sampler = sampler;

		for (VkDescriptorSet s : sets_)
		{
			VkWriteDescriptorSet w{};
			w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w.dstSet = s;
			w.dstBinding = 1;
			w.descriptorCount = 1;
			w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w.pImageInfo = &dii;
			vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
		}
	}

} // namespace scop::vk
