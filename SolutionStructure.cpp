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
	std::string SolutionStructure::Serialize(
		std::ex::optional_ref<const H::IServiceProvider> /*serviceProviderOpt*/
	) const {
		std::string out;

		// 1) Шапка .sln
		out += "\n";
		out += "Microsoft Visual Studio Solution File, Format Version 12.00\n";
		out += "# Visual Studio Version 17\n";
		out += "VisualStudioVersion = 17.11.35312.102\n";
		out += "MinimumVisualStudioVersion = 10.0.40219.1\n";

		// 2) Корневые узлы (без родителей)
		auto rootSolutionNodes =
			this->view->mapGuidToSolutionNode
			| std::ranges::views::values
			| std::ranges::views::filter(
				[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNodeSharedPtr) {
					return solutionNodeSharedPtr->GetParent() == nullptr;
				})
			| std::ex::ranges::views::to<std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>>();

		// 3) Отсортировать корни тем же компаратором
		std::sort(
			rootSolutionNodes.begin(),
			rootSolutionNodes.end(),
			&details::CompareSolutionNodes
		);

		// 4) Функция раскрытия детей папки — с сортировкой тем же компаратором
		const auto expandChildrenFn =
			[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNodeSharedPtr) {
			if (auto folderSharedPtr = solutionNodeSharedPtr.As<Model::Project::SolutionFolder>()) {
				auto childrenVector = folderSharedPtr->GetChildren();

				std::sort(
					childrenVector.begin(),
					childrenVector.end(),
					&details::CompareSolutionNodes
				);

				return childrenVector;
			}
			return std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>{};
			};

		// 5) Один проход: печатаем Project(...) для каждого узла и
		//    параллельно копим GUID’ы только для проектов.
		std::vector<H::Guid> orderedProjectGuidsVector;
		orderedProjectGuidsVector.reserve(this->view->mapGuidToSolutionNode.size());

		for (const auto& solutionNode : 
			rootSolutionNodes
			| std::ex::ranges::views::flatten_tree(expandChildrenFn)
			) {
			out += solutionNode->GetProjectBlock()->Serialize(std::nullopt);
			orderedProjectGuidsVector.push_back(solutionNode->guid);
		}

		// 6) Провайдер сервисов: кладём порядок проектов
		H::DefaultServiceProvider serviceProvider;
		serviceProvider.AddService<Model::Global::ProjectBlocksOrderInfo>(
			std::make_shared<Model::Global::ProjectBlocksOrderInfo>(orderedProjectGuidsVector)
		);

		// 7) Global-блок, который внутри отсортирует ProjectConfigurationPlatforms
		//    «по проектам в порядке Project(...)» (через твой Comparator)
		out += this->view->globalBlock->Serialize(serviceProvider);
		return out;
	}

	
	//
	// API
	//
	const Model::SolutionInfo SolutionStructure::GetSolutionInfo() const {
		return this->solutionStructureProvider->GetSolutionInfo();
	}

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

		outFileStream << this->Serialize(std::nullopt);
	}

	void SolutionStructure::LogSerializedSolution() const {
		LOG_DEBUG_D("{}", this->Serialize(std::nullopt));
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


	std::ex::shared_ptr<Model::Project::SolutionFolder> SolutionStructure::MakeFolder(std::string name) {
		auto it = this->view->mapNameToSolutionNode.find(name);
		if (it != this->view->mapNameToSolutionNode.end()) {
			if (auto existSolutionNode = it->second.As<Model::Project::SolutionFolder>()) {
				return existSolutionNode;
			}
		}

		auto solutionStructureProviderInterface =
			this->solutionStructureProvider.As<Model::ISolutionStructureProvider>();

		auto projectBlock = std::ex::make_shared_ex<Model::Project::ParsedProjectBlock>();

		auto solutionFolder = std::ex::make_shared_ex<Model::Project::SolutionFolder>(
			Model::Project::SolutionFolder::SolutionFolderTypeGuid,
			H::Guid::NewGuid(),
			name,
			name,
			solutionStructureProviderInterface
		);
		projectBlock->solutionNode = solutionFolder;

		this->mapGuidToProjectBlock[projectBlock->solutionNode->guid] = projectBlock;
		return solutionFolder;
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
		// Клонируем srcProjectBlock, корректируем пути и передаем ссылку 
		// на собственный solutionStructureProvider.
		const auto srcSolutionInfo = srcProjectBlock->solutionNode->GetSolutionInfo();
		const auto srcSolutionDir = srcSolutionInfo.solutionFile.parent_path();

		auto clonedProjectBlock = std::ex::make_shared_ex<Model::Project::ParsedProjectBlock>();

		clonedProjectBlock->solutionNode = srcProjectBlock->solutionNode->Clone(
			this->RebuildPathRelativeToCurrentSolution(srcSolutionDir / srcProjectBlock->solutionNode->relativePath),
			this->solutionStructureProvider.As<Model::ISolutionStructureProvider>()
		);

		for (auto& kvp : srcProjectBlock->sectionMap) {
			const auto& key = kvp.first;
			const auto& srcParsedSection = kvp.second;
			clonedProjectBlock->sectionMap[key] = srcParsedSection->Clone();
		}

		this->mapGuidToProjectBlock[clonedProjectBlock->solutionNode->guid] = clonedProjectBlock;

		// Если у исходного узла есть родитель и он уже есть 
		// в mapGuidToProjectBlock — пишем NestedProjects.
		if (auto srcParentNode = srcProjectBlock->solutionNode->GetParent()) {
			const auto childGuid = clonedProjectBlock->solutionNode->guid;
			const auto parentGuid = srcParentNode->guid;

			// Эта проверка нужна на первом стеке рекурсивного вызова, 
			// т.к. последующие рекурсивный вызовы (для детей) гарантируют что родитель существует.  
			const bool parentExistsInTarget = this->mapGuidToProjectBlock.contains(parentGuid);
			if (parentExistsInTarget) {
				this->WriteNestedProjectsEntry(
					Model::Global::ParsedNestedProjectsSection::Entry{
						.childGuid = childGuid,
						.parentGuid = parentGuid
					}
				);
			} // Иначе: «голая» вставка допустима. Привязку можно сделать позже через AttachChild(...).
		}

		// Обновляем глобальую секцию для ProjectNode + рекурсия по детям (для SolutionFolder).
		if (srcProjectBlock->solutionNode.Is<Model::Project::ProjectNode>()) {
			auto srcProjectNode = srcProjectBlock->solutionNode.Cast<Model::Project::ProjectNode>();
			this->AddProjectNodeEntriesToGlobalBlock(srcProjectNode);
		}
		else if (srcProjectBlock->solutionNode.Is<Model::Project::SolutionFolder>()) {
			// Родитель (srcProjectBlock) уже добавлен в mapGuidToProjectBlock,
			// значит в дальнейших рекурсивных вызовах у детей будет валидная привязка для NestedProjects.
			auto srcSolutionFolder = srcProjectBlock->solutionNode.Cast<Model::Project::SolutionFolder>();
			for (auto& srcChildSolutionNode : srcSolutionFolder->GetChildren()) {
				this->AddProjectBlockRecursive(srcChildSolutionNode->GetProjectBlock());
			}
		}
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

		const bool isCsproj = srcProjectNode->relativePath.extension().string() == ".csproj";
		const bool isShproj = srcProjectNode->relativePath.extension().string() == ".shproj";

		// ProjectConfigurationPlatforms
		{
			if (isShproj) {
				// Shared C# (.shproj): ничего не добавляем в ProjectConfigurationPlatforms
			}
			else {
				// Берём solution-configs напрямую из globalBlock, т.к. view еще не готоов.
				std::vector<Model::Entries::ConfigEntry> solutionConfigs;
				{
					auto itSlnCfg = this->globalBlock->sectionMap.find(
						std::string{ Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName }
					);
					if (itSlnCfg != this->globalBlock->sectionMap.end()) {
						auto parsedSlnCfg = itSlnCfg->second
							.Cast<Model::Global::ParsedSolutionConfigurationPlatformsSection>();

						solutionConfigs = parsedSlnCfg->entries;
					}
				}

				for (const auto& slnCfg : solutionConfigs) {
					// Для C# подменяем назначенную платформу на Any CPU
					const auto assignedPlatform = isCsproj
						? "Any CPU"
						: slnCfg.assignedPlatform;

					// ActiveCfg
					{
						Model::Entries::ConfigEntry configEntry{
							.declaredConfguration = slnCfg.declaredConfguration,
							.assignedConfguration = slnCfg.assignedConfguration,
							.declaredPlatform = slnCfg.declaredPlatform,
							.assignedPlatform = assignedPlatform,
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
							.assignedPlatform = assignedPlatform,
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
			std::string{ Model::Global::ParsedNestedProjectsSection::SectionName }
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
			std::string{ Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName }
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
			std::string{ Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName }
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
			std::string{ Model::Global::ParsedNestedProjectsSection::SectionName }
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
			std::string{ Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName }
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
			std::string{ Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName }
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
		auto viewMapNameToSolutionNode = std::unordered_map<std::string, std::ex::shared_ptr<Model::Project::SolutionNode>>{};
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

		// mapNameToSolutionNode
		{
			using Pair_t = std::pair<
				std::string,
				std::ex::shared_ptr<Model::Project::SolutionNode>
			>;
			viewMapNameToSolutionNode =
				this->mapGuidToProjectBlock
				| std::ranges::views::transform(
					[](const std::pair<H::Guid, std::ex::shared_ptr<Model::Project::ParsedProjectBlock>>& kvp) {
						return Pair_t{ kvp.second->solutionNode->name, kvp.second->solutionNode };
					})
				| std::ex::ranges::views::to<std::unordered_map<typename Pair_t::first_type, typename Pair_t::second_type>>();
		}

		// solutionConfigurations
		{
			if (this->globalBlock) {
				auto itParsedSection = this->globalBlock->sectionMap.find(
					std::string{ Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName }
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
			.mapNameToSolutionNode = viewMapNameToSolutionNode,
			.globalBlock = viewGlobalBlock,
			.solutionConfigurations = viewSolutionConfigurations
			});

		this->solutionStructureProvider->RebuildFromView(this->view);
	}
}