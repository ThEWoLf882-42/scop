#pragma once

#include "scop/vk/VkContext.hpp"
#include "scop/vk/Swapchain.hpp"
#include "scop/vk/Pipeline.hpp"
#include "scop/vk/Framebuffers.hpp"
#include "scop/vk/Commands.hpp"
#include "scop/vk/Buffer.hpp"
#include "scop/vk/FramePresenter.hpp"
#include "scop/vk/Vertex.hpp"

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
		VkContext ctx_{};
		Swapchain swap_{};
		Pipeline pipe_{};
		Framebuffers fbs_{};
		VertexBuffer vb_{};
		IndexBuffer ib_{};
		Commands cmds_{};
		FramePresenter presenter_{};
	};

}
