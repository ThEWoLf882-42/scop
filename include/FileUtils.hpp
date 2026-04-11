#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace scop
{

	std::vector<std::uint8_t> readBinaryFile(const std::string &path);

} // namespace scop
