#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/VulkanRenderer.hpp"

#include "scop/vk/VkContext.hpp"
#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Buffer.hpp"
#include "scop/vk/Vertex.hpp"
#include "scop/vk/Framebuffers.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Sync.hpp"

#include <stdexcept>
#include <vector>

namespace
{

	const std::vector<scop::vk::Vertex> kTriangle = {
		{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
		{{0.6f, 0.6f}, {0.2f, 1.0f, 0.2f}},
		{{-0.6f, 0.6f}, {0.2f, 0.4f, 1.0f}},
	};

} // namespace

namespace scop
{

	class VulkanRendererImpl
	{
	public:
		void run()
		{
			init();
			loop();
			shutdown();
		}

	private:
		vk::VkContext ctx_{};
		vk::Swapchain swap_{};
		vk::Pipeline pipe_{};

		std::vector<VkFramebuffer> framebuffers_;

		VkCommandPool commandPool_ = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> commandBuffers_;

		vk::AllocatedBuffer vertex_{};
		vk::SyncObjects sync_{};

		void init()
		{
			ctx_.initWindow(900, 600, "scop - clean split (step6)");
			ctx_.initInstanceAndSurface();
			ctx_.pickPhysicalDevice();
			ctx_.createLogicalDevice();

			swap_.create(ctx_);
			pipe_.create(ctx_.device(), swap_.imageFormat(), swap_.extent(),
						 "shaders/tri.vert.spv", "shaders/tri.frag.spv");

			framebuffers_ = vk::createFramebuffers(ctx_.device(), pipe_.renderPass(),
												   swap_.imageViews(), swap_.extent());

			commandPool_ = vk::createCommandPool(ctx_.device(), ctx_.indices().graphicsFamily.value());

			vertex_ = vk::createVertexBuffer(ctx_.device(), ctx_.physicalDevice(), kTriangle);

			commandBuffers_ = vk::allocateCommandBuffers(ctx_.device(), commandPool_, framebuffers_.size());
			vk::recordTriangleCommandBuffers(commandBuffers_, pipe_.renderPass(), framebuffers_,
											 swap_.extent(), pipe_.pipeline(),
											 vertex_.buffer,
											 static_cast<uint32_t>(kTriangle.size()));

			sync_ = vk::createSyncObjects(ctx_.device());
		}

		void loop()
		{
			while (!glfwWindowShouldClose(ctx_.window()))
			{
				glfwPollEvents();
				drawFrame();

				if (glfwGetKey(ctx_.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
					glfwSetWindowShouldClose(ctx_.window(), GLFW_TRUE);
			}
			vkDeviceWaitIdle(ctx_.device());
		}

		void drawFrame()
		{
			vkWaitForFences(ctx_.device(), 1, &sync_.inFlight, VK_TRUE, UINT64_MAX);
			vkResetFences(ctx_.device(), 1, &sync_.inFlight);

			uint32_t imageIndex = 0;
			if (vkAcquireNextImageKHR(ctx_.device(), swap_.handle(), UINT64_MAX,
									  sync_.imageAvailable, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS)
				throw std::runtime_error("vkAcquireNextImageKHR failed");

			VkSemaphore waitSems[] = {sync_.imageAvailable};
			VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			VkSemaphore signalSems[] = {sync_.renderFinished};

			VkSubmitInfo submit{};
			submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit.waitSemaphoreCount = 1;
			submit.pWaitSemaphores = waitSems;
			submit.pWaitDstStageMask = waitStages;
			submit.commandBufferCount = 1;
			submit.pCommandBuffers = &commandBuffers_[imageIndex];
			submit.signalSemaphoreCount = 1;
			submit.pSignalSemaphores = signalSems;

			if (vkQueueSubmit(ctx_.graphicsQueue(), 1, &submit, sync_.inFlight) != VK_SUCCESS)
				throw std::runtime_error("vkQueueSubmit failed");

			VkSwapchainKHR scs[] = {swap_.handle()};

			VkPresentInfoKHR present{};
			present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present.waitSemaphoreCount = 1;
			present.pWaitSemaphores = signalSems;
			present.swapchainCount = 1;
			present.pSwapchains = scs;
			present.pImageIndices = &imageIndex;

			if (vkQueuePresentKHR(ctx_.presentQueue(), &present) != VK_SUCCESS)
				throw std::runtime_error("vkQueuePresentKHR failed");
		}

		void shutdown()
		{
			vkDeviceWaitIdle(ctx_.device());

			vk::destroySyncObjects(ctx_.device(), sync_);

			vk::freeCommandBuffers(ctx_.device(), commandPool_, commandBuffers_);
			vk::destroyCommandPool(ctx_.device(), commandPool_);

			vk::destroyFramebuffers(ctx_.device(), framebuffers_);

			vk::destroyBuffer(ctx_.device(), vertex_);

			pipe_.destroy(ctx_.device());
			swap_.destroy(ctx_.device());
			ctx_.destroy();
		}
	};

	void VulkanRenderer::run()
	{
		VulkanRendererImpl impl;
		impl.run();
	}

} // namespace scop
