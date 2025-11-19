#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>

#include "ParsedSectionBase.h"
#include "ISerializable.h"

#include <unordered_map>
#include <string_view>
#include <string>

namespace Core {
	namespace Model {
		struct ParsedBlockBase : ISerializable {
			std::unordered_map<std::string, std::ex::shared_ptr<ParsedSectionBase>> sectionMap;

			virtual ~ParsedBlockBase() {}
		};
	}
}