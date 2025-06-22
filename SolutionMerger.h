#pragma once
#include <Helpers/Flags.h>
#include <Helpers/Guid.h>
#include "SolutionStructure.h"
#include <unordered_set>
#include <unordered_map>

namespace Core {
	class SolutionMerger {
	public:
		enum MergeFlags {
			None,
			IncludeParentsForFolders,
		};

	public:
		SolutionMerger(
			SolutionStructure& sourceSlnStructure,
			SolutionStructure& targetSlnStructure
		);

		bool Merge(
			const std::unordered_set<std::string>& projectNamesToInsert,
			const std::unordered_set<std::string>& folderNamesToInsert,
			H::Flags<MergeFlags> mergeFlags = MergeFlags::None);

	private:
		void CollectProjectsAndFoldersToInsert(
			const std::unordered_set<std::string>& projectNamesToInsert,
			const std::unordered_set<std::string>& folderNamesToInsert,
			H::Flags<MergeFlags> mergeFlags,
			std::vector<std::shared_ptr<SolutionProject>>& outProjects);

		void CollectParentsRecursive(
			const std::shared_ptr<SolutionProject>& project,
			std::unordered_set<H::Guid>& visitedGuids,
			std::vector<std::shared_ptr<SolutionProject>>& outProjects);

		void CollectDescendantsRecursive(
			const std::shared_ptr<SolutionProject>& folder,
			std::unordered_set<H::Guid>& visitedGuids,
			std::vector<std::shared_ptr<SolutionProject>>& outProjects);

		std::vector<std::shared_ptr<SolutionProject>> CloneProjectsPreservingHierarchy(const std::vector<std::shared_ptr<SolutionProject>>& sourceProjects);

		void RemapGuidsIfNeeded(std::vector<std::shared_ptr<SolutionProject>>& projects);
		void InsertProjectsIntoTargetSln(const std::vector<std::shared_ptr<SolutionProject>>& projects);

	private:
		SolutionStructure& sourceSlnStructure;
		SolutionStructure& targetSlnStructure;
		std::unordered_map<H::Guid, H::Guid> guidRemap;
	};
}