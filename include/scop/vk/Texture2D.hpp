#pragma once
#include <string>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	class Texture2D
	{
	public:
		Texture2D() = default;
		~Texture2D() noexcept;

		Texture2D(const Texture2D &) = delete;
		Texture2D &operator=(const Texture2D &) = delete;

		Texture2D(Texture2D &&) noexcept;
		Texture2D &operator=(Texture2D &&) noexcept;

		void makeWhite(VkDevice device, VkPhysicalDevice phys,
					   uint32_t queueFamily, VkQueue queue);

		void load(VkDevice device, VkPhysicalDevice phys,
				  uint32_t queueFamily, VkQueue queue,
				  const std::string &path);

		VkImageView view() const { return view_; }
		VkSampler sampler() const { return sampler_; }

	private:
		void destroy() noexcept;

		VkDevice device_ = VK_NULL_HANDLE;
		VkImage image_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkImageView view_ = VK_NULL_HANDLE;
		VkSampler sampler_ = VK_NULL_HANDLE;
		VkFormat format_ = VK_FORMAT_R8G8B8A8_UNORM;
	};

} // namespace scop::vk
