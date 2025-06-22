#pragma once
#include "SolutionProject.h"
#include "LocalHelpers.h"
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>


namespace Core {
	struct CaseInsensitiveComparer {
		bool operator()(const std::string& lhs, const std::string& rhs) const {
			return IsLess(lhs, rhs);
		}

		static bool IsLess(const std::string& lhs, const std::string& rhs) {
			return std::lexicographical_compare(
				lhs.begin(), lhs.end(),
				rhs.begin(), rhs.end(),
				[](unsigned char a, unsigned char b) {
					return std::tolower(a) < std::tolower(b);
				}
			);
		}

		static bool IsEqual(const std::string& lhs, const std::string& rhs) {
			if (lhs.size() != rhs.size()) {
				return false;
			}

			for (size_t i = 0; i < lhs.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i]))) {
					return false;
				}
			}

			return true;
		}
	};


	class SolutionStructure {
	public:
		static inline const H::Guid SolutionFolderGuid = H::Guid::Parse("2150E333-8FDC-42A3-9474-1A3956D46DE8");

	public:
		SolutionStructure(std::filesystem::path solutionPath)
			: solutionPath{ solutionPath }
		{}

		bool Parse() {
			std::ifstream file(this->solutionPath);
			if (!file.is_open()) {
				return false;
			}


			std::regex projectLineParser(R"_rx_(Project\("\{([^\"]+)\}"\) = "([^\"]+)", "([^\"]+)", "\{([^\"]+)\}")_rx_");

			std::regex solutionConfigsSectionStart(R"_rx_(\s*GlobalSection\(SolutionConfigurationPlatforms\) = preSolution)_rx_");
			//std::regex solutionConfigLineParser(R"_rx_(\s*(Debug|Release\|x64|x86|Win32|AnyCPU|[\w\|\-]+) = (Debug|Release\|x64|x86|Win32|AnyCPU|[\w\|\-]+))_rx_");

			std::regex projectConfigsSectionStart(R"_rx_(\s*GlobalSection\(ProjectConfigurationPlatforms\) = postSolution)_rx_");
			std::regex projectConfigLineParser(R"_rx_(\s*\{([0-9A-F\-]+)\}\.([^\=]+) = (.+))_rx_");

			std::regex nestedProjectsSectionStart(R"_rx_(\s*GlobalSection\(NestedProjects\) = preSolution)_rx_");
			std::regex nestedProjectsLineParser(R"_rx_(\s*\{([0-9A-F\-]+)\} = \{([0-9A-F\-]+)\})_rx_");

			std::regex solutionPropertiesSectionStart(R"_rx_(\s*GlobalSection\(SolutionProperties\) = preSolution)_rx_");
			//std::regex solutionPropertiesLineParser(R"_rx_(\s*(\w+) = (.+))_rx_");

			std::regex extensibilityGlobalsSectionStart(R"_rx_(\s*GlobalSection\(ExtensibilityGlobals\) = postSolution)_rx_");
			std::regex solutionGuidLineParser(R"_rx_(SolutionGuid = \{([0-9A-F\-]+)\})_rx_");

			std::regex sharedMSBuildProjectFilesSectionStart(R"_rx_(\s*GlobalSection\(SharedMSBuildProjectFiles\) = preSolution)_rx_");
			std::regex sharedMSBuildProjectFilesLineParser(R"_rx_(^\s*([^\*\s]+)\*\{([0-9a-fA-F\-]+)\}\*([^\s]+)\s*=\s*(.+)$)_rx_");

			std::regex keyValueParser(R"_rx_(^\s*(.*?)\s*=\s*(.*?)\s*$)_rx_");


			enum class ParseSection {
				None,
				SolutionConfigurations,
				ProjectConfigurations,
				NestedProjects,
				SolutionProperties,
				ExtensibilityGlobals,
				SharedMSBuildProjectFiles,
			};
			ParseSection currentSection = ParseSection::None;

			std::string line;
			std::smatch match;

			while (std::getline(file, line)) {
				switch (currentSection) {
				case ParseSection::None:
					if (std::regex_search(line, match, projectLineParser)) {
						auto typeGuid = H::Guid::Parse(match[1].str());
						auto name = match[2].str();
						auto path = match[3].str();
						auto guid = H::Guid::Parse(match[4].str());

						auto project = std::make_shared<SolutionProject>(typeGuid, guid, name, path);
						this->projectsMap[guid] = project;
					}
					else if (std::regex_search(line, match, solutionConfigsSectionStart)) {
						currentSection = ParseSection::SolutionConfigurations;
					}
					else if (std::regex_search(line, match, projectConfigsSectionStart)) {
						currentSection = ParseSection::ProjectConfigurations;
					}
					else if (std::regex_search(line, match, nestedProjectsSectionStart)) {
						currentSection = ParseSection::NestedProjects;
					}
					else if (std::regex_search(line, match, solutionPropertiesSectionStart)) {
						currentSection = ParseSection::SolutionProperties;
					}
					else if (std::regex_search(line, match, extensibilityGlobalsSectionStart)) {
						currentSection = ParseSection::ExtensibilityGlobals;
					}
					else if (std::regex_search(line, match, sharedMSBuildProjectFilesSectionStart)) {
						currentSection = ParseSection::SharedMSBuildProjectFiles;
					}
					break;

				case ParseSection::SolutionConfigurations:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (std::regex_search(line, match, keyValueParser)) {
						auto key = match[1].str();
						auto value = match[2].str();

						this->solutionConfigurations.push_back(ConfigEntry{key, value});
					}

					break;

				case ParseSection::ProjectConfigurations:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (std::regex_search(line, match, projectConfigLineParser)) {
						auto guid = H::Guid::Parse(match[1].str());
						auto key = match[2].str();
						auto value = match[3].str();

						auto it = this->projectsMap.find(guid);
						if (it != this->projectsMap.end()) {
							it->second->configurations.push_back({ key, value });
						}
					}
					break;

				case ParseSection::NestedProjects:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (currentSection == ParseSection::NestedProjects) {
						if (line.find("EndGlobalSection") != std::string::npos) {
							currentSection = ParseSection::None;
						}
						else if (std::regex_search(line, match, nestedProjectsLineParser)) {
							auto childGuid = H::Guid::Parse(match[1].str());
							auto parentGuid = H::Guid::Parse(match[2].str());

							auto itChild = this->projectsMap.find(childGuid);
							auto itParent = this->projectsMap.find(parentGuid);

							if (itChild != this->projectsMap.end() && itParent != this->projectsMap.end()) {
								itParent->second->AddChild(itChild->second);
							}
						}
					}
					break;

				case ParseSection::SolutionProperties:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (std::regex_search(line, match, keyValueParser)) {
						auto key = match[1].str();
						auto value = match[2].str();

						this->solutionProperties[key] = value;
					}
					break;

				case ParseSection::ExtensibilityGlobals:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (std::regex_search(line, match, solutionGuidLineParser)) {
						this->solutionGuid = H::Guid::Parse(match[1].str());
					}
					break;

				case ParseSection::SharedMSBuildProjectFiles:
					if (line.find("EndGlobalSection") != std::string::npos) {
						currentSection = ParseSection::None;
					}
					else if (std::regex_search(line, match, sharedMSBuildProjectFilesLineParser)) {
						auto relativePath = match[1].str();
						auto guid = H::Guid::Parse(match[2].str());
						auto key = match[3].str();
						auto value = match[4].str();

						auto itProject = this->projectsMap.find(guid);
						if (itProject != this->projectsMap.end()) {
							itProject->second->sharedMsBuildProjectFiles.emplace_back(
								relativePath,
								guid,
								key,
								value,
								itProject->second
							);
						}
					}
					break;
				}
			}

			return true;
		}


		std::vector<std::string> SerializeToSln() const {
			std::vector<std::string> result;

			result.push_back("Microsoft Visual Studio Solution File, Format Version 12.00");
			result.push_back("# Visual Studio Version 17");
			result.push_back("VisualStudioVersion = 17.11.35312.102");
			result.push_back("MinimumVisualStudioVersion = 10.0.40219.1");

			// —обираем корневые проекты (у которых нет родителей)
			std::vector<std::shared_ptr<SolutionProject>> rootProjects;
			for (const auto& [guid, projectPtr] : this->projectsMap) {
				if (projectPtr->parentWeak.expired()) { // корневые проекты (без родител€)
					rootProjects.push_back(projectPtr);
				}
			}

			// —ортируем корневые проекты рекурсивно
			this->SortProjectsRecursively(rootProjects);

			for (const auto& root : rootProjects) {
				this->SerializeProjectRecursively(root, result);
			}

			// Global
			result.push_back("Global");

			// SolutionConfigurationPlatforms
			result.push_back(std::format("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution"));
			for (const auto& configEntry : this->solutionConfigurations) {
				result.push_back(
					std::format("\t\t{} = {}",
						configEntry.key,
						configEntry.value
					)
				);
			}
			result.push_back("\tEndGlobalSection");


			// ProjectConfigurationPlatforms
			result.push_back("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution");
			for (const auto& [guid, projectPtr] : this->projectsMap) {
				for (const auto& configEntry : projectPtr->configurations) {
					result.push_back(
						std::format(
							"\t\t{}.{} = {}",
							guid.ToString(),
							configEntry.key,
							configEntry.value
						)
					);
				}
			}
			result.push_back("\tEndGlobalSection");


			// NestedProjects
			result.push_back(std::format("\tGlobalSection(NestedProjects) = preSolution"));
			for (const auto& [guid, projectPtr] : this->projectsMap) {
				if (auto parentShared = projectPtr->parentWeak.lock()) {
					result.push_back(
						std::format(
							"\t\t{} = {}",
							projectPtr->projectGuid.ToString(),
							parentShared->projectGuid.ToString()
						)
					);
				}
			}
			result.push_back("\tEndGlobalSection");


			// SolutionProperties
			result.push_back("\tGlobalSection(SolutionProperties) = preSolution");
			for (const auto& [key, value] : this->solutionProperties) {
				result.push_back(std::format("\t\t{} = {}", key, value));
			}
			result.push_back("\tEndGlobalSection");


			// ExtensibilityGlobals
			result.push_back("\tGlobalSection(ExtensibilityGlobals) = postSolution");
			result.push_back(std::format("\t\tSolutionGuid = {}", this->solutionGuid.ToString()));
			result.push_back("\tEndGlobalSection");


			// SharedMSBuildProjectFiles
			result.push_back(std::format("\tGlobalSection(SharedMSBuildProjectFiles) = preSolution"));
			std::vector<std::string> sharedLines;

			for (const auto& [guid, projectPtr] : this->projectsMap) {
				for (const auto& sharedEntry : projectPtr->sharedMsBuildProjectFiles) {
					sharedLines.push_back(
						std::format(
							"\t\t{}*{}*{} = {}",
							sharedEntry.relativePath.string(),
							sharedEntry.guid.ToString(),
							sharedEntry.key,
							sharedEntry.value
						)
					);
				}
			}

			// —ортируем в алфавитном пор€дке
			std::sort(sharedLines.begin(), sharedLines.end());

			// «аписываем отсортированные строки
			for (const auto& line : sharedLines) {
				result.push_back(line);
			}
			result.push_back(std::format("\tEndGlobalSection"));
			result.push_back(std::format("EndGlobal"));

			return result;
		}


		std::filesystem::path GetSolutionPath() const {
			return this->solutionPath;
		}

		const std::vector<ConfigEntry>& GetSolutionConfigurations() const {
			return this->solutionConfigurations;
		}

		const std::map<H::Guid, std::shared_ptr<SolutionProject>>& GetProjects() const {
			return this->projectsMap;
		}



		void AddSolutionFolder(const std::shared_ptr<SolutionProject>& solutionFolder) {
			this->AddProjectRecursively(solutionFolder);
		}

		void AddProject(const std::shared_ptr<SolutionProject>& project) {
			this->projectsMap[project->projectGuid] = project;
		}

		void AddProjectRecursively(const std::shared_ptr<SolutionProject>& project) {
			this->projectsMap[project->projectGuid] = project;

			for (const auto& child : project->children) {
				this->AddProjectRecursively(child);
			}
		}


		void Save() const {
			this->Save(this->solutionPath);
		}

		void Save(const std::filesystem::path& savePath) const {
			std::ofstream outFileStream(savePath);
			for (const auto& line : this->SerializeToSln()) {
				outFileStream << line << "\n";
			}
		}

		void LogSerializedSolution() const {
			auto lines = this->SerializeToSln();
			for (const auto& line : lines) {
				LOG_DEBUG_D("{}", line);
			}
		}

	private:
		enum class ProjectTypePriority : int {
			SolutionFolder = 0,
			Vcxproj = 1,
			Vcxitems = 2,
			Wapproj = 3,
			Unknown
		};


		void SerializeProjectRecursively(
			const std::shared_ptr<SolutionProject>& project,
			std::vector<std::string>& outLines,
			int indentLevel = 0
		) const {
			std::string indent(indentLevel, '\t');

			outLines.push_back(std::format(
				"{}Project(\"{}\") = \"{}\", \"{}\", \"{}\"",
				indent,
				project->projectTypeGuid.ToString(),
				project->projectName,
				project->projectPath.string(),
				project->projectGuid.ToString()
			));
			outLines.push_back(indent + "EndProject");

			if (!project->children.empty()) {
				auto childrenCopy = project->children;
				SortProjectsRecursively(childrenCopy);

				for (const auto& child : childrenCopy) {
					this->SerializeProjectRecursively(child, outLines, 0); // or indentLevel + 1
				}
			}
		}


		void SortProjectsRecursively(std::vector<std::shared_ptr<SolutionProject>>& projectsToSort) const {
			std::sort(projectsToSort.begin(), projectsToSort.end(),
				[this](const auto& a, const auto& b) {
					return this->CompareProjects(a, b);
				});

			for (auto& proj : projectsToSort) {
				if (!proj->children.empty()) {
					this->SortProjectsRecursively(proj->children);
				}
			}
		}


		bool CompareProjects(
			const std::shared_ptr<SolutionProject>& projectA,
			const std::shared_ptr<SolutionProject>& projectB
		) const {
			ProjectTypePriority prioA = this->GetProjectTypePriorityByPath(projectA);
			ProjectTypePriority prioB = this->GetProjectTypePriorityByPath(projectB);

			if (prioA != prioB) {
				return static_cast<int>(prioA) < static_cast<int>(prioB);
			}
			return CaseInsensitiveComparer::IsLess(projectA->projectName, projectB->projectName);
		}


		ProjectTypePriority GetProjectTypePriorityByPath(std::shared_ptr<SolutionProject> project) const {
			if (project->projectTypeGuid == SolutionStructure::SolutionFolderGuid) {
				return ProjectTypePriority::SolutionFolder;
			}

			std::string ext = project->projectPath.extension().string();

			if (ext == ".vcxproj") {
				return ProjectTypePriority::Vcxproj;
			}
			if (ext == ".vcxitems") {
				return ProjectTypePriority::Vcxitems;
			}
			if (ext == ".wapproj") {
				return ProjectTypePriority::Wapproj;
			}
			return ProjectTypePriority::Unknown;
		}

	private:
		H::Guid solutionGuid;
		std::filesystem::path solutionPath;
		std::vector<ConfigEntry> solutionConfigurations;
		std::map<std::string, std::string, CaseInsensitiveComparer> solutionProperties;
		std::map<H::Guid, std::shared_ptr<SolutionProject>> projectsMap;
	};
}