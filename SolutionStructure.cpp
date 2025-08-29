#include "SolutionStructure.h"
#include <Helpers/Collection/Algorithms.h>
#include <Helpers/Std/Extensions/fstreamEx.h>
#include <Helpers/Std/Extensions/rangesEx.h>
#include <Helpers/StringComparers.h>
#include <Helpers/Logger.h>

#include "Parsers/ProjectBlocksParser.h"
#include "Parsers/GlobalBlockParser.h"
#include "DefaultSolutionStructureProvider.h"

#include <unordered_set>
#include <algorithm>
#include <ranges>

namespace Core {
	namespace details {
		enum class ProjectTypePriority {
			SolutionFolder,
			Csproj,
			Shproj,
			Vcxproj,
			Vcxitems,
			Wapproj,
			Unknown
		};


		ProjectTypePriority GetProjectTypePriorityByFileExt(
			const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode
		) {
			if (solutionNode.Is<Model::Project::SolutionFolder>()) {
				return ProjectTypePriority::SolutionFolder;
			}

			std::string ext = solutionNode->relativePath.extension().string();

			if (ext == ".csproj") {
				return ProjectTypePriority::Csproj;
			}
			else if (ext == ".shproj") {
				return ProjectTypePriority::Shproj;
			}
			else if (ext == ".vcxproj") {
				return ProjectTypePriority::Vcxproj;
			}
			else if (ext == ".vcxitems") {
				return ProjectTypePriority::Vcxitems;
			}
			else if (ext == ".wapproj") {
				return ProjectTypePriority::Wapproj;
			}
			return ProjectTypePriority::Unknown;
		}


		bool CompareSolutionNodes(
			const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNodeA,
			const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNodeB
		) {
			details::ProjectTypePriority priorityA = details::GetProjectTypePriorityByFileExt(solutionNodeA);
			details::ProjectTypePriority priorityB = details::GetProjectTypePriorityByFileExt(solutionNodeB);

			if (priorityA != priorityB) {
				return static_cast<int>(priorityA) < static_cast<int>(priorityB);
			}

			// Если приоритеты равны - сортируем по имени.
			return H::CaseInsensitiveComparer::IsLess(solutionNodeA->name, solutionNodeB->name);
		}


		void SerializeSolutionNodeRecursively(
			const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode,
			std::string& out
		) {
			out += solutionNode->GetProjectBlock()->Serialize();

			if (solutionNode.Is<Model::Project::SolutionFolder>()) {
				auto solutionFolder = solutionNode.Cast<Model::Project::SolutionFolder>();
				auto children = solutionFolder->GetChildren();

				std::sort(
					children.begin(),
					children.end(),
					&CompareSolutionNodes
				);

				for (auto& childSolutionNode : children) {
					SerializeSolutionNodeRecursively(childSolutionNode, out);
				}
			}
		}
	} // namespace details


	SolutionStructure::SolutionStructure(
		const std::unique_ptr<Model::Raw::SolutionDocument>& solutionDocument
	) {
		this->globalBlock = Parsers::GlobalBlockParser{}.Parse(*solutionDocument);

		this->solutionStructureProvider = std::ex::make_shared_ex<DefaultSolutionStructureProvider>(
			solutionDocument->solutionFile,
			this->globalBlock
		);

		auto solutionStructureProviderInterface = 
			this->solutionStructureProvider.As<Model::ISolutionStructureProvider>();

		auto parsedProjectBlocks = Parsers::ProjectBlocksParser{ solutionStructureProviderInterface }.Parse(*solutionDocument);

		for (auto& parsedProjectBlock : parsedProjectBlocks) {
			this->mapGuidToProjectBlock[parsedProjectBlock->solutionNode->guid] = parsedProjectBlock;
		}

		this->UpdateView();
	}


	//
	// ISerializable
	//
	std::string SolutionStructure::Serialize() const {
		std::string out;
		out += "Microsoft Visual Studio Solution File, Format Version 12.00\n";
		out += "# Visual Studio Version 17\n";
		out += "VisualStudioVersion = 17.11.35312.102\n";
		out += "MinimumVisualStudioVersion = 10.0.40219.1\n";

		auto rootSolutionNodes =
			this->view->mapGuidToSolutionNode
			| std::ranges::views::values
			| std::ranges::views::filter(
				[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
					return solutionNode->GetParent() == nullptr;
				})
			| std::ex::ranges::views::to<std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>>();


		std::sort(
			rootSolutionNodes.begin(),
			rootSolutionNodes.end(),
			&details::CompareSolutionNodes
		);

		for (const auto& rootSolutionNode : rootSolutionNodes) {
			details::SerializeSolutionNodeRecursively(rootSolutionNode, out);
		}

		out += this->view->globalBlock->Serialize();
		return out;
	}


