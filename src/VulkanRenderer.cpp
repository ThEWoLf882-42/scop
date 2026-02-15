#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "scop/VulkanRenderer.hpp"
#include "scop/vk/Renderer.hpp"

#include <iostream>
#include <stdexcept>

namespace scop
{

	void VulkanRenderer::run()
	{
		try
		{
			vk::Renderer r(900, 600, "scop - Renderer RAII");

			while (!r.shouldClose())
			{
				r.pollEvents();
				r.draw();

				if (glfwGetKey(r.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
					r.requestClose();
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "Fatal: " << e.what() << "\n";
		}
	}

}
