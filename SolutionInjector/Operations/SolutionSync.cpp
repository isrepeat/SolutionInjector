#include "../Dependencies/ProjectDependencyAnalyzer.h"
#include "../Helpers/TextHelpers.h"
#include "../Model/RawSections.h"
#include "../IO/FileSnapshot.h"
#include "../DefaultSolutionStructureProvider.h"
#include "../SolutionStructure.h"

#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>

namespace Core {
	namespace details {
		struct ManagedProject {
			std::string owner;
			H::Guid guid;
			std::filesystem::path path;
		};

		using ManagedProjects = std::map<H::Guid, ManagedProject>;

		ManagedProjects ReadManagedProjects(const SolutionStructure& solutionStructure) {
			// Читаем сохранённого владельца, тип и путь каждого управляемого проекта.
			ManagedProjects managedProjects;
			const auto& sections = solutionStructure.GetView()->globalBlock->sectionMap;
			const auto found = sections.find("SolutionInjectorProjectOwnership");
			if (found == sections.end()) {
				return managedProjects;
			}
			const auto section = found->second.As<Model::RawGlobalSection>();
			if (!section || section->lines.size() < 2) {
				throw std::runtime_error("Invalid managed-project registry");
			}
			for (size_t index = 1; index + 1 < section->lines.size(); ++index) {
				const auto line = Helpers::Trim(section->lines[index]);
				const auto equal = line.find('=');
				const auto first = line.find('|', equal);
				const auto second = line.find('|', first == std::string::npos ? first : first + 1);
				if (equal == std::string::npos || first == std::string::npos || second == std::string::npos) {
					throw std::runtime_error("Invalid managed-project registry entry");
				}
				const auto guid = H::Guid::Parse(Helpers::Trim(line.substr(0, equal)));
				ManagedProject managedProject{
					Helpers::Trim(line.substr(equal + 1, first - equal - 1)),
					H::Guid::Parse(line.substr(first + 1, second - first - 1)),
					Helpers::Utf8ToWide(line.substr(second + 1))
				};
				if (managedProject.owner.empty() || managedProject.path.empty() || !managedProjects.emplace(guid, managedProject).second) {
					throw std::runtime_error("Duplicate or empty managed-project registry entry");
				}
			}
			return managedProjects;
		}

		void ValidateTree(const SolutionStructure& solutionStructure) {
			// Проверяем существование родителей, единственность вложенности и отсутствие циклов.
			const auto view = solutionStructure.GetView();
			std::set<H::Guid> nestedChildren;
			const auto nested = view->globalBlock->sectionMap.at("NestedProjects").Cast<Model::Global::ParsedNestedProjectsSection>();
			for (const auto& entry : nested->entries) {
				if (!view->mapGuidToSolutionNode.contains(entry.childGuid) || !view->mapGuidToSolutionNode.contains(entry.parentGuid) ||
					!view->mapGuidToSolutionNode.at(entry.parentGuid).Is<Model::Project::SolutionFolder>() || !nestedChildren.insert(entry.childGuid).second) {
					throw std::runtime_error("Invalid or duplicate NestedProjects entry");
				}
			}
			for (const auto& [guid, solutionNode] : solutionStructure.GetView()->mapGuidToSolutionNode) {
				std::set<H::Guid> visited;
				auto current = solutionNode;
				while (current) {
					if (!visited.insert(current->guid).second) {
						throw std::runtime_error("Cycle in NestedProjects");
					}
					current = current->GetParent();
				}
			}
		}
	} // namespace details

	// Снимки представлений сохраняют исходные блоки до завершения переноса пользовательских секций.
	struct SolutionStructure::SyncContext {
		details::ManagedProjects managedProjects;
		std::ex::shared_ptr<View> sourceView;
		std::ex::shared_ptr<View> targetView;
		std::filesystem::path sourceDirectory;
		std::filesystem::path targetDirectory;
		std::string owner;
		std::set<H::Guid> adoptedProjects;
		std::set<H::Guid> previous;
		std::set<H::Guid> desired;
		std::set<H::Guid> newFolders;

		bool IsManaged(const H::Guid& guid) const {
			return this->previous.contains(guid) || this->desired.contains(guid);
		}

		bool IsRemoved(const H::Guid& guid) const {
			return this->previous.contains(guid) && !this->desired.contains(guid);
		}
	};

