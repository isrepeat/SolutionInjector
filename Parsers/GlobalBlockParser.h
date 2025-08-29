#pragma once
#include <Helpers/Logger.h>

#include "../Model/Global/GlobalModel.h"
#include "Global/SolutionConfigurationPlatformsSectionParser.h"
#include "Global/ProjectConfigurationPlatformsSectionParser.h"
#include "Global/SharedMSBuildProjectFilesSectionParser.h"
#include "Global/ExtensibilityGlobalsSectionParser.h"
#include "Global/SolutionPropertiesSectionParser.h"
#include "Global/NestedProjectsSectionParser.h"
#include "ISectionParser.h"

namespace Core {
	namespace Parsers {
		class GlobalBlockParser {
		public:
			GlobalBlockParser() {
				this->sectionParserMap.emplace(
					Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName,
					std::make_unique<Global::SolutionConfigurationPlatformsSectionParser>());

				this->sectionParserMap.emplace(
					Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName,
					std::make_unique<Global::ProjectConfigurationPlatformsSectionParser>());

				this->sectionParserMap.emplace(
					Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName,
					std::make_unique<Global::SharedMSBuildProjectFilesSectionParser>());

				this->sectionParserMap.emplace(
					Model::Global::ParsedExtensibilityGlobalsSection::SectionName,
					std::make_unique<Global::ExtensibilityGlobalsSectionParser>());

				this->sectionParserMap.emplace(
					Model::Global::ParsedSolutionPropertiesSection::SectionName,
					std::make_unique<Global::SolutionPropertiesSectionParser>());

				this->sectionParserMap.emplace(
					Model::Global::ParsedNestedProjectsSection::SectionName,
					std::make_unique<Global::NestedProjectsSectionParser>());
			}

			std::ex::shared_ptr<Model::Global::ParsedGlobalBlock> Parse(
				const Model::Raw::SolutionDocument& solutionDocument
			) {
				auto parsedGlobalBlockResult = std::ex::make_shared_ex<Model::Global::ParsedGlobalBlock>();

				for (const auto& [key, section] : solutionDocument.globalBlock.sectionMap) {
					auto it = this->sectionParserMap.find(section.name);
					if (it != this->sectionParserMap.end()) {
						auto& sectionParser = it->second;

						if (auto parsedSection = sectionParser->TryParse(section)) {
							parsedGlobalBlockResult->sectionMap[key] = parsedSection;
						}
					}
				}

				return parsedGlobalBlockResult;
			}

		private:
			std::unordered_map<std::string, std::unique_ptr<ISectionParser>> sectionParserMap;
		};
	}
}