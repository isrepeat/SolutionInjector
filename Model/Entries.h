#pragma once
#include <Helpers/Guid.h>

#include <filesystem>
#include <optional>
#include <vector>
#include <string>

namespace Core {
	namespace Model {
		namespace Entries {
			struct KeyValuePair {
				std::string key;   // Debug|x86.Build.0
				std::string value; // Debug|Win32
			};


			struct ConfigEntry {
				std::string declaredConfguration;	// Debug
				std::string assignedConfguration;	// Debug
				std::string declaredPlatform;		// x86
				std::string assignedPlatform;		// Win32
				std::string action;					// ActiveCfg / Build / Deploy / Run
				std::optional<int> index;

				std::string declaredConfigurationAndPlatform; // "Debug|x86"
				std::string assignedConfigurationAndPlatform; // "Debug|Win32"
			};


			struct SharedMsBuildProjectFileEntry {
				std::filesystem::path relativePath; // путь перед первым '*'
				H::Guid guid;                       // GUID внутри '{}'
				std::string key;                    // ключ после второго '*'
				std::string value;                  // значение после '='
			};
		}
	}
}