	void SolutionStructure::SynchronizeFrom(
		const SolutionStructure& solutionStructure,
		const std::string& owner,
		const std::vector<H::Guid>& adoptedProjects,
		IO::FileSnapshot& fileSnapshot
	) {
		// 1. Проверяем владение, зависимости и метаданные до изменения модели решения.
		auto syncContext = this->PrepareSyncContext(solutionStructure, owner, adoptedProjects);
		this->ValidateSyncProjects(syncContext);
		this->ValidateSyncDependencies(syncContext, fileSnapshot);
		this->ValidateSyncMetadata(syncContext);

		// 2. Обновляем блоки проектов и связанные с ними секции выбранного владельца.
		this->ReplaceSyncProjectBlocks(syncContext);
		this->SynchronizeProjectConfigurations(syncContext);
		this->SynchronizeProjectNesting(syncContext);
		this->SynchronizeSharedReferences(syncContext);

		// 3. Сохраняем владение и перестраиваем представление для проверки итогового дерева.
		this->WriteSyncOwnership(syncContext);
		this->UpdateView();
		details::ValidateTree(*this);
	}

	SolutionStructure::SyncContext SolutionStructure::PrepareSyncContext(
		const SolutionStructure& solutionStructure,
		const std::string& owner,
		const std::vector<H::Guid>& adoptedProjects
	) const {
		// Проверяем владельца и оба дерева, затем фиксируем исходные данные синхронизации.
		if (owner.empty() || owner.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != std::string::npos) {
			throw std::runtime_error("Invalid sync owner");
		}
		details::ValidateTree(*this);
		details::ValidateTree(solutionStructure);

		SyncContext syncContext;
		syncContext.managedProjects = details::ReadManagedProjects(*this);
		syncContext.sourceView = solutionStructure.GetView();
		syncContext.targetView = this->GetView();
		syncContext.sourceDirectory = solutionStructure.GetSolutionInfo().solutionFile.parent_path();
		syncContext.targetDirectory = this->GetSolutionInfo().solutionFile.parent_path();
		syncContext.owner = owner;
		syncContext.adoptedProjects.insert(adoptedProjects.begin(), adoptedProjects.end());
		return syncContext;
	}

	void SolutionStructure::ValidateSyncProjects(SyncContext& syncContext) const {
		// Проверяем сохранённые идентичности, конфликты путей и явное принятие существующих проектов.
		std::set<std::string> desiredPaths;
		// 1. Сверяем прежние записи владельца с текущими типами и путями проектов.
		for (const auto& [guid, managedProject] : syncContext.managedProjects) {
			if (managedProject.owner != syncContext.owner) {
				continue;
			}
			syncContext.previous.insert(guid);
			const auto found = syncContext.targetView->mapGuidToSolutionNode.find(guid);
			if (found != syncContext.targetView->mapGuidToSolutionNode.end()) {
				const auto& node = found->second;
				if (!node.Is<Model::Project::ProjectNode>() || node->typeGuid != managedProject.guid ||
					IO::FileSnapshot::PathKey(syncContext.targetDirectory / node->relativePath) != IO::FileSnapshot::PathKey(syncContext.targetDirectory / managedProject.path)) {
					throw std::runtime_error("Managed project identity changed outside sync: " + guid.ToString());
				}
			}
		}

		// 2. Собираем желаемые проекты и новые папки, проверяя конфликты и право принятия под управление.
		for (const auto& [guid, node] : syncContext.sourceView->mapGuidToSolutionNode) {
			const auto found = syncContext.targetView->mapGuidToSolutionNode.find(guid);
			if (node.Is<Model::Project::SolutionFolder>()) {
				if (found == syncContext.targetView->mapGuidToSolutionNode.end()) {
					syncContext.newFolders.insert(guid);
				} else if (!found->second.Is<Model::Project::SolutionFolder>() || found->second->name != node->name) {
					throw std::runtime_error("Solution folder GUID collision");
				}
				continue;
			}
			syncContext.desired.insert(guid);
			const auto key = IO::FileSnapshot::PathKey(syncContext.sourceDirectory / node->relativePath);
			if (!desiredPaths.insert(key).second) {
				throw std::runtime_error("Duplicate desired project path");
			}
			const auto managed = syncContext.managedProjects.find(guid);
			if (managed != syncContext.managedProjects.end() && managed->second.owner != syncContext.owner) {
				throw std::runtime_error("Project belongs to another sync owner");
			}
			if (found != syncContext.targetView->mapGuidToSolutionNode.end() && !syncContext.previous.contains(guid)) {
				if (!syncContext.adoptedProjects.contains(guid) || !found->second.Is<Model::Project::ProjectNode>() || found->second->typeGuid != node->typeGuid ||
					IO::FileSnapshot::PathKey(syncContext.targetDirectory / found->second->relativePath) != key) {
					throw std::runtime_error("Existing project requires explicit adoption with matching GUID, type and path: " + guid.ToString());
				}
			}
			for (const auto& [targetGuid, targetNode] : syncContext.targetView->mapGuidToSolutionNode) {
				if (targetGuid != guid && targetNode.Is<Model::Project::ProjectNode>() &&
					IO::FileSnapshot::PathKey(syncContext.targetDirectory / targetNode->relativePath) == key) {
					throw std::runtime_error("Project path belongs to another GUID");
				}
			}
		}

		// 3. Явное принятие допустимо только для проектов, присутствующих в желаемом составе.
		for (const auto& guid : syncContext.adoptedProjects) {
			if (!syncContext.desired.contains(guid)) {
				throw std::runtime_error("Adoption GUID is absent from desired projects");
			}
		}
	}

