#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "ProjectBlock.h"

namespace Core {
	class SolutionFile {
	public:
		bool Load(const std::string& path);
		void Save(const std::string& path) const;

		const std::vector<std::string>& GetLines() const;
		std::unordered_set<std::string> GetGuids() const;
		std::vector<ProjectBlock> GetAllProjects() const;
		std::unordered_map<std::string, std::string> GetNestedMap() const;
		void InsertProjects(const std::vector<ProjectBlock>& blocks);
		void InsertNestedLines(const std::vector<std::string>& nestedLines);
		void InsertConfigurations(const std::vector<std::string>& configLines);

	private:
		std::vector<std::string> lines;
	};
}