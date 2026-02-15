#include "scop/vk/FramePresenter.hpp"

#include <stdexcept>

namespace scop::vk
{

	void FramePresenter::create(VkDevice device,
								VkQueue graphicsQueue,
								VkQueue presentQueue,
								const Swapchain &swapchain,
								const Commands &commands)
	{
		reset();

		device_ = device;
		graphicsQueue_ = graphicsQueue;
		presentQueue_ = presentQueue;
		swapchain_ = swapchain.handle();
		commands_ = &commands;

		sync_.create(device_);
	}

	void FramePresenter::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			(void)vkDeviceWaitIdle(device_);
		}

		sync_.reset();

		device_ = VK_NULL_HANDLE;
		graphicsQueue_ = VK_NULL_HANDLE;
		presentQueue_ = VK_NULL_HANDLE;
		swapchain_ = VK_NULL_HANDLE;
		commands_ = nullptr;
	}

	void FramePresenter::draw()
	{
		if (!commands_)
			throw std::runtime_error("FramePresenter::draw: presenter not created");

		vkWaitForFences(device_, 1, sync_.inFlightPtr(), VK_TRUE, UINT64_MAX);
		vkResetFences(device_, 1, sync_.inFlightPtr());

		uint32_t imageIndex = 0;
		VkResult res = vkAcquireNextImageKHR(
			device_, swapchain_, UINT64_MAX,
			sync_.imageAvailable(), VK_NULL_HANDLE,
			&imageIndex);

		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("vkAcquireNextImageKHR failed");
		}

		VkSemaphore waitSems[] = {sync_.imageAvailable()};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkSemaphore signalSems[] = {sync_.renderFinished()};

		const auto &bufs = commands_->buffers();
		if (imageIndex >= bufs.size())
			throw std::runtime_error("FramePresenter: imageIndex out of range (command buffers)");

		VkSubmitInfo submit{};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = waitSems;
		submit.pWaitDstStageMask = waitStages;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &bufs[imageIndex];
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = signalSems;

		if (vkQueueSubmit(graphicsQueue_, 1, &submit, sync_.inFlight()) != VK_SUCCESS)
			throw std::runtime_error("vkQueueSubmit failed");

		VkPresentInfoKHR present{};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.waitSemaphoreCount = 1;
		present.pWaitSemaphores = signalSems;
		present.swapchainCount = 1;
		present.pSwapchains = &swapchain_;
		present.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(presentQueue_, &present);
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("vkQueuePresentKHR failed");
		}
	}

}