	void SolutionStructure::ValidateSyncDependencies(const SyncContext& syncContext, IO::FileSnapshot& fileSnapshot) const {
		// Блокируем удаление и перенос проектов, если посторонние потребители используют их прежние пути.
		// 1. Выделяем удаляемые проекты и проекты со сменившимся путём.
		std::set<H::Guid> retiredPaths;
		for (const auto& guid : syncContext.previous) {
			if (syncContext.IsRemoved(guid)) {
				retiredPaths.insert(guid);
			} else if (
				syncContext.targetView->mapGuidToSolutionNode.contains(guid) &&
				IO::FileSnapshot::PathKey(syncContext.targetDirectory / syncContext.targetView->mapGuidToSolutionNode.at(guid)->relativePath) !=
					IO::FileSnapshot::PathKey(syncContext.sourceDirectory / syncContext.sourceView->mapGuidToSolutionNode.at(guid)->relativePath)
			) {
				retiredPaths.insert(guid);
			}
		}

		// 2. Проверяем транзитивные XML-зависимости всех посторонних потребителей.
		if (!retiredPaths.empty()) {
			const auto dependencyGraph = Dependencies::ProjectDependencyAnalyzer::Analyze(*this, fileSnapshot);
			if (!dependencyGraph.issues.empty()) {
				throw std::runtime_error("Cannot safely remove projects: " + *dependencyGraph.issues.begin());
			}
			std::set<std::string> roots;
			for (const auto& [guid, node] : syncContext.targetView->mapGuidToSolutionNode) {
				if (!syncContext.IsManaged(guid) && node.Is<Model::Project::ProjectNode>()) {
					roots.insert(guid.ToString());
				}
			}
			const auto reachable = dependencyGraph.Reachable(roots);
			for (const auto& guid : syncContext.previous) {
				if (retiredPaths.contains(guid) && reachable.contains(guid.ToString())) {
					throw std::runtime_error("Unmanaged consumer depends on a removed or relocated project");
				}
			}
		}

		// 3. Проверяем явные ProjectDependencies в сохранённых блоках посторонних проектов.
		for (const auto& [guid, block] : syncContext.targetView->mapGuidToProjectBlock) {
			if (syncContext.IsManaged(guid)) {
				continue;
			}
			const auto dependencySection = block->sectionMap.find("ProjectDependencies");
			if (dependencySection != block->sectionMap.end()) {
				const auto text = dependencySection->second->Serialize(std::nullopt);
				for (const auto& removed : syncContext.previous) {
					if (syncContext.IsRemoved(removed) && Helpers::ToUpperAscii(text).find(removed.ToString()) != std::string::npos) {
						throw std::runtime_error("Unmanaged project depends on a removed project");
					}
				}
			}
		}

		// 4. Проверяем, что удаляемые Shared-проекты не используются чужими Shared-ссылками.
		const auto shared = this->globalBlock->sectionMap.at("SharedMSBuildProjectFiles").Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();
		for (const auto& entry : shared->entries) {
			if (syncContext.IsManaged(entry.guid)) {
				continue;
			}
			for (const auto& removed : syncContext.previous) {
				if (!syncContext.IsRemoved(removed) || !syncContext.targetView->mapGuidToSolutionNode.contains(removed)) {
					continue;
				}
				const auto& node = syncContext.targetView->mapGuidToSolutionNode.at(removed);
				if (node->relativePath.extension() == ".vcxitems" &&
					IO::FileSnapshot::PathKey(syncContext.targetDirectory / entry.relativePath) == IO::FileSnapshot::PathKey(syncContext.targetDirectory / node->relativePath)) {
					throw std::runtime_error("Unmanaged consumer uses a removed Shared project");
				}
			}
		}
	}

