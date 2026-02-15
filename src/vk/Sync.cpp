#include "scop/vk/Sync.hpp"
#include <stdexcept>

namespace scop::vk
{

	SyncObjects createSyncObjects(VkDevice device)
	{
		SyncObjects s{};

		VkSemaphoreCreateInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fi{};
		fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device, &si, nullptr, &s.imageAvailable) != VK_SUCCESS ||
			vkCreateSemaphore(device, &si, nullptr, &s.renderFinished) != VK_SUCCESS ||
			vkCreateFence(device, &fi, nullptr, &s.inFlight) != VK_SUCCESS)
		{
			throw std::runtime_error("createSyncObjects failed");
		}

		return s;
	}

	void destroySyncObjects(VkDevice device, SyncObjects &s)
	{
		if (s.inFlight)
			vkDestroyFence(device, s.inFlight, nullptr);
		if (s.renderFinished)
			vkDestroySemaphore(device, s.renderFinished, nullptr);
		if (s.imageAvailable)
			vkDestroySemaphore(device, s.imageAvailable, nullptr);

		s.inFlight = VK_NULL_HANDLE;
		s.renderFinished = VK_NULL_HANDLE;
		s.imageAvailable = VK_NULL_HANDLE;
	}

} // namespace scop::vk
