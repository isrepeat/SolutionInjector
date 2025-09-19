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
			class NestedProjectsSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedNestedProjectsSection>(section);

					static const std::regex rxEntry(
						R"_rx_(^\s*\{([0-9A-Fa-f\-]+)\}\s*=\s*\{([0-9A-Fa-f\-]+)\}\s*$)_rx_"
						//          └(1)────────────┘           └(2)───────────┘
						// (1): Child GUID
						// (2): Parent GUID
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 2);

							auto childGuid = H::Guid::Parse(rxMatchResult->capturedGroups[1]);
							auto parentGuid = H::Guid::Parse(rxMatchResult->capturedGroups[2]);

							result->entries.push_back(
								Model::Global::ParsedNestedProjectsSection::Entry{
									childGuid,
									parentGuid
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