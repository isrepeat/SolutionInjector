//#include "SolutionMerger.h"
//#include <Helpers/Logger.h>
//
//namespace Core {
//	SolutionMerger::SolutionMerger(const SolutionStructure& sourceSlnStructure)
//		: sourceSlnStructure(sourceSlnStructure)
//	{}
//
//	std::unique_ptr<SolutionStructure> SolutionMerger::Merge(
//		const SolutionStructure& targetSlnStructure,
//		const std::unordered_set<std::string>& projectNamesToInsert,
//		const std::unordered_set<std::string>& folderNamesToInsert,
//		H::Flags<MergeFlags> mergeFlags,
//		std::optional<std::string> rootFolderNameOpt
//	) {
//		if (!targetSlnStructure.IsParsed()) {
//			const_cast<SolutionStructure&>(targetSlnStructure).Parse();
//		}
//
//		MergeContext mergeCtx{
//			targetSlnStructure,
//			projectNamesToInsert,
//			folderNamesToInsert,
//			mergeFlags,
//			rootFolderNameOpt
//		};
//
//		auto solutionNodesToInsert = this->CollectSolutionNodesToInsert(mergeCtx);
//		auto clonedSolutionNodesToInsert = this->CloneSolutionNodes(solutionNodesToInsert);
//
//		//this->RemapGuidsIfNeeded(copiedProjectsToInsert, ctx);
//		this->FilterProjectConfigurations(clonedSolutionNodesToInsert, mergeCtx);
//		auto rootFolderToInsert = this->WrapSolutionNodesToRootFolder(clonedSolutionNodesToInsert, mergeCtx);
//
//		auto targetSlnStructureCopy = std::make_unique<SolutionStructure>(targetSlnStructure);
//		targetSlnStructureCopy->AddSolutionFolder(rootFolderToInsert);
//
//		return targetSlnStructureCopy;
//	}
//
//
//	std::vector<std::ex::shared_ptr<SolutionNode>> SolutionMerger::CollectSolutionNodesToInsert(MergeContext& mergeCtx) {
//		std::unordered_set<H::Guid> visitedGuids;
//		std::vector<std::ex::shared_ptr<SolutionNode>> outSolutionNodes;
//
//		for (const auto& [guid, solutionNode] : this->sourceSlnStructure.GetSolutionNodes()) {
//			if (!mergeCtx.projectNamesToInsert.contains(solutionNode->name) &&
//				!mergeCtx.folderNamesToInsert.contains(solutionNode->name)
//				) {
//				continue;
//			}
//
//			if (solutionNode.Is<SolutionFolder>()) {
//				if (mergeCtx.mergeFlags.Has(MergeFlags::IncludeParentsForFolders)) {
//					this->CollectParentsRecursive(solutionNode, visitedGuids, outSolutionNodes);
//				}
//				this->CollectChildrenRecursive(solutionNode, visitedGuids, outSolutionNodes);
//			}
//			else {
//				this->CollectParentsRecursive(solutionNode, visitedGuids, outSolutionNodes);
//			}
//		}
//
//		return outSolutionNodes;
//	}
//
//	
//	void SolutionMerger::CollectParentsRecursive(
//		const std::ex::shared_ptr<SolutionNode>& solutionNode,
//		std::unordered_set<H::Guid>& visitedGuids,
//		std::vector<std::ex::shared_ptr<SolutionNode>>& outSolutionNodes
//	) {
//		if (!solutionNode || visitedGuids.contains(solutionNode->guid)) {
//			return;
//		}
//
//		visitedGuids.insert(solutionNode->guid);
//
//		if (auto parentSolutionNode = solutionNode->parentNodeWeak.lock()) {
//			this->CollectParentsRecursive(parentSolutionNode, visitedGuids, outSolutionNodes);
//		}
//
//		// Вызываем после рекурсии чтобы добавление шло в порядке от родителя к ребенку.
//		outSolutionNodes.push_back(solutionNode);
//	}
//
//
//	void SolutionMerger::CollectChildrenRecursive(
//		const std::ex::shared_ptr<SolutionNode>& solutionNode,
//		std::unordered_set<H::Guid>& visitedGuids,
//		std::vector<std::ex::shared_ptr<SolutionNode>>& outSolutionNodes
//	) {
//		if (!solutionNode) {
//			return;
//		}
//
//		// WORKAROUND: Не добавляем текущую папку, если она уже была обработана (через проход CollectParentsRecursive).
//		bool wasAlreadyVisited = !visitedGuids.insert(solutionNode->guid).second;
//		if (!wasAlreadyVisited) {
//			// Вызываем до рекурсии чтобы добавление шло в порядке от родителя к ребенку.
//			outSolutionNodes.push_back(solutionNode);
//		}
//
//		if (auto solutionFolder = solutionNode.As<SolutionFolder>()) {
//			for (const auto& childSolutionNode : solutionFolder->childNodes) {
//				this->CollectChildrenRecursive(childSolutionNode, visitedGuids, outSolutionNodes);
//			}
//		}
//	}
//
//
//	std::vector<std::ex::shared_ptr<SolutionNode>> SolutionMerger::CloneSolutionNodes(
//		const std::vector<std::ex::shared_ptr<SolutionNode>>& sourceSolutionNodes
//	) {
//		std::vector<std::ex::shared_ptr<SolutionNode>> result;
//		std::unordered_map<H::Guid, std::ex::shared_ptr<SolutionNode>> mapGuidToClonedSolutionNode;
//
//		// Клонируем все проекты без связей
//		for (const auto& sourceSolutionNode : sourceSolutionNodes) {
//			if (sourceSolutionNode.Is<SolutionFolder>()) {
//				auto sourceSolutionFolder = sourceSolutionNode.Cast<SolutionFolder>();
//
//				auto clonedSolutionFolder = std::ex::make_shared_ex<SolutionFolder>(
//					sourceSolutionNode->typeGuid,
//					sourceSolutionNode->guid,
//					sourceSolutionNode->name,
//					sourceSolutionNode->uniquePath
//				);
//
//				result.push_back(clonedSolutionFolder);
//				mapGuidToClonedSolutionNode[sourceSolutionNode->guid] = result.back();
//			}
//			else {
//				auto sourceProjectNode = sourceSolutionNode.Cast<ProjectNode>();
//				
//				auto clonedProjectNode = std::ex::make_shared_ex<ProjectNode>(
//					sourceSolutionNode->typeGuid,
//					sourceSolutionNode->guid,
//					sourceSolutionNode->name,
//					sourceSolutionNode->uniquePath
//				);
//				clonedProjectNode->configurations = sourceProjectNode->configurations;
//				clonedProjectNode->sharedMsBuildProjectFiles = sourceProjectNode->sharedMsBuildProjectFiles;
//
//				result.push_back(clonedProjectNode);
//				mapGuidToClonedSolutionNode[sourceSolutionNode->guid] = result.back();
//			}
//		}
//
//		// Восстанавливаем иерархию между клонированными узлами, 
//		// поскольку у клонированных узлов childNodes и parentNode пустые.
//		for (const auto& sourceSolutionNode : sourceSolutionNodes) {
//			const auto& clonedSolutionNode = mapGuidToClonedSolutionNode[sourceSolutionNode->guid];
//
//			// Получаем родителя исходного узла
//			auto sourceParentSolutionNode = sourceSolutionNode->parentNodeWeak.lock();
//			if (!sourceParentSolutionNode) {
//				continue; // корневой элемент — родителя нет
//			}
//
//			// Родителем может быть только SolutionFolder
//			if (!sourceParentSolutionNode.Is<SolutionFolder>()) {
//				continue;
//			}
//
//			// Находим соответствущий клонированный узел для sourceParentSolutionNode
//			auto clonedIt = mapGuidToClonedSolutionNode.find(sourceParentSolutionNode->guid);
//			if (clonedIt == mapGuidToClonedSolutionNode.end()) {
//				LOG_ASSERT(false, "Unexpected");
//				continue;
//			}
//
//			LOG_ASSERT(clonedIt->second.Is<SolutionFolder>());
//
//			auto clonedSolutionFolder = clonedIt->second.Cast<SolutionFolder>();
//			clonedSolutionFolder->LinkChildNode(clonedSolutionNode);
//		}
//
//		return result;
//	}
//
//
//	//void SolutionMerger::RemapGuidsIfNeeded(
//	//	std::vector<std::ex::shared_ptr<SolutionNode>>& projects,
//	//	MergeContext& ctx
//	//) {
//	//	std::unordered_set<H::Guid> existingGuids;
//	//	for (const auto& [guid, proj] : ctx.targetSlnStructure.GetProjects()) {
//	//		existingGuids.insert(guid);
//	//	}
//
//	//	for (auto& project : projects) {
//	//		auto originalGuid = project->projectGuid;
//	//		auto newGuid = originalGuid;
//
//	//		while (existingGuids.contains(newGuid)) {
//	//			newGuid = H::Guid::NewGuid();
//	//		}
//
//	//		if (newGuid != originalGuid) {
//	//			this->guidRemap[originalGuid] = newGuid;
//	//			project->projectGuid = newGuid;
//	//		}
//	//	}
//	//}
//
//
//	void SolutionMerger::FilterProjectConfigurations(
//		const std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodes,
//		MergeContext& mergeCtx
//	) {
//		// Собираем допустимые конфигурации из target-решения
//		std::unordered_set<std::string> allowedConfigPlatformEntries;
//		for (const auto& slnConfig : mergeCtx.targetSlnStructure.GetSolutionConfigurations()) {
//			allowedConfigPlatformEntries.insert(slnConfig.configurationAndPlatform);
//		}
//
//		// Корректируем конфигурации у каждого проекта
//		for (const auto& solutionNode : solutionNodes) {
//			if (!solutionNode.Is<ProjectNode>()) {
//				continue;
//			}
//
//			auto projectNode = solutionNode.Cast<ProjectNode>();
//			std::vector<ConfigEntry> filteredConfigs;
//
//			for (const auto& config : projectNode->configurations) {
//				if (allowedConfigPlatformEntries.contains(config.configurationAndPlatform)) {
//					filteredConfigs.push_back(config);
//				}
//			}
//
//			projectNode->configurations = std::move(filteredConfigs);
//		}
//	}
//
//
//	std::ex::shared_ptr<SolutionFolder> SolutionMerger::WrapSolutionNodesToRootFolder(
//		const std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodes,
//		MergeContext& mergeCtx
//	) {
//		auto rootFolderName = mergeCtx.rootFolderNameOpt.has_value()
//			? mergeCtx.rootFolderNameOpt.value()
//			: this->sourceSlnStructure.GetSolutionPath().stem().string();
//
//		auto rootFolderGuid = H::Guid::NewGuid();
//		auto rootFolder = std::ex::make_shared_ex<SolutionFolder>(
//			SolutionStructure::SolutionFolderGuid,
//			rootFolderGuid,
//			rootFolderName,
//			rootFolderName
//		);
//
//		auto sourceSlnDir = std::filesystem::absolute(this->sourceSlnStructure.GetSolutionPath()).parent_path();
//		auto targetSlnDir = std::filesystem::absolute(mergeCtx.targetSlnStructure.GetSolutionPath()).parent_path();
//
//		for (const auto& solutionNode : solutionNodes) {
//			// Присоединяем к корню только те узлы, у которых нет родителя
//			if (!solutionNode->parentNodeWeak.lock()) {
//				rootFolder->LinkChildNode(solutionNode);
//			}
//
//			// Пересчитать путь относительно target
//			if (solutionNode.Is<ProjectNode>()) {
//				solutionNode->uniquePath = std::filesystem::relative(
//					std::filesystem::absolute(sourceSlnDir / solutionNode->uniquePath),
//					targetSlnDir
//				);
//			}
//
//			// Пересчитываем пути shared записей
//			if (auto projectNode = solutionNode.As<ProjectNode>()) {
//				for (auto& shared : projectNode->sharedMsBuildProjectFiles) {
//					shared.relativePath = std::filesystem::relative(
//						std::filesystem::absolute(sourceSlnDir / shared.relativePath),
//						targetSlnDir
//					).generic_string();
//				}
//			}
//		}
//
//		return rootFolder;
//	}
//}