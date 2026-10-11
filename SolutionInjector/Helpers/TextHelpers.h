#pragma once
#include <string>

namespace Core {
	namespace Helpers {
		std::string ToUpperAscii(std::string value);
		std::string Trim(const std::string& value);
		std::wstring Utf8ToWide(const std::string& value);
		std::string WideToUtf8(const std::wstring& value);
	}
}