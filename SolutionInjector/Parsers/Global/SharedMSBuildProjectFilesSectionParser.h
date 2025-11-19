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
			class SharedMSBuildProjectFilesSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedSharedMSBuildProjectFilesSection>(section);

					// Example: "UtilityHelpersLib\Helpers\Helpers.Shared\Helpers.Shared.vcxitems*{1f26dd5b-02c2-4192-ac91-06bed3d214f3}*SharedItemsImports = 4"
					static const std::regex rxEntry(
						R"_rx_(^\s*(.+?)\*\{([0-9A-Fa-f\-]+)\}\*(.+?)\s*=\s*(.+)$)_rx_"
						//         └(1)┘    └(2)───────────┘    └(3)┘       └(4)┘
						// (1): Relative path
						// (2): Project GUID
						// (3): Key
						// (4): Value
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 4);

							auto relativePath = rxMatchResult->capturedGroups[1];
							auto guid = H::Guid::Parse(rxMatchResult->capturedGroups[2]);
							auto key = rxMatchResult->capturedGroups[3];
							auto value = rxMatchResult->capturedGroups[4];

							result->entries.push_back(
								Model::Entries::SharedMsBuildProjectFileEntry {
									relativePath,
									guid,
									key,
									value
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