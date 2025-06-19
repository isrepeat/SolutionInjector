#pragma once
#include "SolutionFile.h"
#include "GuidGenerator.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>

namespace Core {
	class SolutionMerger {
	public:
		void Merge(
			const std::string& targetPath,
			const std::string& sourcePath,
			const std::unordered_set<std::string>& selectedProjectNames,
			const std::unordered_set<std::string>& selectedFolderNames
		);

	private:
		void CollectDescendantProjects(
			const SolutionFile& source,
			const std::unordered_map<std::string, std::string>& guidToName,
			const std::unordered_map<std::string, std::string>& nestedMap,
			const std::string& parentGuid,
			std::unordered_set<std::string>& selectedProjectNames
		);

		std::vector<std::string> GenerateUpdatedNestedLines(
			const std::vector<std::string>& original,
			const std::unordered_map<std::string, std::string>& guidMap
		);

		std::vector<std::string> GenerateUpdatedConfigLines(
			const SolutionFile& source,
			const std::unordered_map<std::string, std::string>& guidMap
		);
	};
}