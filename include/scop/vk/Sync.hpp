#pragma once

#include <vulkan/vulkan.h>
#include <utility>

namespace scop::vk
{

	class FrameSync
	{
	public:
		FrameSync() = default;
		explicit FrameSync(VkDevice device) { create(device); }

		~FrameSync() noexcept { reset(); }

		FrameSync(const FrameSync &) = delete;
		FrameSync &operator=(const FrameSync &) = delete;

		FrameSync(FrameSync &&other) noexcept { *this = std::move(other); }
		FrameSync &operator=(FrameSync &&other) noexcept
		{
			if (this != &other)
			{
				reset();
				device_ = other.device_;
				imageAvailable_ = other.imageAvailable_;
				renderFinished_ = other.renderFinished_;
				inFlight_ = other.inFlight_;

				other.device_ = VK_NULL_HANDLE;
				other.imageAvailable_ = VK_NULL_HANDLE;
				other.renderFinished_ = VK_NULL_HANDLE;
				other.inFlight_ = VK_NULL_HANDLE;
			}
			return *this;
		}

		void create(VkDevice device);
		void reset() noexcept;

		VkSemaphore imageAvailable() const { return imageAvailable_; }
		VkSemaphore renderFinished() const { return renderFinished_; }

		VkFence inFlight() const { return inFlight_; }
		VkFence *inFlightPtr() { return &inFlight_; }
		const VkFence *inFlightPtr() const { return &inFlight_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
		VkSemaphore renderFinished_ = VK_NULL_HANDLE;
		VkFence inFlight_ = VK_NULL_HANDLE;
	};

}
