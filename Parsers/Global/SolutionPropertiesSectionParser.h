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
			class SolutionPropertiesSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedSolutionPropertiesSection>(section);

					static const std::regex rxEntry(
						R"_rx_(^\s*(.*?)\s*=\s*(.*?)\s*$)_rx_"
						//         └(1)┘       └(2)┘
						// (1): Key
						// (2): Value
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 2);

							const auto& key = rxMatchResult->capturedGroups[1];
							const auto& value = rxMatchResult->capturedGroups[2];

							result->entries.push_back(
								Model::Entries::KeyValuePair{
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