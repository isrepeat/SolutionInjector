#pragma once
#include <Helpers/Logger.h>
#include <Helpers/Regex.h>

#include "../../Model/Global/GlobalModel.h"
#include "../ISectionParser.h"

#include <format>
#include <regex>

namespace Core {
	namespace Parsers {
		namespace Global {
			class ProjectConfigurationPlatformsSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedProjectConfigurationPlatformsSection>(section);

					// Example: "{27C2C410-8770-4D35-941C-6EA738388F43}.Debug|x86.Build.0 = Debug|Win32"
					static const std::regex rxEntry(
						R"_rx_(^\s*\{([0-9A-Fa-f\-]+)\}\.([^\|]+)\|([^\|]+)\.([A-Za-z]+)(?:\.(\d+))?\s*=\s*([^\|=]+?)\|([^\|=]+?)\s*$)_rx_"
						//           └(1)───────────┘    └(2)───┘  └(3)───┘  └(4)──────┘     └(5)┘         └(6)─────┘  └(7)─────┘
						// (1): Project GUID
						// (2): Configuration declared
						// (3): Platform declared
						// (4): Action (e.g., ActiveCfg, Build)
						// (5): Optional index
						// (6): Configuration assigned
						// (7): Platform assigned
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 7);

							auto guid = H::Guid::Parse(rxMatchResult->capturedGroups[1]);
							auto declaredConfig = rxMatchResult->capturedGroups[2];
							auto declaredPlatform = rxMatchResult->capturedGroups[3];
							auto action = rxMatchResult->capturedGroups[4];
							auto indexStr = rxMatchResult->capturedGroups[5]; // [optional]
							auto assignedConfig = rxMatchResult->capturedGroups[6];
							auto assignedPlatform = rxMatchResult->capturedGroups[7];

							Model::Entries::ConfigEntry configEntry{};
							configEntry.declaredConfguration = declaredConfig;
							configEntry.declaredPlatform = declaredPlatform;
							configEntry.assignedConfguration = assignedConfig;
							configEntry.assignedPlatform = assignedPlatform;
							configEntry.action = action;

							if (!indexStr.empty()) {
								configEntry.index = std::stoi(indexStr);
							}

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
							
							result->entries.push_back(
								Model::Global::ParsedProjectConfigurationPlatformsSection::Entry{
									guid,
									configEntry
								}
							);
						}
					}

					return result;
				}
			};
		}
	}
}