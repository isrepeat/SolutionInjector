#pragma once
#include <Helpers/Logger.h>

#include "../Model/Global/GlobalModel.h"
#include "../Model/RawSections.h"

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
                    Model::Global::ParsedSolutionPropertiesSection::SectionName,
                    std::make_unique<Global::SolutionPropertiesSectionParser>());

                this->sectionParserMap.emplace(
                    Model::Global::ParsedNestedProjectsSection::SectionName,
                    std::make_unique<Global::NestedProjectsSectionParser>());

                this->sectionParserMap.emplace(
                    Model::Global::ParsedExtensibilityGlobalsSection::SectionName,
                    std::make_unique<Global::ExtensibilityGlobalsSectionParser>());

                this->sectionParserMap.emplace(
                    Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName,
                    std::make_unique<Global::SharedMSBuildProjectFilesSectionParser>());
            }

            std::ex::shared_ptr<Model::Global::ParsedGlobalBlock> Parse(
                const Model::Raw::SolutionDocument& solutionDocument
            ) {
                auto parsedGlobalBlockResult = std::ex::make_shared_ex<Model::Global::ParsedGlobalBlock>();

                for (const auto& [key, sectionParser] : this->sectionParserMap) {
                    auto it = solutionDocument.globalBlock.sectionMap.find(key);
                    if (it != solutionDocument.globalBlock.sectionMap.end()) {
                        auto& section = it->second;

                        if (auto parsedSection = sectionParser->TryParse(section)) {
                            parsedGlobalBlockResult->sectionMap[key] = parsedSection;
                        }
                    }
                    else {
                        Core::Model::Raw::Section dummySection;
                        dummySection.name = key;

                        if (key == Model::Global::ParsedSolutionConfigurationPlatformsSection::SectionName) {
                            dummySection.role = "preSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedSolutionConfigurationPlatformsSection>(dummySection);
                        }
                        else if (key == Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName) {
                            dummySection.role = "postSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedProjectConfigurationPlatformsSection>(dummySection);
                        }
                        else if (key == Model::Global::ParsedSolutionPropertiesSection::SectionName) {
                            dummySection.role = "preSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedSolutionPropertiesSection>(dummySection);
                        }
                        else if (key == Model::Global::ParsedNestedProjectsSection::SectionName) {
                            dummySection.role = "preSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedNestedProjectsSection>(dummySection);
                        }
                        else if (key == Model::Global::ParsedExtensibilityGlobalsSection::SectionName) {
                            dummySection.role = "postSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedExtensibilityGlobalsSection>(dummySection);
                        }
                        else if (key == Model::Global::ParsedSharedMSBuildProjectFilesSection::SectionName) {
                            dummySection.role = "preSolution";
                            parsedGlobalBlockResult->sectionMap[key] =
                                Model::details::MakeSharedParsedSection<Model::Global::ParsedSharedMSBuildProjectFilesSection>(dummySection);
                        }
                    }
                }

                for (const auto& [key, section] : solutionDocument.globalBlock.sectionMap) {
                    if (!this->sectionParserMap.contains(key)) {
                        parsedGlobalBlockResult->sectionMap[key] = std::ex::make_shared_ex<Model::RawGlobalSection>(section);
                    }
                }
                return parsedGlobalBlockResult;
            }

        private:
            std::unordered_map<std::string, std::unique_ptr<ISectionParser>> sectionParserMap;
        };
    }
}