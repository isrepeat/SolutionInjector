#pragma once
#include <Helpers/Flags.h>
#include <Helpers/Guid.h>
#include "SolutionStructure.h"
#include <unordered_set>
#include <unordered_map>
#include <optional>

namespace Core {
	class SolutionMerger {
	public:
		enum MergeFlags {
			None,
			IncludeParentsForFolders,
		};

	public:
		explicit SolutionMerger(const SolutionStructure& sourceSlnStructure);

		std::unique_ptr<SolutionStructure> Merge(
			const SolutionStructure& targetSlnStructure,
			const std::unordered_set<std::string>& projectNamesToInsert,
			const std::unordered_set<std::string>& folderNamesToInsert,
			const H::Flags<MergeFlags> mergeFlags = MergeFlags::None,
			const std::optional<std::string> rootFolderNameOpt = std::nullopt);

	private:
		struct MergeContext {
			const SolutionStructure& targetSlnStructure;
			const std::unordered_set<std::string>& projectNamesToInsert;
			const std::unordered_set<std::string>& folderNamesToInsert;
			const H::Flags<MergeFlags> mergeFlags;
			const std::optional<std::string> rootFolderNameOpt;

			MergeContext(
				const SolutionStructure& targetSlnStructure,
				const std::unordered_set<std::string>& projectNamesToInsert,
				const std::unordered_set<std::string>& folderNamesToInsert,
				const H::Flags<MergeFlags> mergeFlags,
				const std::optional<std::string> rootFolderNameOpt)
				: targetSlnStructure{ targetSlnStructure }
				, projectNamesToInsert{ projectNamesToInsert }
				, folderNamesToInsert{ folderNamesToInsert }
				, mergeFlags{ mergeFlags }
				, rootFolderNameOpt{ rootFolderNameOpt } 
			{}
		};


		std::vector<std::shared_ptr<SolutionProject>> CollectProjectsAndFoldersToInsert(MergeContext& ctx);

		void CollectParentsRecursive(
			const std::shared_ptr<SolutionProject>& project,
			std::unordered_set<H::Guid>& visitedGuids,
			std::vector<std::shared_ptr<SolutionProject>>& outProjects);

		void CollectDescendantsRecursive(
			const std::shared_ptr<SolutionProject>& project,
			std::unordered_set<H::Guid>& visitedGuids,
			std::vector<std::shared_ptr<SolutionProject>>& outProjects);

		std::vector<std::shared_ptr<SolutionProject>> CloneProjectsPreservingHierarchy(
			const std::vector<std::shared_ptr<SolutionProject>>& sourceProjects);

		//void RemapGuidsIfNeeded(
		//	std::vector<std::shared_ptr<SolutionProject>>& projects,
		//	MergeContext& ctx);

		std::shared_ptr<SolutionProject> CreateInsertedRootFolder(
			const std::vector<std::shared_ptr<SolutionProject>>& projects,
			MergeContext& ctx);

	private:
		const SolutionStructure& sourceSlnStructure;
	};
}