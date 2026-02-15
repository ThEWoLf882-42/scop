#include "scop/vk/Renderer.hpp"
#include "scop/vk/Uploader.hpp"
#include "scop/io/ObjLoader.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/math/Mat4.hpp"

#include <iostream>
#include <vector>

namespace
{

	struct alignas(16) UBOData
	{
		scop::math::Mat4 mvp;
	};

	scop::io::MeshData makeCube()
	{
		using scop::vk::Vertex;

		const std::vector<Vertex> v = {
			{{-0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}},
			{{0.5f, -0.5f, -0.5f}, {0.f, 1.f, 0.f}},
			{{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}},
			{{-0.5f, 0.5f, -0.5f}, {1.f, 1.f, 0.f}},
			{{-0.5f, -0.5f, 0.5f}, {1.f, 0.f, 1.f}},
			{{0.5f, -0.5f, 0.5f}, {0.f, 1.f, 1.f}},
			{{0.5f, 0.5f, 0.5f}, {1.f, 1.f, 1.f}},
			{{-0.5f, 0.5f, 0.5f}, {0.2f, 0.2f, 0.2f}},
		};

		const std::vector<uint32_t> idx = {
			0, 1, 2, 2, 3, 0,
			1, 5, 6, 6, 2, 1,
			5, 4, 7, 7, 6, 5,
			4, 0, 3, 3, 7, 4,
			3, 2, 6, 6, 7, 3,
			4, 5, 1, 1, 0, 4};

		scop::io::MeshData m;
		m.vertices = v;
		m.indices = idx;
		return m;
	}

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

		glfwSetWindowUserPointer(ctx_.window(), this);
		glfwSetFramebufferSizeCallback(
			ctx_.window(),
			[](GLFWwindow *win, int /*w*/, int /*h*/)
			{
				auto *self = static_cast<Renderer *>(glfwGetWindowUserPointer(win));
				if (self)
					self->framebufferResized_ = true;
			});

		const std::string objPath = "assets/models/42.obj";
		scop::io::MeshData mesh;

		try
		{
			mesh = scop::io::loadObj(objPath, true);
			std::cerr << "Loaded OBJ: " << objPath
					  << " verts=" << mesh.vertices.size()
					  << " idx=" << mesh.indices.size() << "\n";
		}
		catch (const std::exception &e)
		{
			std::cerr << "OBJ load failed: " << e.what() << "\nUsing fallback cube.\n";
			mesh = makeCube();
		}

		Uploader uploader(ctx_.device(), ctx_.indices().graphicsFamily.value(), ctx_.graphicsQueue());
		vb_ = VertexBuffer(ctx_.device(), ctx_.physicalDevice(), uploader, mesh.vertices);
		ib_ = IndexBuffer(ctx_.device(), ctx_.physicalDevice(), uploader, mesh.indices);

		recreateSwapchain();

		lastTime_ = glfwGetTime();
	}

	void Renderer::recreateSwapchain()
	{
		int w = 0, h = 0;
		glfwGetFramebufferSize(ctx_.window(), &w, &h);
		while (w == 0 || h == 0)
		{
			glfwWaitEvents();
			glfwGetFramebufferSize(ctx_.window(), &w, &h);
		}

		vkDeviceWaitIdle(ctx_.device());

		swap_ = Swapchain(ctx_);
		depth_ = DepthResources(ctx_.device(), ctx_.physicalDevice(), swap_.extent());

		const size_t imageCount = swap_.imageViews().size();

		ubos_.clear();
		ubos_.reserve(imageCount);

		std::vector<VkBuffer> uboBuffers;
		uboBuffers.reserve(imageCount);

		for (size_t i = 0; i < imageCount; ++i)
		{
			ubos_.emplace_back(ctx_.device(), ctx_.physicalDevice(), sizeof(UBOData));
			uboBuffers.push_back(ubos_.back().buffer());
		}

		desc_ = Descriptors(ctx_.device(), uboBuffers, sizeof(UBOData));

		pipe_ = Pipeline(ctx_.device(), swap_.imageFormat(), depth_.format(), swap_.extent(),
						 "shaders/tri.vert.spv", "shaders/tri.frag.spv",
						 desc_.layout());

		fbs_ = Framebuffers(ctx_.device(), pipe_.renderPass(), swap_.imageViews(), depth_.view(), swap_.extent());

		cmds_ = Commands(ctx_.device(), ctx_.indices().graphicsFamily.value(), fbs_.size());
		cmds_.recordIndexed(pipe_.renderPass(), fbs_.get(), swap_.extent(),
							pipe_.pipeline(),
							pipe_.layout(),
							desc_.sets(),
							vb_.buffer(),
							ib_.buffer(),
							ib_.count(),
							VK_INDEX_TYPE_UINT32);

		presenter_ = FramePresenter(ctx_.device(),
									ctx_.graphicsQueue(),
									ctx_.presentQueue(),
									swap_,
									cmds_,
									2);

		framebufferResized_ = false;
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
		if (framebufferResized_)
		{
			recreateSwapchain();
			return;
		}

		const double now = glfwGetTime();
		const float dt = static_cast<float>(now - lastTime_);
		lastTime_ = now;

		const float speed = 2.0f;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_W) == GLFW_PRESS)
			camZ_ -= speed * dt;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_S) == GLFW_PRESS)
			camZ_ += speed * dt;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_A) == GLFW_PRESS)
			camX_ -= speed * dt;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_D) == GLFW_PRESS)
			camX_ += speed * dt;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_Q) == GLFW_PRESS)
			camY_ -= speed * dt;
		if (glfwGetKey(ctx_.window(), GLFW_KEY_E) == GLFW_PRESS)
			camY_ += speed * dt;

		uint32_t imageIndex = 0;
		if (presenter_.acquire(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}

		const VkExtent2D ext = swap_.extent();
		const float aspect = (ext.height == 0) ? 1.0f
											   : (static_cast<float>(ext.width) / static_cast<float>(ext.height));
		const float t = static_cast<float>(now);

		const scop::math::Mat4 model =
			scop::math::Mat4::mul(scop::math::Mat4::rotationY(t),
								  scop::math::Mat4::rotationX(t * 0.7f));

		const scop::math::Mat4 view =
			scop::math::Mat4::translation(-camX_, -camY_, -camZ_);

		const scop::math::Mat4 proj =
			scop::math::Mat4::perspective(45.0f * 3.14159265f / 180.0f, aspect, 0.1f, 50.0f, true);

		UBOData u{};
		u.mvp = scop::math::Mat4::mul(proj, scop::math::Mat4::mul(view, model));
		ubos_.at(imageIndex).update(&u, sizeof(u));

		if (presenter_.submitPresent(imageIndex) == FramePresenter::Result::OutOfDate)
		{
			recreateSwapchain();
			return;
		}
	}

}
