#include "DefaultSolutionStructureProvider.h"
#include <Helpers/Std/Extensions/rangesEx.h>

namespace Core {
	DefaultSolutionStructureProvider::DefaultSolutionStructureProvider(
		std::filesystem::path solutionFile,
		std::ex::weak_ptr<Model::Global::ParsedGlobalBlock> globalBlockWeak
	)
		: solutionInfo{
			.solutionFile = solutionFile,
			.solutionGuid = DefaultSolutionStructureProvider::ExtractSolutionGuid(globalBlockWeak)
		}
		, globalBlockWeak{ globalBlockWeak } {
	}


	//
	// ISolutionInfo
	//
	const Model::SolutionInfo DefaultSolutionStructureProvider::GetSolutionInfo() const {
		return this->solutionInfo;
	}


	//
	// IProjectBlockReader
	//
	std::ex::shared_ptr<Model::Project::ParsedProjectBlock> DefaultSolutionStructureProvider::GetProjectBlock(const H::Guid& guid) const {
		auto it = this->mapGuidToProjectBlockWeak.find(guid);
		if (it == this->mapGuidToProjectBlockWeak.end()) {
			return nullptr;
		}
		return it->second.lock();
	}


	//
	// IProjectEntriesReader
	//
	std::ex::shared_ptr<Model::Project::SolutionNode> DefaultSolutionStructureProvider::GetParent(const H::Guid& guid) const {
		auto it = this->mapGuidToProjectRelation.find(guid);
		if (it == this->mapGuidToProjectRelation.end()) {
			return nullptr;
		}
		return it->second.parentNodeWeak.lock();
	}


	std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>> DefaultSolutionStructureProvider::GetChildren(const H::Guid& guid) const {
		std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>> result;

		auto it = this->mapGuidToProjectRelation.find(guid);
		if (it == this->mapGuidToProjectRelation.end()) {
			return {};
		}

		for (const auto& childWeak : it->second.childrenNodesWeak) {
			if (auto childShared = childWeak.lock()) {
				result.push_back(childShared);
			}
		}

		return result;
	}


	std::vector<Model::Entries::ConfigEntry> DefaultSolutionStructureProvider::GetConfigurations(const H::Guid& guid) const {
		auto it = this->mapGuidToConfigurations.find(guid);
		if (it == this->mapGuidToConfigurations.end()) {
			return {};
		}
		return it->second;
	}


	std::vector<Model::Entries::SharedMsBuildProjectFileEntry> DefaultSolutionStructureProvider::GetSharedMsBuildProjectFiles(const H::Guid& guid) const {
		auto it = this->mapGuidToSharedMsBuildProjectFiles.find(guid);
		if (it == this->mapGuidToSharedMsBuildProjectFiles.end()) {
			return {};
		}
		return it->second;
	}


	//
	// API
	//
	void DefaultSolutionStructureProvider::RebuildFromView(
		std::ex::shared_ptr<SolutionStructure::View> solutionStructureView
	) {
		this->mapGuidToProjectBlockWeak.clear();
		this->mapGuidToProjectRelation.clear();
		this->mapGuidToConfigurations.clear();
		this->mapGuidToSharedMsBuildProjectFiles.clear();

		if (!solutionStructureView) {
			return;
		}

		auto globalBlock = this->globalBlockWeak.lock();
		if (!globalBlock) {
			return;
		}

		//
		// mapGuidToProjectBlockWeak
		//
		{
			using Pair_t = std::pair<
				H::Guid,
				std::ex::weak_ptr<Model::Project::ParsedProjectBlock>
			>;
			this->mapGuidToProjectBlockWeak =
				solutionStructureView->mapGuidToProjectBlock
				| std::ranges::views::transform(
					[](const std::pair<H::Guid, std::ex::shared_ptr<Model::Project::ParsedProjectBlock>>& kvp) {
						return Pair_t{ kvp.first, kvp.second };
					})
				| std::ex::ranges::views::to<std::unordered_map<typename Pair_t::first_type, typename Pair_t::second_type>>();
		}

		//
		// NestedProjects: mapGuidToProjectRelation.
		//
		{
			auto itParsedSection = globalBlock->sectionMap.find(
				Model::Global::ParsedNestedProjectsSection::SectionName
			);
			if (itParsedSection != globalBlock->sectionMap.end()) {
				auto parsedNestedProjectsSection = itParsedSection->second
					.Cast<Model::Global::ParsedNestedProjectsSection>();

				for (const auto& entry : parsedNestedProjectsSection->entries) {
					auto itChildNode = solutionStructureView->mapGuidToSolutionNode.find(entry.childGuid);
					auto itParentNode = solutionStructureView->mapGuidToSolutionNode.find(entry.parentGuid);

					if (itChildNode != solutionStructureView->mapGuidToSolutionNode.end() &&
						itParentNode != solutionStructureView->mapGuidToSolutionNode.end()
						) {
						auto& childNode = itChildNode->second;
						auto& parentNode = itParentNode->second;

						this->mapGuidToProjectRelation[entry.childGuid].parentNodeWeak = parentNode;
						this->mapGuidToProjectRelation[entry.parentGuid].childrenNodesWeak.push_back(childNode);
					}
				}
			}
		}

		//
		// ProjectConfigurationPlatforms: mapGuidToConfigurations.
		//
		{
			auto itParsedSection = globalBlock->sectionMap.find(
				Core::Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName
			);
			if (itParsedSection != globalBlock->sectionMap.end()) {
				auto parsedProjectConfigurationPlatformsSection = itParsedSection->second
					.Cast<Core::Model::Global::ParsedProjectConfigurationPlatformsSection>();

				for (const auto& entry : parsedProjectConfigurationPlatformsSection->entries) {
					this->mapGuidToConfigurations[entry.guid].push_back(entry.configEntry);
				}
			}
		}

		//
		// SharedMSBuildProjectFiles : mapGuidToSharedMsBuildProjectFiles.
		//
		{
			auto itParsedSection = globalBlock->sectionMap.find(
				Core::Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName
			);
			if (itParsedSection != globalBlock->sectionMap.end()) {
				auto parsedSharedMSBuildProjectFilesSection = itParsedSection->second
					.Cast<Core::Model::Global::ParsedSharedMSBuildProjectFilesSection>();

				for (const auto& entry : parsedSharedMSBuildProjectFilesSection->entries) {
					this->mapGuidToSharedMsBuildProjectFiles[entry.guid].push_back(entry);
				}
			}
		}
	}


	const H::Guid DefaultSolutionStructureProvider::ExtractSolutionGuid(
		std::ex::weak_ptr<Model::Global::ParsedGlobalBlock> globalBlockWeak
	) {
		auto globalBlock = globalBlockWeak.lock();
		if (!globalBlock) {
			return {};
		}

		auto itParsedSection = globalBlock->sectionMap.find(
			Model::Global::ParsedExtensibilityGlobalsSection::SectionName
		);
		if (itParsedSection != globalBlock->sectionMap.end()) {
			auto parsedExtensibilityGlobalsSection = itParsedSection->second
				.Cast<Model::Global::ParsedExtensibilityGlobalsSection>();

			return parsedExtensibilityGlobalsSection->solutionGuid;
		}

		return {};
	}
}