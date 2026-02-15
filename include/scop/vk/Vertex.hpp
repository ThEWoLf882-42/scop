#pragma once

#include <array>
#include <cstddef>
#include <vulkan/vulkan.h>

namespace scop::vk
{

	struct Vertex
	{
		float pos[3];
		float normal[3];
		float uv[2];

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
			a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[0].offset = static_cast<uint32_t>(offsetof(Vertex, pos));

			a[1].binding = 0;
			a[1].location = 1;
			a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[1].offset = static_cast<uint32_t>(offsetof(Vertex, normal));

			a[2].binding = 0;
			a[2].location = 2;
			a[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[2].offset = static_cast<uint32_t>(offsetof(Vertex, uv));

			return a;
		}
	};

}
