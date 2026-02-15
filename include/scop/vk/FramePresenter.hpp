#pragma once

#include <vulkan/vulkan.h>
#include <utility>

#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Sync.hpp"

namespace scop::vk
{

	class FramePresenter
	{
	public:
		FramePresenter() = default;

		FramePresenter(VkDevice device,
					   VkQueue graphicsQueue,
					   VkQueue presentQueue,
					   const Swapchain &swapchain,
					   const Commands &commands)
		{
			create(device, graphicsQueue, presentQueue, swapchain, commands);
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
				sync_ = std::move(other.sync_);

				other.device_ = VK_NULL_HANDLE;
				other.graphicsQueue_ = VK_NULL_HANDLE;
				other.presentQueue_ = VK_NULL_HANDLE;
				other.swapchain_ = VK_NULL_HANDLE;
				other.commands_ = nullptr;
			}
			return *this;
		}

		void create(VkDevice device,
					VkQueue graphicsQueue,
					VkQueue presentQueue,
					const Swapchain &swapchain,
					const Commands &commands);

		void reset() noexcept;

		void draw();

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkQueue graphicsQueue_ = VK_NULL_HANDLE;
		VkQueue presentQueue_ = VK_NULL_HANDLE;

		VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
		const Commands *commands_ = nullptr;

		FrameSync sync_{};
	};

}
