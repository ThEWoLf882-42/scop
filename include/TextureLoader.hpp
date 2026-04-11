#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace scop
{

	struct TextureImage
	{
		uint32_t width;
		uint32_t height;
		std::vector<std::uint8_t> pixels;

		bool empty() const;
	};

	class TextureLoader
	{
	public:
		static TextureImage loadPPM(const std::string &path);
		static TextureImage makeFallbackCheckerboard();
	};

} // namespace scop
