#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/VulkanRenderer.hpp"

#include "scop/vk/VkContext.hpp"
#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Framebuffers.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Sync.hpp"
#include "scop/vk/Buffer.hpp"
#include "scop/vk/Vertex.hpp"

#include <iostream>
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

	void VulkanRenderer::run()
	{
		try
		{
			vk::VkContext ctx;
			ctx.init(900, 600, "scop - RAII clean");

			vk::Swapchain swap(ctx);
			vk::Pipeline pipe(ctx.device(), swap.imageFormat(), swap.extent(),
							  "shaders/tri.vert.spv", "shaders/tri.frag.spv");

			vk::Framebuffers fbs(ctx.device(), pipe.renderPass(), swap.imageViews(), swap.extent());

			vk::VertexBuffer vb(ctx.device(), ctx.physicalDevice(), kTriangle);

			vk::Commands cmds(ctx.device(),
							  ctx.indices().graphicsFamily.value(),
							  fbs.size());

			cmds.recordTriangle(pipe.renderPass(), fbs.get(), swap.extent(),
								pipe.pipeline(),
								vb.buffer(),
								vb.count());

			vk::FrameSync sync(ctx.device());

			while (!glfwWindowShouldClose(ctx.window()))
			{
				glfwPollEvents();

				vkWaitForFences(ctx.device(), 1, &sync.inFlight(), VK_TRUE, UINT64_MAX);
				vkResetFences(ctx.device(), 1, &sync.inFlight());

				uint32_t imageIndex = 0;
				if (vkAcquireNextImageKHR(ctx.device(), swap.handle(), UINT64_MAX,
										  sync.imageAvailable(), VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS)
					throw std::runtime_error("vkAcquireNextImageKHR failed");

				VkSemaphore waitSems[] = {sync.imageAvailable()};
				VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
				VkSemaphore signalSems[] = {sync.renderFinished()};

				VkSubmitInfo submit{};
				submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submit.waitSemaphoreCount = 1;
				submit.pWaitSemaphores = waitSems;
				submit.pWaitDstStageMask = waitStages;
				submit.commandBufferCount = 1;
				submit.pCommandBuffers = &cmds.buffers()[imageIndex];
				submit.signalSemaphoreCount = 1;
				submit.pSignalSemaphores = signalSems;

				if (vkQueueSubmit(ctx.graphicsQueue(), 1, &submit, sync.inFlight()) != VK_SUCCESS)
					throw std::runtime_error("vkQueueSubmit failed");

				VkSwapchainKHR scs[] = {swap.handle()};

				VkPresentInfoKHR present{};
				present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				present.waitSemaphoreCount = 1;
				present.pWaitSemaphores = signalSems;
				present.swapchainCount = 1;
				present.pSwapchains = scs;
				present.pImageIndices = &imageIndex;

				if (vkQueuePresentKHR(ctx.presentQueue(), &present) != VK_SUCCESS)
					throw std::runtime_error("vkQueuePresentKHR failed");

				if (glfwGetKey(ctx.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
					glfwSetWindowShouldClose(ctx.window(), GLFW_TRUE);
			}
			// RAII destructors handle cleanup automatically.
		}
		catch (const std::exception &e)
		{
			std::cerr << "Fatal: " << e.what() << "\n";
		}
	}

} // namespace scop
