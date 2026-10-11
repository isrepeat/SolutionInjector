#pragma once
#include <filesystem>
#include <string>
#include <map>

namespace Core {
	namespace IO {
		// Запоминает содержимое файлов, прочитанных при анализе решения, и проверяет его перед сохранением.
		// Если IDE или другой процесс изменит .sln, проект или импортируемые настройки после чтения,
		// Verify() остановит применение устаревшего плана очистки и предотвратит перезапись этих изменений.
		// Файлы не блокируются: проверка не исключает изменения между Verify() и самой заменой решения.
		class FileSnapshot {
		public:
			std::string Read(const std::filesystem::path& path);
			void Verify() const;
			static std::string PathKey(const std::filesystem::path& path);

		private:
			std::map<std::string, std::pair<std::filesystem::path, std::string>> files;
		};

		void ReplaceWithBackup(const std::filesystem::path& path, const std::string& content, const FileSnapshot& fileSnapshot);
	}
}