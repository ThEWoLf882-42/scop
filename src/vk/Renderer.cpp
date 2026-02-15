#include "scop/vk/Renderer.hpp"
#include "scop/vk/Uploader.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/math/Mat4.hpp"

#include <cstdint>
#include <vector>

namespace
{

	const std::vector<scop::vk::Vertex> kTriangle = {
		{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
		{{0.6f, 0.6f}, {0.2f, 1.0f, 0.2f}},
		{{-0.6f, 0.6f}, {0.2f, 0.4f, 1.0f}},
	};

	const std::vector<uint16_t> kIndices = {0, 1, 2};

	struct alignas(16) UBOData
	{
		scop::math::Mat4 mvp;
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

		ubo_ = UniformBuffer(ctx_.device(), ctx_.physicalDevice(), sizeof(UBOData));
		desc_ = Descriptors(ctx_.device(), ubo_.buffer(), sizeof(UBOData));

		pipe_ = Pipeline(ctx_.device(), swap_.imageFormat(), swap_.extent(),
						 "shaders/tri.vert.spv", "shaders/tri.frag.spv",
						 desc_.layout());

		fbs_ = Framebuffers(ctx_.device(), pipe_.renderPass(), swap_.imageViews(), swap_.extent());

		Uploader uploader(ctx_.device(), ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
		vb_ = VertexBuffer(ctx_.device(), ctx_.physicalDevice(), uploader, kTriangle);
		ib_ = IndexBuffer(ctx_.device(), ctx_.physicalDevice(), uploader, kIndices);

		cmds_ = Commands(ctx_.device(),
						 ctx_.indices().graphicsFamily.value(),
						 fbs_.size());

		cmds_.recordIndexed(pipe_.renderPass(), fbs_.get(), swap_.extent(),
							pipe_.pipeline(),
							pipe_.layout(),
							desc_.set(),
							vb_.buffer(),
							ib_.buffer(),
							ib_.count());

		presenter_ = FramePresenter(ctx_.device(),
									ctx_.graphicsQueue(),
									ctx_.presentQueue(),
									swap_,
									cmds_);
	}

	void Renderer::pollEvents() { glfwPollEvents(); }

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
		const float t = static_cast<float>(glfwGetTime());
		UBOData u{};
		u.mvp = scop::math::Mat4::rotationZ(t);
		ubo_.update(&u, sizeof(u));

		presenter_.draw();
	}

}
