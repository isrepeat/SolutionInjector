#pragma once
#include <Helpers/Guid.h>
#include <string>
#include <vector>


struct ConfigEntry {
	std::string key;   // например, "Debug|x64.ActiveCfg"
	std::string value; // например, "Debug|x64"
};


class SolutionProject;
struct SharedMsBuildProjectFileEntry {
	std::filesystem::path relativePath;  // путь перед первым '*'
	H::Guid guid;                        // GUID внутри '{}'
	std::string key;                     // ключ после второго '*'
	std::string value;                   // значение после '='

	std::weak_ptr<SolutionProject> project; // ссылка на проект (необ€зательно)

	SharedMsBuildProjectFileEntry(
		std::string relativePath,
		H::Guid guid,
		std::string key,
		std::string value,
		std::weak_ptr<SolutionProject> project = {})
		: relativePath{ relativePath }
		, guid{ guid }
		, key{ key }
		, value{ value }
		, project{ project } {
	}
};


class SolutionProject : public std::enable_shared_from_this<SolutionProject> {
public:
	H::Guid projectTypeGuid;
	H::Guid projectGuid;
	std::string projectName;
	std::filesystem::path projectPath;

	// –одитель хранитс€ как weak_ptr, чтобы избежать циклических владений:
	// - –одитель владеет детьми через shared_ptr, обеспечива€ их жизнь.
	// - ѕри уничтожении родител€ children удал€ютс€,
	//   что снижает счЄтчики детей, и они тоже уничтожаютс€, если нет других владельцев.
	std::weak_ptr<SolutionProject> parentWeak;
	std::vector<std::shared_ptr<SolutionProject>> children;

	std::vector<ConfigEntry> configurations;
	std::vector<SharedMsBuildProjectFileEntry> sharedMsBuildProjectFiles;


public:
	SolutionProject(
		H::Guid projectTypeGuid,
		H::Guid projectGuid,
		std::string projectName,
		std::string projectPath) 
		: projectTypeGuid{ projectTypeGuid }
		, projectGuid{ projectGuid }
		, projectName{ projectName }
		, projectPath{ projectPath }
	{}

	void AddChild(std::shared_ptr<SolutionProject> child) {
		child->parentWeak = this->shared_from_this();
		this->children.push_back(std::move(child));
	}
};