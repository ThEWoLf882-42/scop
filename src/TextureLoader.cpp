#include "TextureLoader.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

namespace scop
{

	bool TextureImage::empty() const
	{
		return pixels.empty() || width == 0U || height == 0U;
	}

	namespace
	{

		std::string readToken(std::istream &stream)
		{
			std::string token;
			char ch = '\0';
			while (stream.get(ch))
			{
				if (std::isspace(static_cast<unsigned char>(ch)))
				{
					continue;
				}
				if (ch == '#')
				{
					std::string comment;
					std::getline(stream, comment);
					continue;
				}
				token.push_back(ch);
				break;
			}

			while (stream.get(ch))
			{
				if (std::isspace(static_cast<unsigned char>(ch)))
				{
					break;
				}
				token.push_back(ch);
			}
			return token;
		}

	} // namespace

	TextureImage TextureLoader::loadPPM(const std::string &path)
	{
		std::ifstream file(path.c_str(), std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("Failed to open PPM texture: " + path);
		}

		const std::string magic = readToken(file);
		if (magic != "P6" && magic != "P3")
		{
			throw std::runtime_error("Only P6 and P3 PPM files are supported: " + path);
		}

		const uint32_t width = static_cast<uint32_t>(std::stoul(readToken(file)));
		const uint32_t height = static_cast<uint32_t>(std::stoul(readToken(file)));
		const int maxValue = std::stoi(readToken(file));
		if (maxValue <= 0 || maxValue > 255)
		{
			throw std::runtime_error("Unsupported PPM max value in: " + path);
		}

		TextureImage image;
		image.width = width;
		image.height = height;
		image.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);

		if (magic == "P6")
		{
			std::vector<unsigned char> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);
			file.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
			if (file.gcount() != static_cast<std::streamsize>(rgb.size()))
			{
				throw std::runtime_error("Unexpected end of PPM texture data: " + path);
			}
			for (std::size_t i = 0U, j = 0U; i < rgb.size(); i += 3U, j += 4U)
			{
				image.pixels[j + 0U] = rgb[i + 0U];
				image.pixels[j + 1U] = rgb[i + 1U];
				image.pixels[j + 2U] = rgb[i + 2U];
				image.pixels[j + 3U] = 255U;
			}
		}
		else
		{
			for (std::size_t i = 0U; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i)
			{
				const unsigned char r = static_cast<unsigned char>(std::stoi(readToken(file)));
				const unsigned char g = static_cast<unsigned char>(std::stoi(readToken(file)));
				const unsigned char b = static_cast<unsigned char>(std::stoi(readToken(file)));
				image.pixels[i * 4U + 0U] = r;
				image.pixels[i * 4U + 1U] = g;
				image.pixels[i * 4U + 2U] = b;
				image.pixels[i * 4U + 3U] = 255U;
			}
		}

		return image;
	}

	TextureImage TextureLoader::makeFallbackCheckerboard()
	{
		TextureImage image;
		image.width = 4U;
		image.height = 4U;
		image.pixels = {
			255, 255, 255, 255, 255, 105, 180, 255, 255, 255, 255, 255, 255, 105, 180, 255,
			255, 105, 180, 255, 255, 255, 255, 255, 255, 105, 180, 255, 255, 255, 255, 255,
			255, 255, 255, 255, 255, 105, 180, 255, 255, 255, 255, 255, 255, 105, 180, 255,
			255, 105, 180, 255, 255, 255, 255, 255, 255, 105, 180, 255, 255, 255, 255, 255};
		return image;
	}

} // namespace scop
