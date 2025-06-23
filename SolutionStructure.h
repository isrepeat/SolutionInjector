#pragma once
#include <Helpers/StringComparers.h>
#include "SolutionProject.h"
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>


namespace Core {
	class SolutionStructure {
	public:
		static inline const H::Guid SolutionFolderGuid = H::Guid::Parse("2150E333-8FDC-42A3-9474-1A3956D46DE8");

	public:
		SolutionStructure(std::filesystem::path solutionPath);
		
		bool Parse();
		bool IsParsed() const;

		std::vector<std::string> SerializeToSln() const;
		
		std::filesystem::path GetSolutionPath() const;
		const std::vector<ConfigEntry>& GetSolutionConfigurations() const;
		const std::map<H::Guid, std::shared_ptr<SolutionProject>>& GetProjects() const;

		void AddSolutionFolder(const std::shared_ptr<SolutionProject>& solutionFolder);
		void AddProject(const std::shared_ptr<SolutionProject>& project);
		void AddProjectRecursively(const std::shared_ptr<SolutionProject>& project);

		void Save() const;
		void Save(const std::filesystem::path& savePath) const;

		void LogSerializedSolution() const;

	private:
		enum class ProjectTypePriority : int {
			SolutionFolder,
			Vcxproj,
			Vcxitems,
			Wapproj,
			Unknown
		};

		void SerializeProjectRecursively(
			const std::shared_ptr<SolutionProject>& project,
			std::vector<std::string>& outLines,
			int indentLevel = 0
		) const;

		void SortProjectsRecursively(std::vector<std::shared_ptr<SolutionProject>>& projectsToSort) const;
		bool CompareProjects(
			const std::shared_ptr<SolutionProject>& projectA,
			const std::shared_ptr<SolutionProject>& projectB
		) const;

		ProjectTypePriority GetProjectTypePriorityByPath(std::shared_ptr<SolutionProject> project) const;

	private:
		H::Guid solutionGuid;
		std::filesystem::path solutionPath;
		std::vector<ConfigEntry> solutionConfigurations;
		std::map<std::string, std::string, H::CaseInsensitiveComparer> solutionProperties;
		std::map<H::Guid, std::shared_ptr<SolutionProject>> projectsMap;
	};
}