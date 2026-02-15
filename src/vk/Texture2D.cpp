#include "scop/vk/Texture2D.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

// You must add this file:
// include/scop/third_party/stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "scop/third_party/stb_image.h"

namespace scop::vk
{

	static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props)
	{
		VkPhysicalDeviceMemoryProperties mem{};
		vkGetPhysicalDeviceMemoryProperties(phys, &mem);
		for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
				return i;
		}
		throw std::runtime_error("findMemoryType failed");
	}

	static VkCommandPool makePool(VkDevice dev, uint32_t qf)
	{
		VkCommandPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.queueFamilyIndex = qf;
		ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool{};
		if (vkCreateCommandPool(dev, &ci, nullptr, &pool) != VK_SUCCESS)
			throw std::runtime_error("vkCreateCommandPool failed");
		return pool;
	}

	static VkCommandBuffer beginCmd(VkDevice dev, VkCommandPool pool)
	{
		VkCommandBufferAllocateInfo ai{};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;

		VkCommandBuffer cmd{};
		if (vkAllocateCommandBuffers(dev, &ai, &cmd) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateCommandBuffers failed");

		VkCommandBufferBeginInfo bi{};
		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
			throw std::runtime_error("vkBeginCommandBuffer failed");
		return cmd;
	}

	static void endCmd(VkDevice dev, VkQueue q, VkCommandPool pool, VkCommandBuffer cmd)
	{
		vkEndCommandBuffer(cmd);

		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;

		vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
		vkQueueWaitIdle(q);

		vkFreeCommandBuffers(dev, pool, 1, &cmd);
		vkDestroyCommandPool(dev, pool, nullptr);
	}

	static void createBuffer(VkDevice dev, VkPhysicalDevice phys,
							 VkDeviceSize size, VkBufferUsageFlags usage,
							 VkMemoryPropertyFlags props,
							 VkBuffer &outBuf, VkDeviceMemory &outMem)
	{
		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = usage;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(dev, &bci, nullptr, &outBuf) != VK_SUCCESS)
			throw std::runtime_error("vkCreateBuffer failed");

		VkMemoryRequirements req{};
		vkGetBufferMemoryRequirements(dev, outBuf, &req);

		VkMemoryAllocateInfo mai{};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, props);

		if (vkAllocateMemory(dev, &mai, nullptr, &outMem) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateMemory failed");

		vkBindBufferMemory(dev, outBuf, outMem, 0);
	}

	static void transition(VkCommandBuffer cmd, VkImage img, VkImageLayout oldL, VkImageLayout newL)
	{
		VkImageMemoryBarrier b{};
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.oldLayout = oldL;
		b.newLayout = newL;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = img;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.baseMipLevel = 0;
		b.subresourceRange.levelCount = 1;
		b.subresourceRange.baseArrayLayer = 0;
		b.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		if (oldL == VK_IMAGE_LAYOUT_UNDEFINED && newL == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			b.srcAccessMask = 0;
			b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldL == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newL == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
	}

	static VkFormat pickFormat(VkPhysicalDevice phys)
	{
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		VkFormatProperties fp{};
		vkGetPhysicalDeviceFormatProperties(phys, format, &fp);
		if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
			return format;
		return VK_FORMAT_R8G8B8A8_UNORM;
	}

	static void createImage(VkDevice dev, VkPhysicalDevice phys, uint32_t w, uint32_t h,
							VkFormat format, VkImage &img, VkDeviceMemory &mem)
	{
		VkImageCreateInfo ici{};
		ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.extent = {w, h, 1};
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.format = format;
		ici.tiling = VK_IMAGE_TILING_OPTIMAL;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(dev, &ici, nullptr, &img) != VK_SUCCESS)
			throw std::runtime_error("vkCreateImage failed");

		VkMemoryRequirements req{};
		vkGetImageMemoryRequirements(dev, img, &req);

		VkMemoryAllocateInfo mai{};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(dev, &mai, nullptr, &mem) != VK_SUCCESS)
			throw std::runtime_error("vkAllocateMemory image failed");

		vkBindImageMemory(dev, img, mem, 0);
	}

	static VkImageView createView(VkDevice dev, VkImage img, VkFormat fmt)
	{
		VkImageViewCreateInfo vci{};
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = img;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = fmt;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.layerCount = 1;

		VkImageView view{};
		if (vkCreateImageView(dev, &vci, nullptr, &view) != VK_SUCCESS)
			throw std::runtime_error("vkCreateImageView failed");
		return view;
	}

	static VkSampler createSampler(VkDevice dev)
	{
		VkSamplerCreateInfo sci{};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sci.maxAnisotropy = 1.0f;

		VkSampler s{};
		if (vkCreateSampler(dev, &sci, nullptr, &s) != VK_SUCCESS)
			throw std::runtime_error("vkCreateSampler failed");
		return s;
	}

	Texture2D::~Texture2D() noexcept { destroy(); }

	Texture2D::Texture2D(Texture2D &&o) noexcept { *this = std::move(o); }
	Texture2D &Texture2D::operator=(Texture2D &&o) noexcept
	{
		if (this != &o)
		{
			destroy();
			device_ = o.device_;
			image_ = o.image_;
			memory_ = o.memory_;
			view_ = o.view_;
			sampler_ = o.sampler_;
			format_ = o.format_;
			o.device_ = VK_NULL_HANDLE;
			o.image_ = VK_NULL_HANDLE;
			o.memory_ = VK_NULL_HANDLE;
			o.view_ = VK_NULL_HANDLE;
			o.sampler_ = VK_NULL_HANDLE;
		}
		return *this;
	}

	void Texture2D::destroy() noexcept
	{
		if (device_ == VK_NULL_HANDLE)
			return;
		if (sampler_)
			vkDestroySampler(device_, sampler_, nullptr);
		if (view_)
			vkDestroyImageView(device_, view_, nullptr);
		if (image_)
			vkDestroyImage(device_, image_, nullptr);
		if (memory_)
			vkFreeMemory(device_, memory_, nullptr);
		device_ = VK_NULL_HANDLE;
		image_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		view_ = VK_NULL_HANDLE;
		sampler_ = VK_NULL_HANDLE;
	}

	void Texture2D::makeWhite(VkDevice device, VkPhysicalDevice phys, uint32_t qf, VkQueue q)
	{
		const unsigned char px[4] = {255, 255, 255, 255};

		destroy();
		device_ = device;
		format_ = pickFormat(phys);

		VkBuffer staging{};
		VkDeviceMemory stagingMem{};
		createBuffer(device, phys, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, 4, 0, &mapped);
		std::memcpy(mapped, px, 4);
		vkUnmapMemory(device, stagingMem);

		createImage(device, phys, 1, 1, format_, image_, memory_);

		VkCommandPool pool = makePool(device, qf);
		VkCommandBuffer cmd = beginCmd(device, pool);

		transition(cmd, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy bic{};
		bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bic.imageSubresource.layerCount = 1;
		bic.imageExtent = {1, 1, 1};
		vkCmdCopyBufferToImage(cmd, staging, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

		transition(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		endCmd(device, q, pool, cmd);

		vkDestroyBuffer(device, staging, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);

		view_ = createView(device, image_, format_);
		sampler_ = createSampler(device);
	}

	void Texture2D::load(VkDevice device, VkPhysicalDevice phys, uint32_t qf, VkQueue q, const std::string &path)
	{
		destroy();
		device_ = device;
		format_ = pickFormat(phys);

		int w = 0, h = 0, c = 0;
		stbi_set_flip_vertically_on_load(1);
		unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 4);
		if (!data)
			throw std::runtime_error("stbi_load failed: " + path);

		const VkDeviceSize size = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * 4;

		VkBuffer staging{};
		VkDeviceMemory stagingMem{};
		createBuffer(device, phys, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
		std::memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(device, stagingMem);

		stbi_image_free(data);

		createImage(device, phys, static_cast<uint32_t>(w), static_cast<uint32_t>(h), format_, image_, memory_);

		VkCommandPool pool = makePool(device, qf);
		VkCommandBuffer cmd = beginCmd(device, pool);

		transition(cmd, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy bic{};
		bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bic.imageSubresource.layerCount = 1;
		bic.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
		vkCmdCopyBufferToImage(cmd, staging, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

		transition(cmd, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		endCmd(device, q, pool, cmd);

		vkDestroyBuffer(device, staging, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);

		view_ = createView(device, image_, format_);
		sampler_ = createSampler(device);
	}

} // namespace scop::vk
