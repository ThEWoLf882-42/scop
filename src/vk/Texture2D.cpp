#include "scop/vk/Texture2D.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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
		VkFormat fmt = VK_FORMAT_R8G8B8A8_SRGB;
		VkFormatProperties fp{};
		vkGetPhysicalDeviceFormatProperties(phys, fmt, &fp);
		if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
			return fmt;
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

	// ---------- helpers ----------
	static bool hasExtCI(const std::string &path, const char *ext)
	{
		const size_t lp = path.size();
		const size_t le = std::strlen(ext);
		if (lp < le)
			return false;
		for (size_t i = 0; i < le; ++i)
		{
			const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(path[lp - le + i])));
			const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
			if (a != b)
				return false;
		}
		return true;
	}

	// Reads tokens while skipping comments (# ... endline)
	static std::string nextToken(std::istream &in)
	{
		std::string tok;
		char c = 0;
		while (in.get(c))
		{
			if (std::isspace(static_cast<unsigned char>(c)))
				continue;
			if (c == '#')
			{
				std::string dummy;
				std::getline(in, dummy);
				continue;
			}
			tok.push_back(c);
			break;
		}
		while (in.get(c))
		{
			if (std::isspace(static_cast<unsigned char>(c)))
				break;
			tok.push_back(c);
		}
		return tok;
	}

	// ---- PPM P6 -> RGBA ----
	static void loadPPM_P6_RGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgbaOut, bool flipY)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			throw std::runtime_error("PPM open failed: " + path);

		const std::string magic = nextToken(f);
		if (magic != "P6")
			throw std::runtime_error("PPM is not P6: " + path);

		const std::string sw = nextToken(f);
		const std::string sh = nextToken(f);
		const std::string sm = nextToken(f);
		if (sw.empty() || sh.empty() || sm.empty())
			throw std::runtime_error("PPM header parse failed: " + path);

		w = std::stoi(sw);
		h = std::stoi(sh);
		const int maxv = std::stoi(sm);
		if (w <= 0 || h <= 0)
			throw std::runtime_error("PPM invalid size: " + path);
		if (maxv <= 0 || maxv > 255)
			throw std::runtime_error("PPM maxval unsupported: " + path);

		// consume one whitespace if needed
		char c = 0;
		f.read(&c, 1);
		if (!f)
			throw std::runtime_error("PPM missing pixel data: " + path);
		if (!std::isspace(static_cast<unsigned char>(c)))
			f.seekg(-1, std::ios::cur);

		const size_t rgbSize = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
		std::vector<uint8_t> rgb(rgbSize);
		f.read(reinterpret_cast<char *>(rgb.data()), (std::streamsize)rgbSize);
		if (f.gcount() != (std::streamsize)rgbSize)
			throw std::runtime_error("PPM pixel data truncated: " + path);

		rgbaOut.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 255);

		for (int y = 0; y < h; ++y)
		{
			const int outY = flipY ? (h - 1 - y) : y;
			const size_t srcRow = static_cast<size_t>(y) * static_cast<size_t>(w) * 3;
			const size_t dstRow = static_cast<size_t>(outY) * static_cast<size_t>(w) * 4;
			for (int x = 0; x < w; ++x)
			{
				const size_t si = srcRow + static_cast<size_t>(x) * 3;
				const size_t di = dstRow + static_cast<size_t>(x) * 4;
				rgbaOut[di + 0] = rgb[si + 0];
				rgbaOut[di + 1] = rgb[si + 1];
				rgbaOut[di + 2] = rgb[si + 2];
				rgbaOut[di + 3] = 255;
			}
		}
	}

	// ---- BMP (BI_RGB) 24/32 -> RGBA ----
	static uint16_t rd16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)(p[1] << 8); }
	static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)(p[1] << 8) | (uint32_t)(p[2] << 16) | (uint32_t)(p[3] << 24); }
	static int32_t rds32(const uint8_t *p) { return (int32_t)rd32(p); }

	static void loadBMP_RGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgbaOut, bool flipY)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			throw std::runtime_error("BMP open failed: " + path);

		uint8_t fh[14]{};
		f.read(reinterpret_cast<char *>(fh), 14);
		if (f.gcount() != 14)
			throw std::runtime_error("BMP header truncated: " + path);

		if (fh[0] != 'B' || fh[1] != 'M')
			throw std::runtime_error("Not a BMP: " + path);

		const uint32_t offBits = rd32(&fh[10]);

		uint8_t dibSizeBuf[4]{};
		f.read(reinterpret_cast<char *>(dibSizeBuf), 4);
		if (f.gcount() != 4)
			throw std::runtime_error("BMP DIB header truncated: " + path);
		const uint32_t dibSize = rd32(dibSizeBuf);
		if (dibSize < 40)
			throw std::runtime_error("BMP DIB unsupported: " + path);

		std::vector<uint8_t> dib(dibSize);
		std::memcpy(dib.data(), dibSizeBuf, 4);
		f.read(reinterpret_cast<char *>(dib.data() + 4), (std::streamsize)(dibSize - 4));
		if (f.gcount() != (std::streamsize)(dibSize - 4))
			throw std::runtime_error("BMP DIB truncated: " + path);

		const int32_t width = rds32(&dib[4]);
		const int32_t height = rds32(&dib[8]);
		const uint16_t planes = rd16(&dib[12]);
		const uint16_t bpp = rd16(&dib[14]);
		const uint32_t comp = rd32(&dib[16]);

		if (planes != 1)
			throw std::runtime_error("BMP planes != 1: " + path);
		if (comp != 0)
			throw std::runtime_error("BMP compression unsupported (need BI_RGB): " + path);
		if (bpp != 24 && bpp != 32)
			throw std::runtime_error("BMP bpp unsupported (need 24 or 32): " + path);
		if (width <= 0 || height == 0)
			throw std::runtime_error("BMP invalid size: " + path);

		const bool bottomUp = (height > 0);
		w = width;
		h = bottomUp ? height : -height;

		const size_t bytesPerPixel = (bpp == 24) ? 3u : 4u;
		const size_t rowStrideFile = (bpp == 24)
										 ? ((static_cast<size_t>(w) * 3u + 3u) & ~3u)
										 : (static_cast<size_t>(w) * 4u);

		rgbaOut.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 255);

		f.seekg((std::streamoff)offBits, std::ios::beg);
		if (!f)
			throw std::runtime_error("BMP seek failed: " + path);

		std::vector<uint8_t> row(rowStrideFile);

		for (int yFile = 0; yFile < h; ++yFile)
		{
			f.read(reinterpret_cast<char *>(row.data()), (std::streamsize)rowStrideFile);
			if (f.gcount() != (std::streamsize)rowStrideFile)
				throw std::runtime_error("BMP pixel data truncated: " + path);

			// Convert file row index to a "top-down" row index
			const int topDownY = bottomUp ? (h - 1 - yFile) : yFile;
			// Apply the same convention as our PPM loader: output y=0 is bottom row
			const int outY = flipY ? (h - 1 - topDownY) : topDownY;

			const size_t dstRow = static_cast<size_t>(outY) * static_cast<size_t>(w) * 4;
			for (int x = 0; x < w; ++x)
			{
				const size_t si = static_cast<size_t>(x) * bytesPerPixel;
				const size_t di = dstRow + static_cast<size_t>(x) * 4;

				const uint8_t B = row[si + 0];
				const uint8_t G = row[si + 1];
				const uint8_t R = row[si + 2];
				const uint8_t A = (bpp == 32) ? row[si + 3] : 255;

				rgbaOut[di + 0] = R;
				rgbaOut[di + 1] = G;
				rgbaOut[di + 2] = B;
				rgbaOut[di + 3] = A;
			}
		}
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
		const uint8_t px[4] = {255, 255, 255, 255};

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

		int w = 0, h = 0;
		std::vector<uint8_t> rgba;

		// Subject-safe formats:
		// - PPM P6 (binary)
		// - BMP 24/32-bit (BI_RGB)
		if (hasExtCI(path, ".ppm"))
		{
			loadPPM_P6_RGBA(path, w, h, rgba, true);
		}
		else if (hasExtCI(path, ".bmp"))
		{
			loadBMP_RGBA(path, w, h, rgba, true);
		}
		else
		{
			throw std::runtime_error("Texture format not supported (use .bmp or .ppm): " + path);
		}

		const VkDeviceSize size = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * 4;

		VkBuffer staging{};
		VkDeviceMemory stagingMem{};
		createBuffer(device, phys, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					 staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &mapped);
		std::memcpy(mapped, rgba.data(), static_cast<size_t>(size));
		vkUnmapMemory(device, stagingMem);

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
