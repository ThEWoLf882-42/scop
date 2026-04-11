#include "FileUtils.hpp"

#include <fstream>
#include <stdexcept>

namespace scop
{

	std::vector<std::uint8_t> readBinaryFile(const std::string &path)
	{
		std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("Failed to open file: " + path);
		}

		const std::streamsize size = file.tellg();
		if (size < 0)
		{
			throw std::runtime_error("Failed to read file size: " + path);
		}
		std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
		file.seekg(0);
		file.read(reinterpret_cast<char *>(buffer.data()), size);
		if (!file)
		{
			throw std::runtime_error("Failed to read file: " + path);
		}
		return buffer;
	}

} // namespace scop