	//
	// API
	//
	std::ex::shared_ptr<SolutionStructure::View> SolutionStructure::GetView() const {
		return this->view;
	}


	void SolutionStructure::Save() const {
		this->Save(this->solutionStructureProvider->GetSolutionInfo().solutionFile);
	}


	void SolutionStructure::Save(const std::filesystem::path& savePath) const {
		std::optional<std::ex::UTFByteOrderMark> bomType;

		if (std::filesystem::exists(savePath)) {
			std::ex::ifstream inFileStream(savePath, std::ios::binary);
			H::Stream::ThrowIfStreamNotOpened(inFileStream);

			bomType = inFileStream.DetectUTFByteOrderMark();
		}

		std::ex::ofstream outFileStream(savePath, std::ios::binary);
		H::Stream::ThrowIfStreamFailed(outFileStream);

		if (bomType) {
			outFileStream.WriteUTFByteOrderMark(*bomType);
		}

		outFileStream << this->Serialize();

		//for (const auto& line : this->SerializeToSln()) {
		//	outFileStream << line << "\n";
		//}
	}

	void SolutionStructure::LogSerializedSolution() const {
		LOG_DEBUG_D("{}", this->Serialize());
	}


	void SolutionStructure::AddProjectBlock(
		const std::ex::shared_ptr<Model::Project::ParsedProjectBlock>& srcProjectBlock
	) {
		this->AddProjectBlockRecursive(srcProjectBlock);
		this->UpdateView();
	}


	void SolutionStructure::RemoveProjectBlock(const H::Guid& projectGuid) {
		this->RemoveProjectBlockRecursive(projectGuid);
		this->UpdateView();
	}


	void SolutionStructure::AttachChild(const H::Guid& parentGuid, const H::Guid& childGuid) {
		if (!this->mapGuidToProjectBlock.contains(parentGuid) ||
			!this->mapGuidToProjectBlock.contains(childGuid)) {
			return;
		}

		const auto& parentProjectBlock = this->mapGuidToProjectBlock.at(parentGuid);
		const auto& childProjectBlock = this->mapGuidToProjectBlock.at(childGuid);

		this->WriteNestedProjectsEntry(
			Model::Global::ParsedNestedProjectsSection::Entry{
				.childGuid = childProjectBlock->solutionNode->guid,
				.parentGuid = parentProjectBlock->solutionNode->guid
			}
		);

		this->UpdateView();
	}


	void SolutionStructure::DettachChild(const H::Guid& parentGuid, const H::Guid& childGuid) {
		if (!this->mapGuidToProjectBlock.contains(parentGuid) ||
			!this->mapGuidToProjectBlock.contains(childGuid)) {
			return;
		}

		const auto& parentProjectBlock = this->mapGuidToProjectBlock.at(parentGuid);
		const auto& childProjectBlock = this->mapGuidToProjectBlock.at(childGuid);

		// Удаляем конкретную строку (запись).
		this->DeleteNestedProjectsEntries(
			[&](const Model::Global::ParsedNestedProjectsSection::Entry& entry) {
				if (entry.childGuid == childProjectBlock->solutionNode->guid &&
					entry.parentGuid == parentProjectBlock->solutionNode->guid
					) {
					return true;
				}
				return false;
			}
		);

		this->UpdateView();
	}


