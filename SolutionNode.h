#pragma once
#include <Helpers/Extensions/memoryEx.h>
#include <Helpers/Guid.h>

#include <filesystem>
#include <string>
#include <vector>

namespace Core {
	struct ConfigEntry {
		std::string key;   // Debug|x86.Build.0
		std::string value; // Debug|Win32

		std::string configuration; // Debug
		std::string platform;      // x64
		std::string action;        // ActiveCfg / Build / Deploy / Run
		int index = 0;

		std::string configurationAndPlatform; // "Debug|x86"
	};


	struct SolutionNode;

	struct SharedMsBuildProjectFileEntry {
		std::filesystem::path relativePath;     // путь перед первым '*'
		H::Guid guid;                           // GUID внутри '{}'
		std::string key;                        // ключ после второго '*'
		std::string value;                      // значение после '='
		std::weak_ptr<SolutionNode> solutionNode; // ссылка на проект (необ€зательно)

		SharedMsBuildProjectFileEntry(
			std::string relativePath,
			H::Guid guid,
			std::string key,
			std::string value,
			std::weak_ptr<SolutionNode> solutionNode = {})
			: relativePath{ relativePath }
			, guid{ guid }
			, key{ key }
			, value{ value }
			, solutionNode{ solutionNode } {
		}
	};


	struct SolutionNode : public std::enable_shared_from_this<SolutionNode> {
		H::Guid typeGuid;
		H::Guid guid;
		std::string name;
		std::filesystem::path uniquePath;

		// –одитель хранитс€ как weak_ptr, чтобы избежать циклических владений:
		// - –одитель владеет детьми через shared_ptr, обеспечива€ их жизнь.
		// - ѕри уничтожении родител€ childNodes удал€ютс€,
		//   что снижает счЄтчики детей, и они тоже уничтожаютс€, если нет других владельцев.
		std::ex::weak_ptr<SolutionNode> parentNodeWeak;

	protected:
		SolutionNode(
			H::Guid typeGuid,
			H::Guid guid,
			std::string name,
			std::filesystem::path uniquePath)
			: typeGuid{ typeGuid }
			, guid{ guid }
			, name{ name }
			, uniquePath{ uniquePath } {
		}

	public:
		virtual ~SolutionNode() {}
	};


	struct SolutionFolder : SolutionNode {
		std::vector<std::ex::shared_ptr<SolutionNode>> childNodes;

		explicit SolutionFolder(
			H::Guid typeGuid,
			H::Guid guid,
			std::string name,
			std::filesystem::path uniquePath)
			: SolutionNode(typeGuid, guid, name, uniquePath) {
		}

		void LinkChildNode(std::ex::shared_ptr<SolutionNode> childSolutionNode) {
			childSolutionNode->parentNodeWeak = this->shared_from_this();
			this->childNodes.push_back(std::move(childSolutionNode));
		}
	};

	struct ProjectNode : SolutionNode {
		std::vector<ConfigEntry> configurations;
		std::vector<SharedMsBuildProjectFileEntry> sharedMsBuildProjectFiles;

		ProjectNode(
			H::Guid typeGuid,
			H::Guid guid,
			std::string name,
			std::filesystem::path uniquePath)
			: SolutionNode(typeGuid, guid, name, uniquePath) {
		}
	};
}