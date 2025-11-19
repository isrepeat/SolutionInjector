#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/Guid.h>

#include "Model/Project/ProjectModel.h"
#include "Model/Global/GlobalModel.h"
#include "Model/ISerializable.h"

#include <unordered_map>
#include <filesystem>
#include <vector>

namespace Core {
	class DefaultSolutionStructureProvider;

	class SolutionStructure : public Model::ISerializable {
	public:
		struct View {
			const std::unordered_map<H::Guid, std::ex::shared_ptr<Model::Project::ParsedProjectBlock>> mapGuidToProjectBlock;
			const std::unordered_map<H::Guid, std::ex::shared_ptr<Model::Project::SolutionNode>> mapGuidToSolutionNode;
			const std::unordered_map<std::string, std::ex::shared_ptr<Model::Project::SolutionNode>> mapNameToSolutionNode;
			const std::ex::shared_ptr<Model::Global::ParsedGlobalBlock> globalBlock;
			const std::vector<Model::Entries::ConfigEntry> solutionConfigurations;
		};

		SolutionStructure(const std::unique_ptr<Model::Raw::SolutionDocument>& solutionDocument);

		// Запрещаем копирование / перемещение чтоб гарантровать стабильный адресс обьекта для подписок.
		SolutionStructure(const SolutionStructure&) = delete;
		SolutionStructure& operator=(const SolutionStructure&) = delete;

		SolutionStructure(SolutionStructure&&) = delete;
		SolutionStructure& operator=(SolutionStructure&&) = delete;

		//
		// ISerializable
		//
		std::string Serialize(
			std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
		) const override;

		//
		// API
		//
		const Model::SolutionInfo GetSolutionInfo() const;
		std::ex::shared_ptr<View> GetView() const;

		void Save() const;
		void Save(const std::filesystem::path& savePath) const;

		void LogSerializedSolution() const;

		void AddProjectBlock(const std::ex::shared_ptr<Model::Project::ParsedProjectBlock>& srcProjectBlock);
		void RemoveProjectBlock(const H::Guid& projectGuid);

		std::ex::shared_ptr<Model::Project::SolutionFolder> MakeFolder(std::string name);

		void AttachChild(const H::Guid& parentGuid, const H::Guid& childGuid);
		void DettachChild(const H::Guid& parentGuid, const H::Guid& childGuid);

		void RemoveSolutionConfigurationsByDeclaredKeys(
			const std::unordered_set<std::string>& solutionConfigDeclaredKeys
		);

	private:
		void AddProjectBlockRecursive(const std::ex::shared_ptr<Model::Project::ParsedProjectBlock>& srcProjectBlock);
		void RemoveProjectBlockRecursive(const H::Guid& projectGuid);

		void AddProjectNodeEntriesToGlobalBlock(const std::ex::shared_ptr<Model::Project::ProjectNode>& srcProjectNode);
		void RemoveProjectNodeEntriesFromGlobalBlock(const std::ex::shared_ptr<Model::Project::ProjectNode>& projectNode);

		void WriteNestedProjectsEntry(Model::Global::ParsedNestedProjectsSection::Entry entry);
		void WriteSharedMSBuildProjectFilesEntry(Model::Entries::SharedMsBuildProjectFileEntry entry);
		void WriteProjectConfigurationPlatformsEntry(Model::Global::ParsedProjectConfigurationPlatformsSection::Entry entry);

		void DeleteNestedProjectsEntries(
			std::function<bool(const Model::Global::ParsedNestedProjectsSection::Entry&)> predicate
		);
		void DeleteSharedMSBuildProjectFilesEntries(
			std::function<bool(const Model::Entries::SharedMsBuildProjectFileEntry&)> predicate
		);
		void DeleteProjectConfigurationPlatformsEntries(
			std::function<bool(const Model::Global::ParsedProjectConfigurationPlatformsSection::Entry&)> predicate
		);

		std::filesystem::path RebuildPathRelativeToCurrentSolution(const std::filesystem::path& absolutePath);

		void UpdateView();

	private:
		std::ex::shared_ptr<View> view;
		std::ex::shared_ptr<Model::Global::ParsedGlobalBlock> globalBlock;
		std::ex::shared_ptr<DefaultSolutionStructureProvider> solutionStructureProvider;
		std::unordered_map<H::Guid, std::ex::shared_ptr<Model::Project::ParsedProjectBlock>> mapGuidToProjectBlock;
	};
}