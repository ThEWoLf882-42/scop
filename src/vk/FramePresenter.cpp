#include "scop/vk/FramePresenter.hpp"

#include <stdexcept>

namespace scop::vk
{

	void FramePresenter::create(VkDevice device,
								VkQueue graphicsQueue,
								VkQueue presentQueue,
								const Swapchain &swapchain,
								const Commands &commands,
								size_t framesInFlight)
	{
		reset();

		if (framesInFlight == 0)
			throw std::runtime_error("FramePresenter: framesInFlight must be > 0");

		device_ = device;
		graphicsQueue_ = graphicsQueue;
		presentQueue_ = presentQueue;
		swapchain_ = swapchain.handle();
		commands_ = &commands;

		frames_.resize(framesInFlight);
		for (auto &f : frames_)
			f.create(device_);

		imagesInFlight_.assign(commands_->buffers().size(), VK_NULL_HANDLE);
		currentFrame_ = 0;
	}

	void FramePresenter::reset() noexcept
	{
		if (device_ != VK_NULL_HANDLE)
		{
			(void)vkDeviceWaitIdle(device_);
		}

		for (auto &f : frames_)
			f.reset();
		frames_.clear();
		imagesInFlight_.clear();

		device_ = VK_NULL_HANDLE;
		graphicsQueue_ = VK_NULL_HANDLE;
		presentQueue_ = VK_NULL_HANDLE;
		swapchain_ = VK_NULL_HANDLE;
		commands_ = nullptr;
		currentFrame_ = 0;
	}

	FramePresenter::Result FramePresenter::acquire(uint32_t &outImageIndex)
	{
		if (!commands_)
			throw std::runtime_error("FramePresenter::acquire: not created");

		FrameSync &sync = frames_[currentFrame_];

		vkWaitForFences(device_, 1, sync.inFlightPtr(), VK_TRUE, UINT64_MAX);

		VkResult res = vkAcquireNextImageKHR(
			device_, swapchain_, UINT64_MAX,
			sync.imageAvailable(), VK_NULL_HANDLE,
			&outImageIndex);

		if (res == VK_ERROR_OUT_OF_DATE_KHR)
			return Result::OutOfDate;

		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error("vkAcquireNextImageKHR failed");

		if (outImageIndex >= imagesInFlight_.size())
			throw std::runtime_error("FramePresenter: acquired imageIndex out of range");

		if (imagesInFlight_[outImageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device_, 1, &imagesInFlight_[outImageIndex], VK_TRUE, UINT64_MAX);
		}

		imagesInFlight_[outImageIndex] = sync.inFlight();

		vkResetFences(device_, 1, sync.inFlightPtr());

		return Result::Ok;
	}

	FramePresenter::Result FramePresenter::submitPresent(uint32_t imageIndex)
	{
		if (!commands_)
			throw std::runtime_error("FramePresenter::submitPresent: not created");

		FrameSync &sync = frames_[currentFrame_];

		const auto &bufs = commands_->buffers();
		if (imageIndex >= bufs.size())
			throw std::runtime_error("FramePresenter: imageIndex out of range (cmd buffers)");

		VkSemaphore waitSems[] = {sync.imageAvailable()};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkSemaphore signalSems[] = {sync.renderFinished()};

		VkSubmitInfo submit{};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = waitSems;
		submit.pWaitDstStageMask = waitStages;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &bufs[imageIndex];
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = signalSems;

		if (vkQueueSubmit(graphicsQueue_, 1, &submit, sync.inFlight()) != VK_SUCCESS)
			throw std::runtime_error("vkQueueSubmit failed");

		VkPresentInfoKHR present{};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.waitSemaphoreCount = 1;
		present.pWaitSemaphores = signalSems;
		present.swapchainCount = 1;
		present.pSwapchains = &swapchain_;
		present.pImageIndices = &imageIndex;

		VkResult pres = vkQueuePresentKHR(presentQueue_, &present);

		currentFrame_ = (currentFrame_ + 1) % frames_.size();

		if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
			return Result::OutOfDate;

		if (pres != VK_SUCCESS)
			throw std::runtime_error("vkQueuePresentKHR failed");

		return Result::Ok;
	}

}