	//
	// Internal logic
	//
	void SolutionStructure::AddProjectBlockRecursive(
		const std::ex::shared_ptr<Model::Project::ParsedProjectBlock>& srcProjectBlock
	) {
		// NestedProjects:
		// Восстанавливаем те же связи что и в исходном SolutionStructure откуда пришел srcProjectBlock.
		// Если у srcSolutionNode есть родитель, значит у него и есть связь в NestedProjects section.
		// (Как альтернатива можно было просто скопировать напрямую строки из srcGlobalBlock)
		if (auto srcParentNode = srcProjectBlock->solutionNode->GetParent()) {
			this->WriteNestedProjectsEntry(
				Model::Global::ParsedNestedProjectsSection::Entry{
					.childGuid = srcProjectBlock->solutionNode->guid,
					.parentGuid = srcParentNode->guid
				}
			);
		}

		if (srcProjectBlock->solutionNode.Is<Model::Project::ProjectNode>()) {
			auto srcProjectNode = srcProjectBlock->solutionNode.Cast<Model::Project::ProjectNode>();
			this->AddProjectNodeEntriesToGlobalBlock(srcProjectNode);
		}
		else if (srcProjectBlock->solutionNode.Is<Model::Project::SolutionFolder>()) {
			auto srcSolutionFolder = srcProjectBlock->solutionNode.Cast<Model::Project::SolutionFolder>();
			for (auto& srcChildSolutionNode : srcSolutionFolder->GetChildren()) {
				this->AddProjectBlockRecursive(srcChildSolutionNode->GetProjectBlock());
			}
		}

		// Клонируем srcProjectBlock, корректируем пути и передаем ссылку 
		// на собственный solutionStructureProvider.
		const auto srcSolutionInfo = srcProjectBlock->solutionNode->GetSolutionInfo();
		const auto srcSolutionDir = srcSolutionInfo.solutionFile.parent_path();

		auto clonedProjectBlock = std::ex::make_shared_ex<Model::Project::ParsedProjectBlock>();

		clonedProjectBlock->solutionNode = srcProjectBlock->solutionNode->Clone(
			this->RebuildPathRelativeToCurrentSolution(srcSolutionDir / srcProjectBlock->solutionNode->relativePath),
			this->solutionStructureProvider.As<Model::ISolutionStructureProvider>()
		);

		for (auto& [key, srcParsedSection] : srcProjectBlock->sectionMap) {
			clonedProjectBlock->sectionMap[key] = srcParsedSection->Clone();
		}

		this->mapGuidToProjectBlock[clonedProjectBlock->solutionNode->guid] = clonedProjectBlock;
	}


	void SolutionStructure::RemoveProjectBlockRecursive(const H::Guid& projectGuid) {
		if (!this->mapGuidToProjectBlock.contains(projectGuid)) {
			return;
		}

		const auto& projectBlock = this->mapGuidToProjectBlock.at(projectGuid);

		// Удаляем любые строки где встречатеся projectGuid.
		this->DeleteNestedProjectsEntries(
			[&](const Model::Global::ParsedNestedProjectsSection::Entry& entry) {
				if (entry.childGuid == projectGuid ||
					entry.parentGuid == projectGuid
					) {
					return true;
				}
				return false;
			}
		);

		if (projectBlock->solutionNode.Is<Model::Project::ProjectNode>()) {
			auto projectNode = projectBlock->solutionNode.Cast<Model::Project::ProjectNode>();
			this->RemoveProjectNodeEntriesFromGlobalBlock(projectNode);
		}
		else if (projectBlock->solutionNode.Is<Model::Project::SolutionFolder>()) {
			auto solutionFolder = projectBlock->solutionNode.Cast<Model::Project::SolutionFolder>();
			for (auto& childSolutionNode : solutionFolder->GetChildren()) {
				this->RemoveProjectBlockRecursive(childSolutionNode->guid);
			}
		}

		this->mapGuidToProjectBlock.erase(projectGuid);
	}


