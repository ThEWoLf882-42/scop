#include "scop/VulkanRenderer.hpp"
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/vk/Renderer.hpp"

namespace scop
{
	void VulkanRenderer::run(const std::string &initialObjPath)
	{
		try
		{
			vk::Renderer r(1920, 1080, "scop - Renderer RAII", initialObjPath);

			while (!r.shouldClose())
			{
				r.pollEvents();
				r.draw();

				// Quit: Ctrl+Q
				const bool q = glfwGetKey(r.window(), GLFW_KEY_Q) == GLFW_PRESS;
				const bool ctrl = (glfwGetKey(r.window(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
								  (glfwGetKey(r.window(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
				if (q && ctrl)
					r.requestClose();
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "Fatal: " << e.what() << "\n";
		}
	}
}
