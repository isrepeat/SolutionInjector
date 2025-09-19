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
			// ░ ProjectBlocksOrderInfo
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			class ProjectBlocksOrderInfo {
			public:
				ProjectBlocksOrderInfo(const std::vector<H::Guid>& orderedProjectGuids);

				std::size_t GetOrderPriorityForProjectGuid(const H::Guid& guid) const;

			private:
				static std::unordered_map<H::Guid, std::size_t> CreateMapGuidToOrderPriority(
					const std::vector<H::Guid>& orderedProjectGuids
				);

			private:
				const std::unordered_map<H::Guid, std::size_t> mapGuidToOrderPriority;
			};


			//
			// ░ Parsed global block
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			struct ParsedGlobalBlock : ParsedBlockBase {
				std::string Serialize(
					std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
				) const override;
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

				ParsedSolutionConfigurationPlatformsSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> /*serviceProviderOpt*/
				) const override;
			};


			// 
			// ░ ProjectConfigurationPlatforms
			//
			struct ParsedProjectConfigurationPlatformsSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedProjectConfigurationPlatformsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "ProjectConfigurationPlatforms";

				// Добавляем операторы сравнения для упрощения сортировки, разделяем ее на 2 прохода.
				struct Entry {
					H::Guid guid;
					Model::Entries::ConfigEntry configEntry;

					bool operator<(const Entry& other) const;
					bool operator==(const Entry& other) const;
				};

				std::vector<Entry> entries;

				ParsedProjectConfigurationPlatformsSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
				) const override;
			};


			// 
			// ░ SolutionProperties
			//
			struct ParsedSolutionPropertiesSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedSolutionPropertiesSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SolutionProperties";

				std::vector<Model::Entries::KeyValuePair> entries;

				ParsedSolutionPropertiesSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
				) const override;
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

				ParsedNestedProjectsSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
				) const override;
			};


			// 
			// ░ ExtensibilityGlobals
			//
			struct ParsedExtensibilityGlobalsSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedExtensibilityGlobalsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "ExtensibilityGlobals";

				H::Guid solutionGuid;

				ParsedExtensibilityGlobalsSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> /*serviceProviderOpt*/
				) const override;
			};


			// 
			// ░ SharedMSBuildProjectFiles
			//
			struct ParsedSharedMSBuildProjectFilesSection :
				ParsedGlobalSectionBase::DefaultCloneableImpl_t<ParsedSharedMSBuildProjectFilesSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SharedMSBuildProjectFiles";

				std::vector<Model::Entries::SharedMsBuildProjectFileEntry> entries;

				ParsedSharedMSBuildProjectFilesSection(const Model::Raw::Section& section);

			private:
				std::vector<std::string> SerializeBody(
					std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
				) const override;
			};
		}
	}
}