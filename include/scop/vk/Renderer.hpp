#pragma once

#include <vector>

#include "scop/vk/VkContext.hpp"
#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Depth.hpp"
#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Framebuffers.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Buffer.hpp"
#include "scop/vk/FramePresenter.hpp"
#include "scop/vk/UniformBuffer.hpp"
#include "scop/vk/Descriptors.hpp"

struct GLFWwindow;

namespace scop::vk
{

	class Renderer
	{
	public:
		Renderer() = default;
		Renderer(int width, int height, const char *title);

		Renderer(const Renderer &) = delete;
		Renderer &operator=(const Renderer &) = delete;
		Renderer(Renderer &&) = delete;
		Renderer &operator=(Renderer &&) = delete;

		void init(int width, int height, const char *title);

		void pollEvents();
		bool shouldClose() const;
		void requestClose();

		GLFWwindow *window() const { return ctx_.window(); }

		void draw();

	private:
		void recreateSwapchain();

		VkContext ctx_{};
		Swapchain swap_{};
		DepthResources depth_{};

		std::vector<UniformBuffer> ubos_;
		Descriptors desc_{};

		Pipeline pipe_{};
		Framebuffers fbs_{};

		VertexBuffer vb_{};
		IndexBuffer ib_{};

		Commands cmds_{};
		FramePresenter presenter_{};

		float camX_ = 0.0f;
		float camY_ = 0.0f;
		float camZ_ = 2.5f;

		float yawDeg_ = -90.0f;
		float pitchDeg_ = 0.0f;

		float fovDeg_ = 55.0f;

		bool firstMouse_ = true;
		double lastMouseX_ = 0.0;
		double lastMouseY_ = 0.0;

		bool cursorLocked_ = true;
		bool escWasDown_ = false;
		bool rWasDown_ = false;
		bool spaceWasDown_ = false;

		bool f1WasDown_ = false;
		bool wireframe_ = false;
		bool warnedNoWire_ = false;

		bool paused_ = false;
		float modelTime_ = 0.0f;

		double lastTime_ = 0.0;
		double fpsAccum_ = 0.0;
		int fpsFrames_ = 0;

		bool framebufferResized_ = false;
	};

}
