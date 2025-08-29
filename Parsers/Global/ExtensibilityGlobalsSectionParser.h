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
			class ExtensibilityGlobalsSectionParser : public ISectionParser {
			public:
				std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) override {
					auto result = Model::details::MakeSharedParsedSection<Model::Global::ParsedExtensibilityGlobalsSection>(section);

					static const std::regex rxEntry(
						R"_rx_(^\s*SolutionGuid\s*=\s*\{([0-9A-Fa-f\-]+)\}\s*$)_rx_"
						//                              └(1)───────────┘
						// (1): Solution GUID (без фигурных скобок)
					);

					for (const auto& line
						: section.lines
						| std::ranges::views::drop(1)
						| std::ex::ranges::views::drop_last(1)
						) {
						if (auto rxMatchResult = H::Regex::GetRegexMatch<char, std::string>(line, rxEntry)) {
							LOG_ASSERT(rxMatchResult->capturedGroups.size() > 1);

							result->solutionGuid = H::Guid::Parse(rxMatchResult->capturedGroups[1]);
						}
					}

					return result;
				}
			};
		}
	}
}