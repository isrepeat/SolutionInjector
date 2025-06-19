#include "SolutionMerger.h"
#include <iostream>
#include <regex>

namespace Core {
	void SolutionMerger::Merge(
		const std::string& targetPath,
		const std::string& sourcePath,
		const std::unordered_set<std::string>& selectedProjectNames,
		const std::unordered_set<std::string>& selectedFolderNames
	) {
		SolutionFile target;
		SolutionFile source;
		if (!target.Load(targetPath) || !source.Load(sourcePath)) {
			std::cerr << "Failed to load source or target solution" << std::endl;
			return;
		}

		std::unordered_set<std::string> selectedNames = selectedProjectNames;
		auto guidToName = [&]() {
			std::unordered_map<std::string, std::string> result;
			for (const auto& block : source.GetAllProjects()) {
				result[block.guid] = block.name;
			}
			return result;
			}();

		auto nestedMap = source.GetNestedMap();

		for (const auto& folder : selectedFolderNames) {
			for (const auto& [guid, name] : guidToName) {
				if (name == folder) {
					CollectDescendantProjects(source, guidToName, nestedMap, guid, selectedNames);
					break;
				}
			}
		}

		auto existingGuids = target.GetGuids();
		std::unordered_map<std::string, std::string> guidMap;
		std::vector<ProjectBlock> toInsert;

		for (auto block : source.GetAllProjects()) {
			if (selectedNames.count(block.name)) {
				std::string newGuid = block.guid;
				while (existingGuids.count(newGuid)) newGuid = GuidGenerator::Generate();
				if (newGuid != block.guid) block.ReplaceGuid(newGuid);
				existingGuids.insert(newGuid);
				guidMap[block.guid] = newGuid;
				toInsert.push_back(block);
			}
		}

		target.InsertProjects(toInsert);
		target.InsertConfigurations(GenerateUpdatedConfigLines(source, guidMap));
		target.InsertNestedLines(GenerateUpdatedNestedLines(source.GetLines(), guidMap));

		target.Save(targetPath);
		std::cout << "Merge completed." << std::endl;
	}

	void SolutionMerger::CollectDescendantProjects(
		const SolutionFile& source,
		const std::unordered_map<std::string, std::string>& guidToName,
		const std::unordered_map<std::string, std::string>& nestedMap,
		const std::string& parentGuid,
		std::unordered_set<std::string>& selectedProjectNames
	) {
		for (const auto& [child, parent] : nestedMap) {
			if (parent == parentGuid) {
				auto it = guidToName.find(child);
				if (it != guidToName.end()) {
					selectedProjectNames.insert(it->second);
					CollectDescendantProjects(source, guidToName, nestedMap, child, selectedProjectNames);
				}
			}
		}
	}

	std::vector<std::string> SolutionMerger::GenerateUpdatedNestedLines(
		const std::vector<std::string>& original,
		const std::unordered_map<std::string, std::string>& guidMap
	) {
		std::vector<std::string> result;
		bool inBlock = false;
		for (const auto& line : original) {
			if (line.find("GlobalSection(NestedProjects)") != std::string::npos) {
				inBlock = true;
			}
			else if (inBlock && line.find("EndGlobalSection") != std::string::npos) {
				break;
			}
			else if (inBlock) {
				std::string replaced = line;
				for (const auto& [oldG, newG] : guidMap) {
					replaced = std::regex_replace(replaced, std::regex(oldG), newG);
				}
				result.push_back(replaced);
			}
		}
		return result;
	}

	std::vector<std::string> SolutionMerger::GenerateUpdatedConfigLines(
		const SolutionFile& source,
		const std::unordered_map<std::string, std::string>& guidMap
	) {
		std::vector<std::string> result;
		bool inBlock = false;
		for (const auto& line : source.GetLines()) {
			if (line.find("GlobalSection(ProjectConfigurationPlatforms)") != std::string::npos) {
				inBlock = true;
			}
			else if (inBlock && line.find("EndGlobalSection") != std::string::npos) {
				break;
			}
			else if (inBlock) {
				std::string replaced = line;
				for (const auto& [oldG, newG] : guidMap) {
					replaced = std::regex_replace(replaced, std::regex(oldG), newG);
				}
				result.push_back(replaced);
			}
		}
		return result;
	}
}