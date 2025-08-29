#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/Std/FormatSpecializations.h>
#include <Helpers/Event/Signal.h>
#include <Helpers/ICloneable.h>
#include <Helpers/Logger.h>
#include <Helpers/Guid.h>

#include "../Raw/SolutionDocument.h"
#include "../ISolutionStructureProvider.h"
#include "../ParsedBlockBase.h"
#include "../Entries.h"

#include <unordered_map>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>

namespace Core {
	namespace Model {
		namespace Project {
			//
			// ░ ISolutionStructureProvider
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			struct SolutionNode;
			struct ParsedProjectBlock;

			class IProjectBlockReader : public virtual Model::ISolutionStructureProvider {
			public:
				virtual ~IProjectBlockReader() = default;

				virtual std::ex::shared_ptr<ParsedProjectBlock> GetProjectBlock(const H::Guid& guid) const = 0;
			};


			class IProjectEntriesReader : public virtual Model::ISolutionStructureProvider {
			public:
				virtual ~IProjectEntriesReader() = default;

				virtual std::ex::shared_ptr<SolutionNode> GetParent(const H::Guid& guid) const = 0;
				virtual std::vector<std::ex::shared_ptr<SolutionNode>> GetChildren(const H::Guid& guid) const = 0;
				virtual std::vector<Model::Entries::ConfigEntry> GetConfigurations(const H::Guid& guid) const = 0;
				virtual std::vector<Model::Entries::SharedMsBuildProjectFileEntry> GetSharedMsBuildProjectFiles(const H::Guid& guid) const = 0;
			};


			//
			// ░ SolutionNode
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			struct SolutionNode : H::ICloneable<
				SolutionNode,
				std::filesystem::path,
				std::ex::weak_ptr<Model::ISolutionStructureProvider>
			> {
				const H::Guid typeGuid;
				const H::Guid guid;
				const std::string name;
				const std::filesystem::path relativePath;

			protected:
				const std::ex::weak_ptr<Model::ISolutionStructureProvider> solutionStructureProviderWeak;

			public:
				SolutionNode(
					H::Guid typeGuid,
					H::Guid guid,
					std::string name,
					std::filesystem::path relativePath,
					std::ex::weak_ptr<Model::ISolutionStructureProvider> solutionStructureProviderWeak
				)
					: typeGuid{ typeGuid }
					, guid{ guid }
					, name{ name }
					, relativePath{ relativePath }
					, solutionStructureProviderWeak{ solutionStructureProviderWeak } {
				}

				SolutionNode(
					const SolutionNode& other,
					std::filesystem::path newRelativePath,
					std::ex::weak_ptr<Model::ISolutionStructureProvider> newSolutionStructureProviderWeak
				)
					: typeGuid{ other.typeGuid }
					, guid{ other.guid }
					, name{ other.name }
					, relativePath{ newRelativePath }
					, solutionStructureProviderWeak{ std::move(newSolutionStructureProviderWeak) } {
				}

				virtual ~SolutionNode() {}

				const Model::SolutionInfo GetSolutionInfo() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto solutionInfoReader = solutionStructureProvider.Cast<Model::ISolutionInfoReader>();
						return solutionInfoReader->GetSolutionInfo();
					}
					return {};
				}

				std::ex::shared_ptr<ParsedProjectBlock> GetProjectBlock() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto projectBlockReader = solutionStructureProvider.Cast<IProjectBlockReader>();
						return projectBlockReader->GetProjectBlock(this->guid);
					}
					return {};
				}

				std::ex::shared_ptr<SolutionNode> GetParent() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto projectEntriesReader = solutionStructureProvider.Cast<IProjectEntriesReader>();
						return projectEntriesReader->GetParent(this->guid);
					}
					return nullptr;
				}
			};


			//
			// ░ SolutionFolder
			//
			struct SolutionFolder : SolutionNode::ICloneableInherited_t::DefaultCloneableImpl<SolutionFolder> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				std::vector<std::ex::shared_ptr<SolutionNode>> GetChildren() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto projectEntriesReader = solutionStructureProvider.Cast<IProjectEntriesReader>();
						return projectEntriesReader->GetChildren(this->guid);
					}
					return {};
				}
			};


			//
			// ░ ProjectNode
			//
			struct ProjectNode : SolutionNode::ICloneableInherited_t::DefaultCloneableImpl<ProjectNode> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				std::vector<Model::Entries::ConfigEntry> GetConfigurations() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto projectEntriesReader = solutionStructureProvider.Cast<IProjectEntriesReader>();
						return projectEntriesReader->GetConfigurations(this->guid);
					}
					return {};
				}

				std::vector<Model::Entries::SharedMsBuildProjectFileEntry> GetSharedMsBuildProjectFiles() const {
					if (auto solutionStructureProvider = this->solutionStructureProviderWeak.lock()) {
						auto projectEntriesReader = solutionStructureProvider.Cast<IProjectEntriesReader>();
						return projectEntriesReader->GetSharedMsBuildProjectFiles(this->guid);
					}
					return {};
				}
			};


			//
			// ░ Parsed project block
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			//
			struct ParsedProjectBlock : ParsedBlockBase {
				std::ex::shared_ptr<SolutionNode> solutionNode;

				std::string Serialize() const override {
					std::string out;
					out += std::format(
						"Project(\"{}\") = \"{}\", \"{}\", \"{}\"\n",
						this->solutionNode->typeGuid,
						this->solutionNode->name,
						this->solutionNode->relativePath,
						this->solutionNode->guid
					);

					for (const auto& [key, section] : this->sectionMap) {
						out += section->Serialize();
					}

					out += "EndProject\n";
					return out;
				}
			};


			//
			// ░ Parsed project sections
			// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
			// 
			// ░ SolutionItems
			//
			struct ParsedSolutionItemsSection :
				ParsedProjectSectionBase::DefaultCloneableImpl_t<ParsedSolutionItemsSection> {
				using DefaultCloneableImplInherited_t::DefaultCloneableImplInherited_t;

				static constexpr std::string_view SectionName = "SolutionItems";

				std::vector<Model::Entries::KeyValuePair> entries;

				ParsedSolutionItemsSection(const Model::Raw::Section& section)
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

					return rows;
				}
			};
		}
	}
}