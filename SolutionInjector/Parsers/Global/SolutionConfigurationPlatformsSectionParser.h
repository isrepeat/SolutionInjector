#pragma once
#include <Helpers/Std/Extensions/rangesEx.h>
#include <Helpers/Logger.h>
#include <Helpers/Regex.h>

#include "../../Model/Global/GlobalModel.h"
#include "../ISectionParser.h"

#include <format>
#include <regex>

namespace Core {
	namespace Parsers {
		namespace Global {
			class SolutionConfigurationPlatformsSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedSolutionConfigurationPlatformsSection>(section);

					static const std::regex rxEntry(
						R"_rx_(^\s*([^\|=]+?)\|([^\|=]+?)\s*=\s*([^\|=]+?)\|([^\|=]+?)\s*$)_rx_"
						//         └(1)─────┘  └(2)─────┘       └(3)─────┘  └(4)─────┘
						// (1): Declared Configuration
						// (2): Declared Platform
						// (3): Assigned Configuration
						// (4): Assigned Platform
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 4);

							const auto& declaredConfig = rxMatchResult->capturedGroups[1];
							const auto& declaredPlatform = rxMatchResult->capturedGroups[2];
							const auto& assignedConfig = rxMatchResult->capturedGroups[3];
							const auto& assignedPlatform = rxMatchResult->capturedGroups[4];

							Model::Entries::ConfigEntry configEntry;
							configEntry.declaredConfguration = declaredConfig;
							configEntry.declaredPlatform = declaredPlatform;
							configEntry.assignedConfguration = assignedConfig;
							configEntry.assignedPlatform = assignedPlatform;
							configEntry.action = {};
							configEntry.index = std::nullopt;

							configEntry.declaredConfigurationAndPlatform = std::format(
								"{}|{}",
								configEntry.declaredConfguration,
								configEntry.declaredPlatform
							);

							configEntry.assignedConfigurationAndPlatform = std::format(
								"{}|{}",
								configEntry.assignedConfguration,
								configEntry.assignedPlatform
							);

							result->entries.push_back(configEntry);
						}
					}

					return result;
				}
			};
		}
	}
}