	void SolutionStructure::ValidateSyncMetadata(const SyncContext& syncContext) const {
		// Проверяем уникальность конфигураций, mappings и Shared-ссылок желаемого решения.
		const auto sourceMappings = syncContext.sourceView->globalBlock->sectionMap.at("ProjectConfigurationPlatforms")
			.Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();
		const auto sourceShared = syncContext.sourceView->globalBlock->sectionMap.at("SharedMSBuildProjectFiles")
			.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();
		std::set<std::string> configurationKeys;
		for (const auto& entry : syncContext.sourceView->solutionConfigurations) {
			if (!configurationKeys.insert(entry.declaredConfguration + "|" + entry.declaredPlatform).second) {
				throw std::runtime_error("Duplicate solution configuration");
			}
		}
		std::set<std::string> mappingKeys;
		for (const auto& entry : sourceMappings->entries) {
			const auto config = entry.configEntry.declaredConfguration + "|" + entry.configEntry.declaredPlatform;
			const auto key = entry.guid.ToString() + ":" + config + ":" + entry.configEntry.action + ":" + std::to_string(entry.configEntry.index.value_or(-1));
			if (!syncContext.desired.contains(entry.guid) || !configurationKeys.contains(config) || !mappingKeys.insert(key).second) {
				throw std::runtime_error("Invalid or duplicate project configuration mapping");
			}
		}
		std::set<std::string> sharedKeys;
		for (const auto& entry : sourceShared->entries) {
			const auto key = entry.guid.ToString() + ":" + IO::FileSnapshot::PathKey(syncContext.sourceDirectory / entry.relativePath) + ":" + entry.key;
			if (!syncContext.desired.contains(entry.guid) || entry.key != "SharedItemsImports" || !sharedKeys.insert(key).second) {
				throw std::runtime_error("Invalid or duplicate Shared project mapping");
			}
		}
	}

	void SolutionStructure::ReplaceSyncProjectBlocks(const SyncContext& syncContext) {
		// Заменяем управляемые проекты, сохраняя их неизвестные секции и существующие посторонние папки.
		// 1. Убираем прежние блоки владельца; исходные блоки остаются доступны через снимок targetView.
		for (const auto& guid : syncContext.previous) {
			this->mapGuidToProjectBlock.erase(guid);
		}

		// 2. Клонируем желаемые блоки и переносим неизвестные секции из исходных проектов.
		for (const auto& [guid, block] : syncContext.sourceView->mapGuidToProjectBlock) {
			const auto folder = block->solutionNode.Is<Model::Project::SolutionFolder>();
			if (folder && !syncContext.newFolders.contains(guid)) {
				continue;
			}
			auto clone = std::ex::make_shared_ex<Model::Project::ParsedProjectBlock>();
			const auto path =
				folder ? block->solutionNode->relativePath : this->RebuildPathRelativeToCurrentSolution(syncContext.sourceDirectory / block->solutionNode->relativePath);
			clone->solutionNode = block->solutionNode->Clone(path, this->solutionStructureProvider.As<Model::ISolutionStructureProvider>());
			if (!folder && syncContext.targetView->mapGuidToProjectBlock.contains(guid)) {
				for (const auto& [key, section] : syncContext.targetView->mapGuidToProjectBlock.at(guid)->sectionMap) {
					if (key != "ProjectDependencies") {
						clone->sectionMap[key] = section->Clone();
					}
				}
			}
			for (const auto& [key, section] : block->sectionMap) {
				clone->sectionMap[key] = section->Clone();
			}
			this->mapGuidToProjectBlock[guid] = clone;
		}
	}

