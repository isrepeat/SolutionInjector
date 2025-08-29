#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/Logger.h>
#include <Helpers/Guid.h>

#include "../Raw/SolutionDocument.h"
#include "../ParsedBlockBase.h"
#include "../Entries.h"

namespace Core {
	namespace Model {
		namespace Global {
			//
			// ░ Parsed global block
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			struct ParsedGlobalBlock : ParsedBlockBase {
				std::string Serialize() const override {
					std::string out;
					out += "Global\n";

					for (const auto& [key, section] : this->sectionMap) {
						out += section->Serialize();
					}

					out += "EndGlobal\n";
					return out;
				}
			};


			//
			// ░ Parsed global sections
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			// 
			// ░ SolutionConfigurationPlatforms
			//
			struct ParsedSolutionConfigurationPlatformsSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedSolutionConfigurationPlatformsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SolutionConfigurationPlatforms";

				std::vector<Model::Entries::ConfigEntry> entries;

				ParsedSolutionConfigurationPlatformsSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
					std::vector<std::string> rows;

					for (const auto& configEntry : this->entries) {
						rows.push_back(std::format(
							"{}|{} = {}|{}",
							configEntry.declaredConfguration,
							configEntry.declaredPlatform,
							configEntry.assignedConfguration,
							configEntry.assignedPlatform
						));
					}

					std::sort(rows.begin(), rows.end());
					return rows;
				}
			};


			// 
			// ░ ProjectConfigurationPlatforms
			//
			struct ParsedProjectConfigurationPlatformsSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedProjectConfigurationPlatformsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "ProjectConfigurationPlatforms";

				struct Entry {
					H::Guid guid;
					Model::Entries::ConfigEntry configEntry;
				};

				std::vector<Entry> entries;

				ParsedProjectConfigurationPlatformsSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
					std::vector<std::string> rows;

					for (const auto& entry : this->entries) {
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

					std::sort(rows.begin(), rows.end());
					return rows;
				}
			};


			// 
			// ░ SolutionProperties
			//
			struct ParsedSolutionPropertiesSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedSolutionPropertiesSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SolutionProperties";

				std::vector<Model::Entries::KeyValuePair> entries;

				ParsedSolutionPropertiesSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
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
			};


			// 
			// ░ NestedProjects
			//
			struct ParsedNestedProjectsSection : 
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedNestedProjectsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "NestedProjects";

				struct Entry {
					H::Guid childGuid;
					H::Guid parentGuid;
				};

				std::vector<Entry> entries;

				ParsedNestedProjectsSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
					std::vector<std::string> rows;

					for (const auto& entry : this->entries) {
						rows.push_back(std::format(
							"{} = {}",
							entry.childGuid,
							entry.parentGuid
						));
					}

					std::sort(rows.begin(), rows.end());
					return rows;
				}
			};


			// 
			// ░ ExtensibilityGlobals
			//
			struct ParsedExtensibilityGlobalsSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedExtensibilityGlobalsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "ExtensibilityGlobals";

				H::Guid solutionGuid;

				ParsedExtensibilityGlobalsSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
					std::vector<std::string> rows;

					rows.push_back(std::format(
						"SolutionGuid = {}",
						this->solutionGuid
					));

					std::sort(rows.begin(), rows.end());
					return rows;
				}
			};


			// 
			// ░ SharedMSBuildProjectFiles
			//
			struct ParsedSharedMSBuildProjectFilesSection : 
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedSharedMSBuildProjectFilesSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SharedMSBuildProjectFiles";

				std::vector<Model::Entries::SharedMsBuildProjectFileEntry> entries;

				ParsedSharedMSBuildProjectFilesSection(const Model::Raw::Section& section)
					: DefaultCloneableImplInherited_t{ section.name, section.role } {
				}

			private:
				std::vector<std::string> SerializeBody() const override {
					std::vector<std::string> rows;

					for (const auto& entry : this->entries) {
						rows.push_back(std::format(
							"{}*{}*{} = {}",
							entry.relativePath,
							entry.guid,
							entry.key,
							entry.value
						));
					}

					std::sort(rows.begin(), rows.end());
					return rows;
				}
			};
		}
	}
}