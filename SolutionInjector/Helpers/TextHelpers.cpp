#include "TextHelpers.h"

#include <windows.h>

#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace Core {
	namespace Helpers {
		std::string ToUpperAscii(std::string value) {
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
				return static_cast<char>(std::toupper(character));
			});
			return value;
		}

		std::string Trim(const std::string& value) {
			// Находим границы содержимого; строку из одних пробелов считаем пустой.
			const auto first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) {
				return {};
			}
			return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
		}

		std::wstring Utf8ToWide(const std::string& value) {
			// 1. Определяем размер результата и проверяем корректность UTF-8.
			const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
			if (!count && !value.empty()) {
				throw std::runtime_error("Invalid UTF-8");
			}
			std::wstring result(count, L' ');

			// 2. Выполняем преобразование в буфер рассчитанного размера.
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
			return result;
		}

		std::string WideToUtf8(const std::wstring& value) {
			// 1. Определяем размер UTF-8 и проверяем корректность исходной строки.
			const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
			if (!count && !value.empty()) {
				throw std::runtime_error("Invalid Unicode");
			}
			std::string result(count, ' ');

			// 2. Записываем UTF-8 без завершающего нулевого символа.
			WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
			return result;
		}
	}
}