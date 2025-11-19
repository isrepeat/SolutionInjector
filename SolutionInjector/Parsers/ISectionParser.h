#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include "../Model/Raw/SolutionDocument.h"
#include "../Model/ParsedSectionBase.h"

namespace Core {
	namespace Parsers {
		class ISectionParser {
		public:
			virtual ~ISectionParser() = default;
			virtual std::ex::shared_ptr<Model::ParsedSectionBase> TryParse(const Model::Raw::Section& section) = 0;
		};
	}
}