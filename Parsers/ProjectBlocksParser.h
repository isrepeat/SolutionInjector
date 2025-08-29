#pragma once
#include <Helpers/Logger.h>
#include <Helpers/Guid.h>

#include "../Model/Project/ProjectModel.h"
#include "Project/SolutionItemsSectionParser.h"
#include "ISectionParser.h"

#include <unordered_map>
#include <regex>

namespace Core {
	namespace Parsers {
		class ProjectBlocksParser {
		public:
			static inline const H::Guid SolutionFolderGuid = H::Guid::Parse("2150E333-8FDC-42A3-9474-1A3956D46DE8");

			ProjectBlocksParser(std::weak_ptr<Model::ISolutionStructureProvider> solutionStructureProviderWeak)
				: solutionStructureProviderWeak{ solutionStructureProviderWeak } {
				
				this->sectionParserMap.emplace(
					Model::Project::ParsedSolutionItemsSection::SectionName,
					std::make_unique<Project::SolutionItemsSectionParser>());
			}

			std::vector<std::ex::shared_ptr<Model::Project::ParsedProjectBlock>> Parse(
				const Model::Raw::SolutionDocument& solutionDocument
			) {
				// Example: 'Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "Helpers.Shared", "UtilityHelpersLib\Helpers\Helpers.Shared\Helpers.Shared.vcxitems", "{A3451486-AB38-4360-B752-94680B491792}"'
				static const std::regex rxProjectStart(
					R"_rx_(^\s*Project\("\{([0-9A-Fa-f\-]+)\}"\)\s*=\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"\{([0-9A-Fa-f\-]+)\}"\s*$)_rx_"
					//                     └(1)────────────────┘        └(2)──┘         └(3)──┘           └(4)───────────┘
					// (1): Type guid
					// (2): Name
					// (3): Relative path
					// (4): Guid
				);

				std::vector<std::ex::shared_ptr<Model::Project::ParsedProjectBlock>> resultParsedProjectBlocks;

				for (const auto& projectBlock : solutionDocument.projectBlocks) {
					auto headerLine = projectBlock.lines.front();

					if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(headerLine, rxProjectStart)) {
						LOG_ASSERT(rxMatchResult->capturedGroups.size() > 4);

						auto typeGuid = H::Guid::Parse(rxMatchResult->capturedGroups[1]);
						auto name = rxMatchResult->capturedGroups[2];
						auto relativePath = rxMatchResult->capturedGroups[3];
						auto guid = H::Guid::Parse(rxMatchResult->capturedGroups[4]);

						auto parsedProjectBlock = std::ex::make_shared_ex<Model::Project::ParsedProjectBlock>();
						if (typeGuid == this->SolutionFolderGuid) {
							parsedProjectBlock->solutionNode = std::ex::make_shared_ex<Model::Project::SolutionFolder>(
								typeGuid,
								guid,
								name,
								relativePath,
								this->solutionStructureProviderWeak);
						}
						else {
							parsedProjectBlock->solutionNode = std::ex::make_shared_ex<Model::Project::ProjectNode>(
								typeGuid,
								guid,
								name,
								relativePath,
								this->solutionStructureProviderWeak);
						}

						resultParsedProjectBlocks.push_back(parsedProjectBlock);

						for (const auto& [key, section] : projectBlock.sectionMap) {
							auto it = this->sectionParserMap.find(section.name);
							if (it != this->sectionParserMap.end()) {
								auto& sectionParser = it->second;

								if (auto parsedSection = sectionParser->TryParse(section)) {
									parsedProjectBlock->sectionMap[key] = parsedSection;
								}
							}
						}
					}
					else {
						LOG_ASSERT(false, "unexpected");
					}
				}

				return resultParsedProjectBlocks;
			}

		private:
			std::weak_ptr<Model::ISolutionStructureProvider> solutionStructureProviderWeak;
			std::unordered_map<std::string, std::unique_ptr<ISectionParser>> sectionParserMap;
		};
	}
}