	void SolutionStructure::SynchronizeProjectConfigurations(const SyncContext& syncContext) {
		// Добавляем конфигурации решения и полностью заменяем mappings только управляемых проектов.
		// 1. Дополняем общий список, не удаляя конфигурации, нужные посторонним проектам.
		auto& targetSections = this->globalBlock->sectionMap;
		auto targetConfigurations = targetSections.at("SolutionConfigurationPlatforms").Cast<Model::Global::ParsedSolutionConfigurationPlatformsSection>();
		const auto sourceConfigurations =
			syncContext.sourceView->globalBlock->sectionMap.at("SolutionConfigurationPlatforms").Cast<Model::Global::ParsedSolutionConfigurationPlatformsSection>();
		for (const auto& entry : sourceConfigurations->entries) {
			const auto isMissing = std::none_of(
				targetConfigurations->entries.begin(),
				targetConfigurations->entries.end(),
				[&](const auto& existing) {
					return existing.declaredConfguration == entry.declaredConfguration && existing.declaredPlatform == entry.declaredPlatform;
				}
			);
			if (isMissing) {
				targetConfigurations->entries.push_back(entry);
			}
		}

		// 2. Заменяем соответствия конфигураций владельца на точный набор из желаемого решения.
		auto mappings = targetSections.at("ProjectConfigurationPlatforms").Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();
		std::erase_if(
			mappings->entries,
			[&](const auto& entry) {
				return syncContext.IsManaged(entry.guid);
			}
		);
		const auto sourceMappings = syncContext.sourceView->globalBlock->sectionMap.at("ProjectConfigurationPlatforms")
			.Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();
		for (const auto& entry : sourceMappings->entries) {
			if (!syncContext.desired.contains(entry.guid)) {
				throw std::runtime_error("Mapping references a non-project GUID");
			}
			mappings->entries.push_back(entry);
		}
	}

	void SolutionStructure::SynchronizeProjectNesting(const SyncContext& syncContext) {
		// Обновляем родителей управляемых проектов и новых папок, сохраняя посторонние связи.
		auto& targetSections = this->globalBlock->sectionMap;
		auto nested = targetSections.at("NestedProjects").Cast<Model::Global::ParsedNestedProjectsSection>();
		const auto sourceNested = syncContext.sourceView->globalBlock->sectionMap.at("NestedProjects")
			.Cast<Model::Global::ParsedNestedProjectsSection>();
		std::erase_if(
			nested->entries,
			[&](const auto& entry) {
				return syncContext.IsManaged(entry.childGuid) || syncContext.newFolders.contains(entry.childGuid);
			}
		);
		for (const auto& entry : sourceNested->entries) {
			if (syncContext.desired.contains(entry.childGuid) || syncContext.newFolders.contains(entry.childGuid)) {
				nested->entries.push_back(entry);
			}
		}
	}

	void SolutionStructure::SynchronizeSharedReferences(const SyncContext& syncContext) {
		// Заменяем Shared-ссылки управляемых проектов и пересчитываем пути относительно текущего решения.
		auto shared = this->globalBlock->sectionMap.at("SharedMSBuildProjectFiles")
			.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();
		const auto sourceShared = syncContext.sourceView->globalBlock->sectionMap.at("SharedMSBuildProjectFiles")
			.Cast<Model::Global::ParsedSharedMSBuildProjectFilesSection>();
		std::erase_if(
			shared->entries,
			[&](const auto& entry) {
				return syncContext.IsManaged(entry.guid);
			}
		);
		for (auto entry : sourceShared->entries) {
			if (!syncContext.desired.contains(entry.guid)) {
				throw std::runtime_error("Shared entry references a non-project GUID");
			}
			entry.relativePath = this->RebuildPathRelativeToCurrentSolution(syncContext.sourceDirectory / entry.relativePath);
			shared->entries.push_back(entry);
		}
	}

	void SolutionStructure::WriteSyncOwnership(SyncContext& syncContext) {
		// Обновляем записи текущего владельца внутри решения; сведения остальных владельцев сохраняем.
		std::erase_if(
			syncContext.managedProjects,
			[&](const auto& entry) {
				return entry.second.owner == syncContext.owner;
			}
		);
		for (const auto& guid : syncContext.desired) {
			const auto& node = this->mapGuidToProjectBlock.at(guid)->solutionNode;
			syncContext.managedProjects.emplace(guid, details::ManagedProject{ syncContext.owner, node->typeGuid, node->relativePath });
		}
		Model::Raw::Section raw;
		raw.name = "SolutionInjectorProjectOwnership";
		raw.role = "preSolution";
		auto registry = std::ex::make_shared_ex<Model::RawGlobalSection>(raw);
		registry->lines.emplace_back("\tGlobalSection(SolutionInjectorProjectOwnership) = preSolution");
		for (const auto& [guid, managedProject] : syncContext.managedProjects) {
			registry->lines.push_back(
				"\t\t" + guid.ToString() + " = " + managedProject.owner + "|" + managedProject.guid.ToString() + "|" +
				Helpers::WideToUtf8(managedProject.path.wstring())
			);
		}
		registry->lines.emplace_back("\tEndGlobalSection");
		this->globalBlock->sectionMap[raw.name] = registry;
	}

}