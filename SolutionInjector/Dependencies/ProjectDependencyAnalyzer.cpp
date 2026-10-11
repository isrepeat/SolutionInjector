#include "ProjectDependencyAnalyzer.h"

#include "../Helpers/TextHelpers.h"

#include "MsBuildDependencyReader.h"

#include <sstream>
#include <regex>

namespace Core {
	namespace Dependencies {
		DependencyGraph ProjectDependencyAnalyzer::Analyze(const SolutionStructure& solutionStructure, IO::FileSnapshot& fileSnapshot) {
			// 1. Проверяем владельцев записей Shared Projects.
			DependencyGraph dependencyGraph;
			const auto view = solutionStructure.GetView();
			const auto solutionPath = std::filesystem::absolute(solutionStructure.GetSolutionInfo().solutionFile);
			const auto sharedSection = view->globalBlock->sectionMap.find("SharedMSBuildProjectFiles");
			if (sharedSection != view->globalBlock->sectionMap.end()) {
				const auto parsed = sharedSection->second.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();
				for (const auto& entry : parsed->entries) {
					if (!view->mapGuidToSolutionNode.contains(entry.guid) || entry.key != "SharedItemsImports") {
						dependencyGraph.issues.insert("Unresolved SharedMSBuildProjectFiles owner or key");
					}
				}
			}

			// 2. Сопоставляем нормализованные пути проектов с GUID решения.
			std::map<std::string, std::string> projectPaths;
			for (const auto& entry : view->mapGuidToSolutionNode) {
				if (entry.second.Is<Model::Project::ProjectNode>()) {
					const auto key = IO::FileSnapshot::PathKey(solutionPath.parent_path() / entry.second->relativePath);
					if (!projectPaths.emplace(key, entry.first.ToString()).second) {
						dependencyGraph.issues.insert("Duplicate project path: " + key);
					}
				}
			}
			for (const auto& entry : view->mapGuidToSolutionNode) {
				if (!entry.second.Is<Model::Project::ProjectNode>()) {
					continue;
				}
				const auto guid = entry.first.ToString();
				const auto projectNode = entry.second.Cast<Model::Project::ProjectNode>();

				// 3. Добавляем XML-зависимости текущего проекта и метаданные Shared Projects.
				MsBuildDependencyReader msBuildDependencyReader(fileSnapshot);
				msBuildDependencyReader.Read(solutionPath.parent_path() / projectNode->relativePath, solutionPath, projectPaths);
				dependencyGraph.edges[guid] = msBuildDependencyReader.targets;
				dependencyGraph.issues.insert(msBuildDependencyReader.issues.begin(), msBuildDependencyReader.issues.end());
				for (const auto& shared : projectNode->GetSharedMsBuildProjectFiles()) {
					const auto found = projectPaths.find(IO::FileSnapshot::PathKey(solutionPath.parent_path() / shared.relativePath));
					if (found == projectPaths.end()) {
						dependencyGraph.issues.insert("Missing shared project: " + Helpers::WideToUtf8(shared.relativePath.wstring()));
					} else {
						dependencyGraph.edges[guid].insert(found->second);
					}
				}
				const auto block = projectNode->GetProjectBlock();

				// 4. Дополняем граф явными зависимостями из секции решения.
				const auto section = block->sectionMap.find("ProjectDependencies");
				if (section != block->sectionMap.end()) {
					const std::regex dependency(R"(^\s*(\{[0-9A-Fa-f-]+\})\s*=\s*(\{[0-9A-Fa-f-]+\})\s*$)");
					std::istringstream stream(section->second->Serialize(std::nullopt));
					for (std::string line; std::getline(stream, line);) {
						if (line.find("ProjectSection") != std::string::npos || Helpers::Trim(line).empty()) {
							continue;
						}
						std::smatch match;
						if (!std::regex_match(line, match, dependency) || Helpers::ToUpperAscii(match[1]) != Helpers::ToUpperAscii(match[2])) {
							dependencyGraph.issues.insert("Malformed ProjectDependencies: " + line);
							continue;
						}
						const auto targetGuid = H::Guid::Parse(match[1]);
						const auto target = view->mapGuidToSolutionNode.find(targetGuid);
						if (target == view->mapGuidToSolutionNode.end() || !target->second.Is<Model::Project::ProjectNode>()) {
							dependencyGraph.issues.insert("Missing dependency: " + targetGuid.ToString());
						} else {
							dependencyGraph.edges[guid].insert(targetGuid.ToString());
						}
					}
				}
			}
			return dependencyGraph;
		}
	}
}