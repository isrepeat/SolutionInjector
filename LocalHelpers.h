#pragma once
#include <string>
#include <regex>

namespace LocalHelpers {
	///// Escapes all regex-special punctuation characters in the input string.
	///// Required for safe usage in std::regex, e.g. for GUIDs like "{ABC-123}".
	///// Uses std::ispunct with unsigned char to avoid UB on negative chars.
	//inline std::string EscapeRegex(const std::string& input) {
	//	std::string result;
	//	for (char ch : input) {
	//		if (std::ispunct(static_cast<unsigned char>(ch))) {
	//			result += '\\';
	//		}
	//		result += ch;
	//	}
	//	return result;
	//}



	//inline std::string Trim(const std::string& str) {
	//	size_t first = 0;
	//	while (first < str.size() && (str[first] == ' ' || str[first] == '\t')) {
	//		++first;
	//	}

	//	if (first == str.size()) {
	//		return "";
	//	}

	//	size_t last = str.size() - 1;
	//	while (last > first && (str[last] == ' ' || str[last] == '\t')) {
	//		--last;
	//	}

	//	return str.substr(first, last - first + 1);
	//}
}