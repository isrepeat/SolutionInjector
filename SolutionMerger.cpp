#include <Helpers/Logger.h>
#include "SolutionMerger.h"

namespace Core {
	SolutionMerger::SolutionMerger(const SolutionStructure& sourceSlnStructure)
		: sourceSlnStructure(sourceSlnStructure)
	{}

	std::unique_ptr<SolutionStructure> SolutionMerger::Merge(
		const SolutionStructure& targetSlnStructure,
		const std::unordered_set<std::string>& projectNamesToInsert,
		const std::unordered_set<std::string>& folderNamesToInsert,
		H::Flags<MergeFlags> mergeFlags,
		std::optional<std::string> rootFolderNameOpt
	) {
		if (!targetSlnStructure.IsParsed()) {
			const_cast<SolutionStructure&>(targetSlnStructure).Parse();
		}

		MergeContext ctx{
			targetSlnStructure,
			projectNamesToInsert,
			folderNamesToInsert,
			mergeFlags,
			rootFolderNameOpt
		};

		auto projectsToInsert = this->CollectProjectsAndFoldersToInsert(ctx);
		auto copiedProjectsToInsert = this->CloneProjectsPreservingHierarchy(projectsToInsert);

		//this->RemapGuidsIfNeeded(copiedProjectsToInsert, ctx);
		auto rootFolderToInsert = this->CreateInsertedRootFolder(copiedProjectsToInsert, ctx);

		auto targetSlnStructureCopy = std::make_unique<SolutionStructure>(targetSlnStructure);
		targetSlnStructureCopy->AddSolutionFolder(rootFolderToInsert);

		return targetSlnStructureCopy;
	}


	std::vector<std::shared_ptr<SolutionProject>> SolutionMerger::CollectProjectsAndFoldersToInsert(MergeContext& ctx) {
		std::unordered_set<H::Guid> visitedGuids;
		std::vector<std::shared_ptr<SolutionProject>> outProjects;

		for (const auto& [guid, project] : this->sourceSlnStructure.GetProjects()) {
			bool isProjectMatch = ctx.projectNamesToInsert.contains(project->projectName);
			bool isFolderMatch =
				project->projectTypeGuid == SolutionStructure::SolutionFolderGuid &&
				ctx.folderNamesToInsert.contains(project->projectName);

			if (isProjectMatch) {
				this->CollectParentsRecursive(project, visitedGuids, outProjects);
			}
			else if (isFolderMatch) {
				if (ctx.mergeFlags.Has(MergeFlags::IncludeParentsForFolders)) {
					this->CollectParentsRecursive(project, visitedGuids, outProjects);
				}
				this->CollectDescendantsRecursive(project, visitedGuids, outProjects);
			}
		}

		return outProjects;
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
		const std::shared_ptr<SolutionProject>& project,
		std::unordered_set<H::Guid>& visitedGuids,
		std::vector<std::shared_ptr<SolutionProject>>& outProjects
	) {
		if (!project) {
			return;
		}

		// Мы не добавляем текущую папку, если она уже была обработана (через проход CollectParentsRecursive)
		bool wasAlreadyVisited = !visitedGuids.insert(project->projectGuid).second;
		if (!wasAlreadyVisited) {
			outProjects.push_back(project);
		}

		for (const auto& child : project->children) {
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


	//void SolutionMerger::RemapGuidsIfNeeded(
	//	std::vector<std::shared_ptr<SolutionProject>>& projects,
	//	MergeContext& ctx
	//) {
	//	std::unordered_set<H::Guid> existingGuids;
	//	for (const auto& [guid, proj] : ctx.targetSlnStructure.GetProjects()) {
	//		existingGuids.insert(guid);
	//	}

	//	for (auto& project : projects) {
	//		auto originalGuid = project->projectGuid;
	//		auto newGuid = originalGuid;

	//		while (existingGuids.contains(newGuid)) {
	//			newGuid = H::Guid::NewGuid();
	//		}

	//		if (newGuid != originalGuid) {
	//			this->guidRemap[originalGuid] = newGuid;
	//			project->projectGuid = newGuid;
	//		}
	//	}
	//}


	std::shared_ptr<SolutionProject> SolutionMerger::CreateInsertedRootFolder(
		const std::vector<std::shared_ptr<SolutionProject>>& projects,
		MergeContext& ctx
	) {
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
		std::unordered_set<std::string> allowedConfigPlatformEntries;
		for (const auto& slnConfig : ctx.targetSlnStructure.GetSolutionConfigurations()) {
			allowedConfigPlatformEntries.insert(slnConfig.configurationAndPlatform);
		}

		// Корректируем список конфигураций
		for (const auto& project : projects) {
			std::vector<ConfigEntry> filteredProjectConfigurations;

			for (const auto& projectConfig : project->configurations) {
				if (allowedConfigPlatformEntries.contains(projectConfig.configurationAndPlatform)) {
					filteredProjectConfigurations.push_back(projectConfig);
				}
			}
			project->configurations = std::move(filteredProjectConfigurations);
		}


		auto rootFolderName = ctx.rootFolderNameOpt.has_value()
			? ctx.rootFolderNameOpt.value()
			: this->sourceSlnStructure.GetSolutionPath().stem().string();

		auto rootFolderGuid = H::Guid::NewGuid();
		auto rootFolder = std::make_shared<SolutionProject>(
			SolutionStructure::SolutionFolderGuid,
			rootFolderGuid,
			rootFolderName,
			rootFolderName
		);

		auto sourceSlnDir = std::filesystem::absolute(this->sourceSlnStructure.GetSolutionPath()).parent_path();
		auto targetSlnDir = std::filesystem::absolute(ctx.targetSlnStructure.GetSolutionPath()).parent_path();

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

		return rootFolder;
	}
}