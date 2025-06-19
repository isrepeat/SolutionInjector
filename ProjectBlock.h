#pragma once
#include <string>
#include <vector>
#include <optional>

namespace Core {
	class ProjectBlock {
	public:
		std::string typeGuid;
		std::string name;
		std::string path;
		std::string guid;
		std::vector<std::string> lines;

		static std::optional<ProjectBlock> Parse(const std::vector<std::string>& lines, size_t& index);
		void ReplaceGuid(const std::string& newGuid);
	};
}