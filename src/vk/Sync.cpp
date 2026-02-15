#include "scop/vk/Sync.hpp"
#include <stdexcept>

namespace scop::vk
{

	void FrameSync::create(VkDevice device)
	{
		reset();
		device_ = device;

		VkSemaphoreCreateInfo sci{};
		sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(device_, &sci, nullptr, &imageAvailable_) != VK_SUCCESS)
			throw std::runtime_error("FrameSync: vkCreateSemaphore(imageAvailable) failed");

		if (vkCreateSemaphore(device_, &sci, nullptr, &renderFinished_) != VK_SUCCESS)
			throw std::runtime_error("FrameSync: vkCreateSemaphore(renderFinished) failed");

		VkFenceCreateInfo fci{};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateFence(device_, &fci, nullptr, &inFlight_) != VK_SUCCESS)
			throw std::runtime_error("FrameSync: vkCreateFence failed");
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

		imageAvailable_ = VK_NULL_HANDLE;
		renderFinished_ = VK_NULL_HANDLE;
		inFlight_ = VK_NULL_HANDLE;
		device_ = VK_NULL_HANDLE;
	}

}
