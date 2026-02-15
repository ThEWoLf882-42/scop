#pragma once
#include <array>
#include <cstddef>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	struct Vertex
	{
		float pos[2];
		float color[3];

		static VkVertexInputBindingDescription bindingDescription()
		{
			VkVertexInputBindingDescription b{};
			b.binding = 0;
			b.stride = sizeof(Vertex);
			b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return b;
		}

		static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> a{};

			a[0].binding = 0;
			a[0].location = 0;
			a[0].format = VK_FORMAT_R32G32_SFLOAT;
			a[0].offset = offsetof(Vertex, pos);

			a[1].binding = 0;
			a[1].location = 1;
			a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[1].offset = offsetof(Vertex, color);

			return a;
		}
	};

}
