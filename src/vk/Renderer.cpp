#include "scop/vk/Renderer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

namespace
{

	const std::vector<scop::vk::Vertex> kTriangle = {
		{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
		{{0.6f, 0.6f}, {0.2f, 1.0f, 0.2f}},
		{{-0.6f, 0.6f}, {0.2f, 0.4f, 1.0f}},
	};

}

namespace scop::vk
{

	Renderer::Renderer(int width, int height, const char *title)
	{
		init(width, height, title);
	}

	void Renderer::init(int width, int height, const char *title)
	{
		ctx_.init(width, height, title);

		swap_ = Swapchain(ctx_);
		pipe_ = Pipeline(ctx_.device(), swap_.imageFormat(), swap_.extent(),
						 "shaders/tri.vert.spv", "shaders/tri.frag.spv");

		fbs_ = Framebuffers(ctx_.device(), pipe_.renderPass(), swap_.imageViews(), swap_.extent());

		vb_ = VertexBuffer(ctx_.device(), ctx_.physicalDevice(), kTriangle);

		cmds_ = Commands(ctx_.device(),
						 ctx_.indices().graphicsFamily.value(),
						 fbs_.size());

		cmds_.recordTriangle(pipe_.renderPass(), fbs_.get(), swap_.extent(),
							 pipe_.pipeline(),
							 vb_.buffer(),
							 vb_.count());

		presenter_ = FramePresenter(ctx_.device(),
									ctx_.graphicsQueue(),
									ctx_.presentQueue(),
									swap_,
									cmds_);
	}

	void Renderer::pollEvents()
	{
		glfwPollEvents();
	}

	bool Renderer::shouldClose() const
	{
		return glfwWindowShouldClose(ctx_.window()) != 0;
	}

	void Renderer::requestClose()
	{
		glfwSetWindowShouldClose(ctx_.window(), GLFW_TRUE);
	}

	void Renderer::draw()
	{
		presenter_.draw();
	}

}
