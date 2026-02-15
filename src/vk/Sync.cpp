#include "scop/vk/Sync.hpp"
#include <stdexcept>

namespace scop::vk
{

	void FrameSync::create(VkDevice device)
	{
		reset();
		device_ = device;

		VkSemaphoreCreateInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fi{};
		fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device_, &si, nullptr, &imageAvailable_) != VK_SUCCESS ||
			vkCreateSemaphore(device_, &si, nullptr, &renderFinished_) != VK_SUCCESS ||
			vkCreateFence(device_, &fi, nullptr, &inFlight_) != VK_SUCCESS)
		{
			throw std::runtime_error("FrameSync::create failed");
		}
	}

	void FrameSync::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			if (inFlight_)
				vkDestroyFence(device_, inFlight_, nullptr);
			if (renderFinished_)
				vkDestroySemaphore(device_, renderFinished_, nullptr);
			if (imageAvailable_)
				vkDestroySemaphore(device_, imageAvailable_, nullptr);
		}

		inFlight_ = VK_NULL_HANDLE;
		renderFinished_ = VK_NULL_HANDLE;
		imageAvailable_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

}
