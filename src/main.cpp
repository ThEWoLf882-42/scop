#include "App.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

	std::string trim(const std::string &value)
	{
		std::size_t start = 0;
		while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
		{
			++start;
		}

		std::size_t end = value.size();
		while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
		{
			--end;
		}

		return value.substr(start, end - start);
	}

	bool startsWithKeyword(const std::string &line, const std::string &keyword)
	{
		std::size_t i = 0;
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
		{
			++i;
		}

		if (line.compare(i, keyword.size(), keyword) != 0)
		{
			return false;
		}

		const std::size_t after = i + keyword.size();
		return after >= line.size() || std::isspace(static_cast<unsigned char>(line[after]));
	}

	std::string afterKeyword(const std::string &line, const std::string &keyword)
	{
		std::size_t i = 0;
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
		{
			++i;
		}
		i += keyword.size();
		return trim(line.substr(i));
	}

	bool fileExists(const std::string &path)
	{
		std::ifstream file(path.c_str(), std::ios::binary);
		return file.good();
	}

	std::string directoryOf(const std::string &path)
	{
		const std::size_t slash = path.find_last_of("/\\");
		if (slash == std::string::npos)
		{
			return ".";
		}
		if (slash == 0)
		{
			return "/";
		}
		return path.substr(0, slash);
	}

	bool isAbsolutePath(const std::string &path)
	{
		return !path.empty() && (path[0] == '/' || path[0] == '\\');
	}

	std::string joinPath(const std::string &baseDir, const std::string &path)
	{
		if (path.empty() || isAbsolutePath(path))
		{
			return path;
		}
		if (baseDir.empty() || baseDir == ".")
		{
			return path;
		}
		if (baseDir.back() == '/' || baseDir.back() == '\\')
		{
			return baseDir + path;
		}
		return baseDir + "/" + path;
	}

	std::string findMtllibInObj(const std::string &objPath)
	{
		std::ifstream file(objPath.c_str());
		if (!file)
		{
			return "";
		}

		std::string line;
		while (std::getline(file, line))
		{
			const std::string clean = trim(line);
			if (clean.empty() || clean[0] == '#')
			{
				continue;
			}
			if (startsWithKeyword(clean, "mtllib"))
			{
				return afterKeyword(clean, "mtllib");
			}
		}
		return "";
	}

	std::string findMapKdInMtl(const std::string &mtlPath)
	{
		std::ifstream file(mtlPath.c_str());
		if (!file)
		{
			return "";
		}

		std::string line;
		while (std::getline(file, line))
		{
			const std::string clean = trim(line);
			if (clean.empty() || clean[0] == '#')
			{
				continue;
			}
			if (startsWithKeyword(clean, "map_Kd"))
			{
				const std::string rest = afterKeyword(clean, "map_Kd");
				if (rest.empty())
				{
					return "";
				}

				std::istringstream iss(rest);
				std::string token;
				std::string lastToken;
				while (iss >> token)
				{
					lastToken = token;
				}
				return lastToken;
			}
		}
		return "";
	}

	std::string resolveTexturePath(const std::string &objPath, const std::string &explicitTexturePath)
	{
		if (!explicitTexturePath.empty())
		{
			return explicitTexturePath;
		}

		const std::string mtllibRel = findMtllibInObj(objPath);
		if (mtllibRel.empty())
		{
			return "";
		}

		const std::string objDir = directoryOf(objPath);
		const std::string mtlPath = joinPath(objDir, mtllibRel);
		if (!fileExists(mtlPath))
		{
			return "";
		}

		const std::string mapKdRel = findMapKdInMtl(mtlPath);
		if (mapKdRel.empty())
		{
			return "";
		}

		return joinPath(directoryOf(mtlPath), mapKdRel);
	}

} // namespace

int main(int argc, char **argv)
{
	try
	{
		const std::string modelPath = (argc > 1) ? argv[1] : "assets/demo_cube.obj";
		const std::string explicitTexturePath = (argc > 2) ? argv[2] : "";

		std::string texturePath = resolveTexturePath(modelPath, explicitTexturePath);

		if (!explicitTexturePath.empty())
		{
			std::cout << "Using explicit texture: " << explicitTexturePath << '\n';
		}
		else if (!texturePath.empty())
		{
			std::cout << "Using texture from MTL: " << texturePath << '\n';
		}
		else
		{
			std::cout << "No explicit texture or usable MTL texture found.\n";
		}

		scop::ScopApp app;
		app.run(modelPath, texturePath);
		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}
}