	void SolutionStructure::AddProjectNodeEntriesToGlobalBlock(
		const std::ex::shared_ptr<Model::Project::ProjectNode>& srcProjectNode
	) {
		const auto srcSolutionInfo = srcProjectNode->GetSolutionInfo();
		const auto srcSolutionDir = srcSolutionInfo.solutionFile.parent_path();

		// ProjectConfigurationPlatforms
		{
			// Берём solution-configs напрямую из globalBlock, т.к. view еще не готоов.
			std::vector<Model::Entries::ConfigEntry> solutionConfigs;
			{
				auto itSlnCfg = this->globalBlock->sectionMap.find(
					Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName
				);
				if (itSlnCfg != this->globalBlock->sectionMap.end()) {
					auto parsedSlnCfg = itSlnCfg->second
						.Cast<Model::Global::ParsedSolutionConfigurationPlatformsSection>();

					solutionConfigs = parsedSlnCfg->entries;
				}
			}

			for (const auto& slnCfg : solutionConfigs) {
				// ActiveCfg
				{
					Model::Entries::ConfigEntry configEntry{
						.declaredConfguration = slnCfg.declaredConfguration,
						.assignedConfguration = slnCfg.assignedConfguration,
						.declaredPlatform = slnCfg.declaredPlatform,
						.assignedPlatform = slnCfg.assignedPlatform,
						.action = std::string{ "ActiveCfg" },
						.index = std::nullopt
					};
					this->WriteProjectConfigurationPlatformsEntry(
						Model::Global::ParsedProjectConfigurationPlatformsSection::Entry{
							.guid = srcProjectNode->guid,
							.configEntry = configEntry
						}
					);
				}
				// Build.0
				{
					Model::Entries::ConfigEntry configEntry{
						.declaredConfguration = slnCfg.declaredConfguration,
						.assignedConfguration = slnCfg.assignedConfguration,
						.declaredPlatform = slnCfg.declaredPlatform,
						.assignedPlatform = slnCfg.assignedPlatform,
						.action = std::string{ "Build" },
						.index = std::optional<int>{ 0 }
					};
					this->WriteProjectConfigurationPlatformsEntry(
						Model::Global::ParsedProjectConfigurationPlatformsSection::Entry{
							.guid = srcProjectNode->guid,
							.configEntry = configEntry
						}
					);
				}
			}
		}

		// SharedMSBuildProjectFiles
		{
			auto srcSharedMsBuildProjectFilesEntriesCopy = srcProjectNode->GetSharedMsBuildProjectFiles();

			auto srcSharedProjectEntries = H::Collection::Extract(
				srcSharedMsBuildProjectFilesEntriesCopy,
				[&](const Model::Entries::SharedMsBuildProjectFileEntry& entry) {
					return entry.guid == srcProjectNode->guid;
				}
			);

			if (!srcSharedProjectEntries.empty()) {
				for (const auto& srcEntry : srcSharedProjectEntries) {
					this->WriteSharedMSBuildProjectFilesEntry(
						Model::Entries::SharedMsBuildProjectFileEntry{
							this->RebuildPathRelativeToCurrentSolution(srcSolutionDir / srcEntry.relativePath),
							srcEntry.guid,
							srcEntry.key,
							srcEntry.value
						}
					);
				}
			}
		}
	}


	void SolutionStructure::RemoveProjectNodeEntriesFromGlobalBlock(
		const std::ex::shared_ptr<Model::Project::ProjectNode>& projectNode
	) {
		this->DeleteProjectConfigurationPlatformsEntries(
			[&](const Model::Global::ParsedProjectConfigurationPlatformsSection::Entry& entry) {
				return entry.guid == projectNode->guid;
			}
		);
		this->DeleteSharedMSBuildProjectFilesEntries(
			[&](const Model::Entries::SharedMsBuildProjectFileEntry& entry) {
				return entry.guid == projectNode->guid;
			}
		);
	}


