#include <Helpers/Logger.h>
#include "SolutionMerger.h"

namespace Core {
	SolutionMerger::SolutionMerger(
		SolutionStructure& sourceSlnStructure,
		SolutionStructure& targetSlnStructure)
		: sourceSlnStructure(sourceSlnStructure)
		, targetSlnStructure(targetSlnStructure)
	{}

	bool SolutionMerger::Merge(
		const std::unordered_set<std::string>& projectNamesToInsert,
		const std::unordered_set<std::string>& folderNamesToInsert,
		H::Flags<MergeFlags> mergeFlags
	) {
		std::vector<std::shared_ptr<SolutionProject>> sourceProjectsToInsert;
		this->CollectProjectsAndFoldersToInsert(
			projectNamesToInsert,
			folderNamesToInsert,
			mergeFlags,
			sourceProjectsToInsert
		);

		auto copiedSourceProjectsToInsert = this->CloneProjectsPreservingHierarchy(sourceProjectsToInsert);

		this->RemapGuidsIfNeeded(copiedSourceProjectsToInsert);
		this->InsertProjectsIntoTargetSln(copiedSourceProjectsToInsert);
		return true;
	}


	void SolutionMerger::CollectProjectsAndFoldersToInsert(
		const std::unordered_set<std::string>& projectNamesToInsert,
		const std::unordered_set<std::string>& folderNamesToInsert,
		H::Flags<MergeFlags> mergeFlags,
		std::vector<std::shared_ptr<SolutionProject>>& outProjects
	) {
		std::unordered_set<H::Guid> visitedGuids;

		for (const auto& [guid, project] : this->sourceSlnStructure.GetProjects()) {
			bool isProjectMatch = projectNamesToInsert.contains(project->projectName);
			bool isFolderMatch =
				project->projectTypeGuid == SolutionStructure::SolutionFolderGuid &&
				folderNamesToInsert.contains(project->projectName);

			if (isProjectMatch) {
				this->CollectParentsRecursive(project, visitedGuids, outProjects);
			}
			else if (isFolderMatch) {
				if (mergeFlags.Has(MergeFlags::IncludeParentsForFolders)) {
					this->CollectParentsRecursive(project, visitedGuids, outProjects);
				}
				this->CollectDescendantsRecursive(project, visitedGuids, outProjects);
			}
		}
	}


	void SolutionMerger::CollectParentsRecursive(
		const std::shared_ptr<SolutionProject>& project,
		std::unordered_set<H::Guid>& visitedGuids,
		std::vector<std::shared_ptr<SolutionProject>>& outProjects
	) {
		if (!project || visitedGuids.contains(project->projectGuid)) {
			return;
		}

		visitedGuids.insert(project->projectGuid);

		if (auto parent = project->parentWeak.lock()) {
			this->CollectParentsRecursive(parent, visitedGuids, outProjects);
		}

		outProjects.push_back(project);
	}


	void SolutionMerger::CollectDescendantsRecursive(
		const std::shared_ptr<SolutionProject>& node,
		std::unordered_set<H::Guid>& visitedGuids,
		std::vector<std::shared_ptr<SolutionProject>>& outProjects
	) {
		if (!node) {
			return;
		}

		// Мы не добавляем текущую папку, если она уже была обработана (через проход CollectParentsRecursive)
		bool wasAlreadyVisited = !visitedGuids.insert(node->projectGuid).second;
		if (!wasAlreadyVisited) {
			outProjects.push_back(node);
		}

		for (const auto& child : node->children) {
			this->CollectDescendantsRecursive(child, visitedGuids, outProjects);
		}
	}


