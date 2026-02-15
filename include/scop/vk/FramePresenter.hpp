#pragma once

#include <vulkan/vulkan.h>
#include <utility>
#include <vector>

#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Sync.hpp"

namespace scop::vk
{

	class FramePresenter
	{
	public:
		enum class Result
		{
			Ok,
			OutOfDate
		};

		FramePresenter() = default;

		FramePresenter(VkDevice device,
					   VkQueue graphicsQueue,
					   VkQueue presentQueue,
					   const Swapchain &swapchain,
					   const Commands &commands,
					   size_t framesInFlight = 2)
		{
			create(device, graphicsQueue, presentQueue, swapchain, commands, framesInFlight);
		}

		~FramePresenter() noexcept { reset(); }

		FramePresenter(const FramePresenter &) = delete;
		FramePresenter &operator=(const FramePresenter &) = delete;

		FramePresenter(FramePresenter &&other) noexcept { *this = std::move(other); }
		FramePresenter &operator=(FramePresenter &&other) noexcept
		{
			if (this != &other)
			{
				reset();

				device_ = other.device_;
				graphicsQueue_ = other.graphicsQueue_;
				presentQueue_ = other.presentQueue_;
				swapchain_ = other.swapchain_;
				commands_ = other.commands_;
				frames_ = std::move(other.frames_);
				imagesInFlight_ = std::move(other.imagesInFlight_);
				currentFrame_ = other.currentFrame_;

				other.device_ = VK_NULL_HANDLE;
				other.graphicsQueue_ = VK_NULL_HANDLE;
				other.presentQueue_ = VK_NULL_HANDLE;
				other.swapchain_ = VK_NULL_HANDLE;
				other.commands_ = nullptr;
				other.currentFrame_ = 0;
			}
			return *this;
		}

		void create(VkDevice device,
					VkQueue graphicsQueue,
					VkQueue presentQueue,
					const Swapchain &swapchain,
					const Commands &commands,
					size_t framesInFlight = 2);

		void reset() noexcept;

		Result acquire(uint32_t &outImageIndex);

		Result submitPresent(uint32_t imageIndex);

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkQueue graphicsQueue_ = VK_NULL_HANDLE;
		VkQueue presentQueue_ = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		const Commands *commands_ = nullptr;

		std::vector<FrameSync> frames_;
		std::vector<VkFence> imagesInFlight_;
		size_t currentFrame_ = 0;
	};

}
