#pragma once
#include <Helpers/StringComparers.h>
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

				bool operator<(const ConfigEntry& other) const {
					if (this->declaredConfguration != other.declaredConfguration) {
						return H::CaseInsensitiveComparer::IsLess(
							this->declaredConfguration,
							other.declaredConfguration
						);
					}

					if (this->declaredPlatform != other.declaredPlatform) {
						return H::CaseInsensitiveComparer::IsLess(
							this->declaredPlatform,
							other.declaredPlatform
						);
					}

					// Для SolutionConfigurationPlatforms action пустой и index = nullopt —
					// ниже просто не сработает и порядок останется лексикографическим по (Cfg, Plat).
					const int la = this->ActionPriority(this->action);
					const int ra = this->ActionPriority(other.action);
					if (la != ra) {
						return la < ra;
					}

					const int li = this->index ? *this->index : -1;
					const int ri = other.index ? *other.index : -1;
					if (li != ri) {
						return li < ri;
					}
					
					if (this->assignedConfguration != other.assignedConfguration) {
						return H::CaseInsensitiveComparer::IsLess(
							this->assignedConfguration,
							other.assignedConfguration
						);
					}
					return H::CaseInsensitiveComparer::IsLess(
						this->assignedPlatform,
						other.assignedPlatform
					);
				}

				bool operator==(const ConfigEntry& other) const {
					return
						H::CaseInsensitiveComparer::IsEqual(this->declaredConfguration, other.declaredConfguration) &&
						H::CaseInsensitiveComparer::IsEqual(this->declaredPlatform, other.declaredPlatform) &&
						this->action == other.action &&
						this->index == other.index &&
						H::CaseInsensitiveComparer::IsEqual(this->assignedConfguration, other.assignedConfguration) &&
						H::CaseInsensitiveComparer::IsEqual(this->assignedPlatform, other.assignedPlatform);
				}

			private:
				int ActionPriority(std::string_view action) const {
					if (action == "ActiveCfg") {
						return 0;
					}
					if (action == "Build") {
						return 1;
					}
					if (action == "Deploy") {
						return 2;
					}
					return 10;
				}
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