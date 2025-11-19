#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/Event/Signal.h>
#include <Helpers/Guid.h>

#include "Model/Project/ProjectModel.h"
#include "Model/Global/GlobalModel.h"
#include "SolutionStructure.h"

#include <unordered_map>
#include <filesystem>
#include <vector>

namespace Core {
	class DefaultSolutionStructureProvider :
		public Model::ISolutionInfoReader,
		public Model::Project::IProjectBlockReader,
		public Model::Project::IProjectEntriesReader {
	public:
		struct ProjectRelation {
			std::ex::weak_ptr<Model::Project::SolutionNode> parentNodeWeak;
			std::vector<std::ex::weak_ptr<Model::Project::SolutionNode>> childrenNodesWeak;
		};

		DefaultSolutionStructureProvider(
			std::filesystem::path solutionFile,
			std::ex::weak_ptr<Model::Global::ParsedGlobalBlock> globalBlockWeak
		);

		//
		// ISolutionInfoReader
		//
		const Model::SolutionInfo GetSolutionInfo() const override;

		//
		// IProjectBlockReader
		//
		std::ex::shared_ptr<Model::Project::ParsedProjectBlock> GetProjectBlock(const H::Guid& guid) const override;

		//
		// IProjectEntriesReader
		//
		std::ex::shared_ptr<Model::Project::SolutionNode> GetParent(const H::Guid& guid) const override;
		std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>> GetChildren(const H::Guid& guid) const override;
		std::vector<Model::Entries::ConfigEntry> GetConfigurations(const H::Guid& guid) const override;
		std::vector<Model::Entries::SharedMsBuildProjectFileEntry> GetSharedMsBuildProjectFiles(const H::Guid& guid) const override;

		//
		// API
		//
		void RebuildFromView(std::ex::shared_ptr<SolutionStructure::View> solutionStructureView);

	private:
		static const H::Guid ExtractSolutionGuid(
			std::ex::weak_ptr<Model::Global::ParsedGlobalBlock> globalBlockWeak
		);

	private:
		const Model::SolutionInfo solutionInfo;
		std::ex::weak_ptr<Model::Global::ParsedGlobalBlock> globalBlockWeak;
		std::unordered_map<H::Guid, std::ex::weak_ptr<Model::Project::ParsedProjectBlock>> mapGuidToProjectBlockWeak;
		std::unordered_map<H::Guid, ProjectRelation> mapGuidToProjectRelation;
		std::unordered_map<H::Guid, std::vector<Model::Entries::ConfigEntry>> mapGuidToConfigurations;
		std::unordered_map<H::Guid, std::vector<Model::Entries::SharedMsBuildProjectFileEntry>> mapGuidToSharedMsBuildProjectFiles;
	};
}