	std::vector<std::shared_ptr<SolutionProject>> SolutionMerger::CloneProjectsPreservingHierarchy(
		const std::vector<std::shared_ptr<SolutionProject>>& sourceProjects
	) {
		std::vector<std::shared_ptr<SolutionProject>> result;
		std::unordered_map<H::Guid, std::shared_ptr<SolutionProject>> sourceProjectsCopyiesMap;

		// Клонируем все проекты без связей
		for (const auto& sourceProject : sourceProjects) {
			auto sourceProjectCopy = std::make_shared<SolutionProject>(
				sourceProject->projectTypeGuid,
				sourceProject->projectGuid,
				sourceProject->projectName,
				sourceProject->projectPath.string()
			);
			sourceProjectCopy->configurations = sourceProject->configurations;
			sourceProjectCopy->sharedMsBuildProjectFiles = sourceProject->sharedMsBuildProjectFiles;
			sourceProjectsCopyiesMap[sourceProject->projectGuid] = sourceProjectCopy;
			result.push_back(sourceProjectCopy);
		}

		// Восстанавливаем parent-связи между клонами
		for (const auto& sourceProject : sourceProjects) {
			auto sourceProjectCopy = sourceProjectsCopyiesMap[sourceProject->projectGuid];

			if (auto parent = sourceProject->parentWeak.lock()) {
				auto it = sourceProjectsCopyiesMap.find(parent->projectGuid);
				if (it != sourceProjectsCopyiesMap.end()) {
					it->second->AddChild(sourceProjectCopy);
				}
			}
		}

		return result;
	}


	void SolutionMerger::RemapGuidsIfNeeded(std::vector<std::shared_ptr<SolutionProject>>& projects) {
		std::unordered_set<H::Guid> existingGuids;
		for (const auto& [guid, proj] : this->targetSlnStructure.GetProjects()) {
			existingGuids.insert(guid);
		}

		for (auto& project : projects) {
			auto originalGuid = project->projectGuid;
			auto newGuid = originalGuid;

			while (existingGuids.contains(newGuid)) {
				newGuid = H::Guid::NewGuid();
			}

			if (newGuid != originalGuid) {
				this->guidRemap[originalGuid] = newGuid;
				project->projectGuid = newGuid;
			}
		}
	}


	void SolutionMerger::InsertProjectsIntoTargetSln(const std::vector<std::shared_ptr<SolutionProject>>& projects) {
		// Создаём множество вставляемых GUID'ов
		std::unordered_set<H::Guid> includedGuids;
		for (const auto& project : projects) {
			includedGuids.insert(project->projectGuid);
		}

		// Корректируем список детей
		for (const auto& project : projects) {
			std::vector<std::shared_ptr<SolutionProject>> filteredProjects;
			
			for (const auto& child : project->children) {
				if (includedGuids.contains(child->projectGuid)) {
					filteredProjects.push_back(child);
				}
			}

			project->children = std::move(filteredProjects);
		}


		// Создаём множество допустимых ключей конфигураций
		std::unordered_set<std::string> allowedConfigurationsKeys;
		for (const auto& configEntry : this->targetSlnStructure.GetSolutionConfigurations()) {
			allowedConfigurationsKeys.insert(configEntry.key);
		}

		// Корректируем список конфигураций
		for (const auto& project : projects) {
			std::vector<ConfigEntry> filteredConfigurations;

			for (const auto& config : project->configurations) {
				if (allowedConfigurationsKeys.contains(config.key)) {
					filteredConfigurations.push_back(config);
				}
			}

			project->configurations = std::move(filteredConfigurations);
		}


		auto sourceSlnName = this->sourceSlnStructure.GetSolutionPath().stem().string() + " [submodule]";
		auto sourceSlnDir = std::filesystem::absolute(this->sourceSlnStructure.GetSolutionPath()).parent_path();
		auto targetSlnDir = std::filesystem::absolute(this->targetSlnStructure.GetSolutionPath()).parent_path();

		auto rootFolderGuid = H::Guid::NewGuid();
		auto rootFolder = std::make_shared<SolutionProject>(
			SolutionStructure::SolutionFolderGuid,
			rootFolderGuid,
			sourceSlnName,
			sourceSlnName
		);

		for (const auto& project : projects) {
			// Только те проекты, у которых нет родителя
			if (project->parentWeak.expired()) {
				project->parentWeak = rootFolder;
				rootFolder->children.push_back(project);
			}

			// Пересчитать путь относительно target
			project->projectPath = std::filesystem::relative(
				std::filesystem::absolute(sourceSlnDir / project->projectPath),
				targetSlnDir
			);

			// Пересчитать пути shared записей
			for (auto& shared : project->sharedMsBuildProjectFiles) {
				shared.relativePath = std::filesystem::relative(
					std::filesystem::absolute(sourceSlnDir / shared.relativePath),
					targetSlnDir
				).generic_string();
			}
		}

		// Добавляем папку и все проекты в структуру
		this->targetSlnStructure.AddSolutionFolder(rootFolder);
	}
}