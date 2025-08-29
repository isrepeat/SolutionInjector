//#pragma once
//#include <Helpers/Std/Extensions/memoryEx.h>
//#include <Helpers/Flags.h>
//#include <Helpers/Guid.h>
//
//#include "SolutionStructure.h"
//#include <unordered_set>
//#include <unordered_map>
//#include <optional>
//
//namespace Core {
//	class SolutionMerger {
//	public:
//		enum MergeFlags {
//			None,
//			IncludeParentsForFolders,
//		};
//
//	public:
//		explicit SolutionMerger(const SolutionStructure& sourceSlnStructure);
//
//		std::unique_ptr<SolutionStructure> Merge(
//			const SolutionStructure& targetSlnStructure,
//			const std::unordered_set<std::string>& projectNamesToInsert,
//			const std::unordered_set<std::string>& folderNamesToInsert,
//			const H::Flags<MergeFlags> mergeFlags = MergeFlags::None,
//			const std::optional<std::string> rootFolderNameOpt = std::nullopt);
//
//	private:
//		struct MergeContext {
//			const SolutionStructure& targetSlnStructure;
//			const std::unordered_set<std::string>& projectNamesToInsert;
//			const std::unordered_set<std::string>& folderNamesToInsert;
//			const H::Flags<MergeFlags> mergeFlags;
//			const std::optional<std::string> rootFolderNameOpt;
//
//			MergeContext(
//				const SolutionStructure& targetSlnStructure,
//				const std::unordered_set<std::string>& projectNamesToInsert,
//				const std::unordered_set<std::string>& folderNamesToInsert,
//				const H::Flags<MergeFlags> mergeFlags,
//				const std::optional<std::string> rootFolderNameOpt)
//				: targetSlnStructure{ targetSlnStructure }
//				, projectNamesToInsert{ projectNamesToInsert }
//				, folderNamesToInsert{ folderNamesToInsert }
//				, mergeFlags{ mergeFlags }
//				, rootFolderNameOpt{ rootFolderNameOpt } 
//			{}
//		};
//
//
//		std::vector<std::ex::shared_ptr<SolutionNode>> CollectSolutionNodesToInsert(MergeContext& mergeCtx);
//
//		void CollectParentsRecursive(
//			const std::ex::shared_ptr<SolutionNode>& solutionNode,
//			std::unordered_set<H::Guid>& visitedGuids,
//			std::vector<std::ex::shared_ptr<SolutionNode>>& outSolutionNodes);
//
//		void CollectChildrenRecursive(
//			const std::ex::shared_ptr<SolutionNode>& solutionNode,
//			std::unordered_set<H::Guid>& visitedGuids,
//			std::vector<std::ex::shared_ptr<SolutionNode>>& outSolutionNodes);
//
//		std::vector<std::ex::shared_ptr<SolutionNode>> CloneSolutionNodes(
//			const std::vector<std::ex::shared_ptr<SolutionNode>>& sourceSolutionNodes);
//
//		//void RemapGuidsIfNeeded(
//		//	std::vector<std::ex::shared_ptr<SolutionNode>>& projects,
//		//	MergeContext& ctx);
//
//		void FilterProjectConfigurations(
//			const std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodes,
//			MergeContext& mergeCtx);
//
//		std::ex::shared_ptr<SolutionFolder> WrapSolutionNodesToRootFolder(
//			const std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodes,
//			MergeContext& mergeCtx);
//
//	private:
//		const SolutionStructure& sourceSlnStructure;
//	};
//}