	void SolutionStructure::WriteNestedProjectsEntry(
		Model::Global::ParsedNestedProjectsSection::Entry entry
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedNestedProjectsSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedNestedProjectsSection = itParsedSection->second
				.Cast<Model::Global::ParsedNestedProjectsSection>();

			parsedNestedProjectsSection->entries.push_back(entry);
		}
	}


	void SolutionStructure::WriteSharedMSBuildProjectFilesEntry(
		Model::Entries::SharedMsBuildProjectFileEntry entry
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedSharedMSBuildProjectFilesSection = itParsedSection->second
				.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();

			parsedSharedMSBuildProjectFilesSection->entries.push_back(entry);
		}
	}


	void SolutionStructure::WriteProjectConfigurationPlatformsEntry
	(Model::Global::ParsedProjectConfigurationPlatformsSection::Entry entry
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedProjectConfigurationPlatformsSection = itParsedSection->second
				.Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();

			parsedProjectConfigurationPlatformsSection->entries.push_back(entry);
		}
	}


	void SolutionStructure::DeleteNestedProjectsEntries(
		std::function<bool(const Model::Global::ParsedNestedProjectsSection::Entry&)> predicate
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedNestedProjectsSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedNestedProjectsSection = itParsedSection->second
				.Cast<Model::Global::ParsedNestedProjectsSection>();

			std::erase_if(parsedNestedProjectsSection->entries, predicate);
		}
	}


	void SolutionStructure::DeleteSharedMSBuildProjectFilesEntries(
		std::function<bool(const Model::Entries::SharedMsBuildProjectFileEntry&)> predicate
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedSharedMSBuildProjectFilesSection = itParsedSection->second
				.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();

			std::erase_if(parsedSharedMSBuildProjectFilesSection->entries, predicate);
		}
	}


	void SolutionStructure::DeleteProjectConfigurationPlatformsEntries(
		std::function<bool(const Model::Global::ParsedProjectConfigurationPlatformsSection::Entry&)> predicate
	) {
		auto itParsedSection = this->globalBlock->sectionMap.find(
			Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName
		);
		if (itParsedSection != this->globalBlock->sectionMap.end()) {
			auto parsedProjectConfigurationPlatformsSection = itParsedSection->second
				.Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();

			std::erase_if(parsedProjectConfigurationPlatformsSection->entries, predicate);
		}
	}


	// Преобразует абсолютный путь к файлу в путь, относительный к текущему .sln.
	std::filesystem::path SolutionStructure::RebuildPathRelativeToCurrentSolution(
		const std::filesystem::path& absolutePath
	) {
		LOG_ASSERT(absolutePath.is_absolute());
		std::error_code ec{};

		// Канонизируем абсолют через std::filesystem::canonical(..., ec).
		// canonical требует существования полного пути; при ошибке берём lexically_normal().
		auto canonicalAbs = std::filesystem::canonical(absolutePath, ec);
		auto baseAbs = ec
			? absolutePath.lexically_normal()
			: canonicalAbs;

		ec.clear();

		const auto solutionInfo = this->solutionStructureProvider->GetSolutionInfo();
		const auto currentSolutionDir = solutionInfo.solutionFile.parent_path();

		// Пытаемся вычислить относительный путь к директории текущего решения.
		// Если relative(...) не удаётся (другой диск и т.п.), возвращаем канонизированный абсолют.
		auto relativeToCurrentSln = std::filesystem::relative(baseAbs, currentSolutionDir, ec);
		if (ec) {
			LOG_WARNING("Cannot construct path ('relativeToCurrentSln'), use absolute.");
			return baseAbs.lexically_normal();
		}
		else {
			return relativeToCurrentSln.lexically_normal();
		}
	}


	void SolutionStructure::UpdateView() {
		auto viewGlobalBlock = this->globalBlock;
		auto viewMapGuidToProjectBlock = this->mapGuidToProjectBlock;
		auto viewMapGuidToSolutionNode = std::unordered_map<H::Guid, std::ex::shared_ptr<Model::Project::SolutionNode>>{};
		auto viewSolutionConfigurations = std::vector<Model::Entries::ConfigEntry>{};

		// mapGuidToSolutionNode
		{
			using Pair_t = std::pair<
				H::Guid,
				std::ex::shared_ptr<Model::Project::SolutionNode>
			>;
			viewMapGuidToSolutionNode =
				this->mapGuidToProjectBlock
				| std::ranges::views::transform(
					[](const std::pair<H::Guid, std::ex::shared_ptr<Model::Project::ParsedProjectBlock>>& kvp) {
						return Pair_t{ kvp.first, kvp.second->solutionNode };
					})
				| std::ex::ranges::views::to<std::unordered_map<typename Pair_t::first_type, typename Pair_t::second_type>>();
		}

		// solutionConfigurations
		{
			if (this->globalBlock) {
				auto itParsedSection = this->globalBlock->sectionMap.find(
					Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName
				);
				if (itParsedSection != this->globalBlock->sectionMap.end()) {
					auto parsedSection = itParsedSection->second
						.Cast<Model::Global::ParsedSolutionConfigurationPlatformsSection>();

					viewSolutionConfigurations = parsedSection->entries;
				}
			}
		}

		this->view = std::ex::make_shared_ex<View>(View{
			.mapGuidToProjectBlock = viewMapGuidToProjectBlock,
			.mapGuidToSolutionNode = viewMapGuidToSolutionNode,
			.globalBlock = viewGlobalBlock,
			.solutionConfigurations = viewSolutionConfigurations
			});

		this->solutionStructureProvider->RebuildFromView(this->view);
	}
}