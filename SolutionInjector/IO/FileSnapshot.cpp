#include "FileSnapshot.h"

#include <windows.h>

#include "../Helpers/TextHelpers.h"

#include <stdexcept>
#include <fstream>

namespace Core {
	namespace IO {
		namespace {
			std::string ReadBytes(const std::filesystem::path& path) {
				std::ifstream stream(path, std::ios::binary);
				if (!stream) {
					throw std::runtime_error("Cannot read " + Helpers::WideToUtf8(path.wstring()));
				}
				return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
			}
		}

		std::string FileSnapshot::PathKey(const std::filesystem::path& path) {
			return Helpers::ToUpperAscii(Helpers::WideToUtf8(std::filesystem::weakly_canonical(std::filesystem::absolute(path)).wstring()));
		}

		std::string FileSnapshot::Read(const std::filesystem::path& path) {
			// 1. Читаем содержимое и сверяем его с ранее зафиксированным снимком.
			const auto content = ReadBytes(path);
			const auto key = PathKey(path);
			const auto found = files.find(key);
			if (found != files.end() && found->second.second != content) {
				throw std::runtime_error("File changed during analysis");
			}

			// 2. Запоминаем путь и байты для проверки перед сохранением.
			files[key] = {path, content};
			return content;
		}

		void FileSnapshot::Verify() const {
			// Проверяем, что все прочитанные файлы сохранили исходное содержимое.
			for (const auto& entry : files) {
				if (ReadBytes(entry.second.first) != entry.second.second) {
					throw std::runtime_error("File changed; rerun analysis");
				}
			}
		}

		void ReplaceWithBackup(const std::filesystem::path& path, const std::string& content, const FileSnapshot& fileSnapshot) {
			// 1. Проверяем снимок до подготовки нового решения.
			fileSnapshot.Verify();
			auto temporary = path;
			temporary += L".SolutionInjector." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
			auto backup = path;
			backup += L".SolutionInjector." + std::to_wstring(GetTickCount64()) + L".bak";
			HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				throw std::runtime_error("Cannot create temporary solution");
			}

			// 2. Записываем временный файл и сбрасываем его на диск.
			DWORD written = 0;
			const bool success = content.size() <= MAXDWORD &&
				WriteFile(handle, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
				written == content.size() && FlushFileBuffers(handle);
			CloseHandle(handle);
			if (!success) {
				DeleteFileW(temporary.c_str());
				throw std::runtime_error("Cannot write temporary solution");
			}

			// 3. Повторно проверяем снимок непосредственно перед заменой.
			try {
				fileSnapshot.Verify();
			} catch (...) {
				DeleteFileW(temporary.c_str());
				throw;
			}

			// 4. Заменяем решение с созданием резервной копии.
			if (!ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr)) {
				DeleteFileW(temporary.c_str());
				throw std::runtime_error("Cannot replace solution");
			}
		}
	}
}