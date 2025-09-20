#include "GlobalModel.h"
#include <Helpers/Std/FormatSpecializations.h>

#include <array>

namespace Core {
	namespace Model {
		namespace Global {
			//
			// ░ ProjectBlocksOrderInfo
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			ProjectBlocksOrderInfo::ProjectBlocksOrderInfo(const std::vector<H::Guid>& orderedProjectGuids)
				: mapGuidToOrderPriority{ ProjectBlocksOrderInfo::CreateMapGuidToOrderPriority(orderedProjectGuids) } {
			}

			std::size_t ProjectBlocksOrderInfo::GetOrderPriorityForProjectGuid(const H::Guid& guid) const {
				if (auto it = this->mapGuidToOrderPriority.find(guid);
					it != this->mapGuidToOrderPriority.end()) {
					return it->second;
				}
				return (std::numeric_limits<std::size_t>::max)(); // max priority
			};

			std::unordered_map<H::Guid, std::size_t> ProjectBlocksOrderInfo::CreateMapGuidToOrderPriority(
				const std::vector<H::Guid>& orderedProjectGuids
			) {
				std::unordered_map<H::Guid, std::size_t> resultMapGuidToOrderPriority;

				std::size_t orderPriority = 0;
				for (const auto& guid : orderedProjectGuids) {
					if (!resultMapGuidToOrderPriority.contains(guid)) {
						resultMapGuidToOrderPriority[guid] = orderPriority;
						orderPriority += 1;
					}
				}

				return resultMapGuidToOrderPriority;
			}


			//
			// ░ Parsed global block
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			std::string ParsedGlobalBlock::Serialize(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const {
				std::string out;
				out += "Global\n";

				// Сортируем секции в порядке принятом Visual Studio.
				const std::array<std::string_view, 6> sectionOrder = {
					ParsedSolutionConfigurationPlatformsSection::SectionName,
					ParsedProjectConfigurationPlatformsSection::SectionName,
					ParsedSolutionPropertiesSection::SectionName,
					ParsedNestedProjectsSection::SectionName,
					ParsedExtensibilityGlobalsSection::SectionName,
					ParsedSharedMSBuildProjectFilesSection::SectionName,
				};

				using SectionMapConstIterator_t = typename decltype(this->sectionMap)::const_iterator;

				std::vector<SectionMapConstIterator_t> sortedSectionIterators;
				sortedSectionIterators.reserve(this->sectionMap.size());

				for (auto it = this->sectionMap.cbegin(); it != this->sectionMap.cend(); ++it) {
					sortedSectionIterators.push_back(it);
				}

				auto getSectionPriorityFn = [&sectionOrder](std::string_view sectionName) {
					for (std::size_t i = 0; i < sectionOrder.size(); ++i) {
						if (sectionOrder[i] == sectionName) {
							return i;
						}
					}
					return (std::numeric_limits<std::size_t>::max)();
					};

				std::stable_sort(
					sortedSectionIterators.begin(),
					sortedSectionIterators.end(),
					[&getSectionPriorityFn](
						const SectionMapConstIterator_t& itA,
						const SectionMapConstIterator_t& itB
						) {
						auto priorityA = getSectionPriorityFn(itA->first);
						auto priorityB = getSectionPriorityFn(itB->first);

						if (priorityA != priorityB) {
							return priorityA < priorityB;
						}

						return false;
					}
				);

				for (const auto& itSectioon : sortedSectionIterators) {
					out += itSectioon->second->Serialize(serviceProviderOpt);
				}

				out += "EndGlobal\n";
				return out;
			}


			//
			// ░ Parsed global sections
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			// 
			// ░ SolutionConfigurationPlatforms
			//
			ParsedSolutionConfigurationPlatformsSection::ParsedSolutionConfigurationPlatformsSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}

			std::vector<std::string> ParsedSolutionConfigurationPlatformsSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> /*serviceProviderOpt*/
			) const {
				std::vector<std::string> rows;

				// Сортируем в естественном порядке.
				auto sortedEntries = this->entries;
				std::sort(sortedEntries.begin(), sortedEntries.end());

				for (const auto& configEntry : sortedEntries) {
					rows.push_back(std::format(
						"{}|{} = {}|{}",
						configEntry.declaredConfguration,
						configEntry.declaredPlatform,
						configEntry.assignedConfguration,
						configEntry.assignedPlatform
					));
				}

				return rows;
			}


			// 
			// ░ ProjectConfigurationPlatforms
			//
			bool ParsedProjectConfigurationPlatformsSection::Entry::operator<(const Entry& other) const {
				if (this->guid == other.guid) {
					return this->configEntry < other.configEntry;
				}
				return this->guid < other.guid;
			}

			bool ParsedProjectConfigurationPlatformsSection::Entry::operator==(const Entry& other) const {
				return 
					this->guid == other.guid &&
					this->configEntry == other.configEntry;
			}


			ParsedProjectConfigurationPlatformsSection::ParsedProjectConfigurationPlatformsSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}

			std::vector<std::string> ParsedProjectConfigurationPlatformsSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const {
				// Группы строк идут по проектам в том порядке, в каком проекты
				// объявлены в верхней части ".sln".
				std::vector<std::string> rows;

				// Сортируем в естественном порядке (это нужно чтобы равные guid отсортировались по configEntry).
				auto sortedEntries = this->entries;
				std::sort(sortedEntries.begin(), sortedEntries.end());

				// Перегруппируем под порядок проектов из верхней части .sln.
				// Используем stable_sort, чтобы сохранить локальный порядок внутри равных групп.
				if (serviceProviderOpt) {
					if (auto projectBlocksOrderInfo = serviceProviderOpt->Get<ProjectBlocksOrderInfo>()) {
						std::stable_sort(
							sortedEntries.begin(),
							sortedEntries.end(),
							[&](const Entry& a, const Entry& b) {
								auto priorityA = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(a.guid);
								auto priorityB = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(b.guid);

								if (priorityA != priorityB) {
									return priorityA < priorityB;
								}

								return false; // Сохраняем исходный порядок
							}
						);
					}
				}

				for (const auto& entry : sortedEntries) {
					const std::string indexSuffix = entry.configEntry.index
						? std::format(".{}", *entry.configEntry.index)
						: "";

					const std::string key = std::format(
						"{}|{}.{}{}",
						entry.configEntry.declaredConfguration,
						entry.configEntry.declaredPlatform,
						entry.configEntry.action,
						indexSuffix
					);

					const std::string value = std::format(
						"{}|{}",
						entry.configEntry.assignedConfguration,
						entry.configEntry.assignedPlatform
					);

					rows.push_back(std::format(
						"{}.{} = {}",
						entry.guid,
						key,
						value
					));
				}

				return rows;
			}


			// 
			// ░ SolutionProperties
			//
			ParsedSolutionPropertiesSection::ParsedSolutionPropertiesSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}

			std::vector<std::string> ParsedSolutionPropertiesSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const {
				std::vector<std::string> rows;

				for (const auto& entry : this->entries) {
					rows.push_back(std::format(
						"{} = {}",
						entry.key,
						entry.value
					));
				}

				std::sort(rows.begin(), rows.end());
				return rows;
			}

			
			// 
			// ░ NestedProjects
			//
			ParsedNestedProjectsSection::ParsedNestedProjectsSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}
			
			std::vector<std::string> ParsedNestedProjectsSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const {
				// Группы строк идут по проектам в том порядке, в каком проекты
				// объявлены в верхней части ".sln".
				std::vector<std::string> rows;

				auto sortedEntries = this->entries;

				if (serviceProviderOpt) {
					if (auto projectBlocksOrderInfo = serviceProviderOpt->Get<ProjectBlocksOrderInfo>()) {
						std::stable_sort(
							sortedEntries.begin(),
							sortedEntries.end(),
							[&](const Entry& a, const Entry& b) {
								auto childPriorityA = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(a.childGuid);
								auto childPriorityB = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(b.childGuid);

								if (childPriorityA != childPriorityB) {
									return childPriorityA < childPriorityB;
								}

								// Обычно записи в первой колонке (child guid) уникальны.
								// childPriorityA == childPriorityB только в случае когда оба приоритета = max priority.
								auto parentPriorityA = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(a.parentGuid);
								auto parentPriorityB = projectBlocksOrderInfo->GetOrderPriorityForProjectGuid(b.parentGuid);

								if (parentPriorityA != parentPriorityB) {
									return parentPriorityA < parentPriorityB;
								}

								return false; // Сохраняем исходный порядок
							}
						);
					}
				}

				for (const auto& entry : sortedEntries) {
					rows.push_back(std::format(
						"{} = {}",
						entry.childGuid,
						entry.parentGuid
					));
				}

				return rows;
			}

			
			// 
			// ░ ExtensibilityGlobals
			//
			ParsedExtensibilityGlobalsSection::ParsedExtensibilityGlobalsSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}
			
			std::vector<std::string> ParsedExtensibilityGlobalsSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> /*serviceProviderOpt*/
			) const {
				std::vector<std::string> rows;

				rows.push_back(std::format(
					"SolutionGuid = {}",
					this->solutionGuid
				));

				std::sort(rows.begin(), rows.end());
				return rows;
			}
			

			// 
			// ░ SharedMSBuildProjectFiles
			//
			ParsedSharedMSBuildProjectFilesSection::ParsedSharedMSBuildProjectFilesSection(const Model::Raw::Section& section)
				: DefaultCloneableImplInherited_t{ section.name, section.role } {
			}

			std::vector<std::string> ParsedSharedMSBuildProjectFilesSection::SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const {
				// Группы строк сортируются по guid в лексиграфическом порядке.
				std::vector<std::string> rows;

				auto sortedEntries = this->entries;

				std::stable_sort(
					sortedEntries.begin(),
					sortedEntries.end(),
					[](
						const Model::Entries::SharedMsBuildProjectFileEntry& a,
						const Model::Entries::SharedMsBuildProjectFileEntry& b
						) noexcept {
							if (a.guid != b.guid) {
								return a.guid < b.guid;
							}
							else if (a.relativePath != b.relativePath) {
								return a.relativePath < b.relativePath; // по пути
							}
							return a.key < b.key; // по ключу (например, SharedItemsImports)
					}
				);

				for (const auto& entry : sortedEntries) {
					rows.push_back(std::format(
						"{}*{}*{} = {}",
						entry.relativePath,
						entry.guid.ToString(H::Guid::Format::Lowercase), // по дизайну VS
						entry.key,
						entry.value
					));
				}

				return rows;
			}
		}
	}
}