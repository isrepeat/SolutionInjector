#pragma once
#include <Helpers/Extensions/memoryEx.h>
#include <Helpers/StringComparers.h>

#include "SolutionNode.h"
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
		const std::map<H::Guid, std::ex::shared_ptr<SolutionNode>>& GetSolutionNodes() const;

		void AddProjectNode(const std::ex::shared_ptr<ProjectNode>& projectNode);
		void AddSolutionFolder(const std::ex::shared_ptr<SolutionFolder>& solutionFolder);
		void AddSolutionNodeRecursively(const std::ex::shared_ptr<SolutionNode>& solutionNode);

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

		void SerializeSolutionNodeRecursively(
			const std::ex::shared_ptr<SolutionNode>& solutionNode,
			std::vector<std::string>& outLines,
			int indentLevel = 0
		) const;

		void SortSolutionNodesRecursively(std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodesToSort) const;
		bool CompareSolutionNodes(
			const std::ex::shared_ptr<SolutionNode>& solutionNodeA,
			const std::ex::shared_ptr<SolutionNode>& solutionNodeB
		) const;

		ProjectTypePriority GetProjectTypePriorityByPath(std::ex::shared_ptr<SolutionNode> solutionNode) const;

	private:
		H::Guid solutionGuid;
		std::filesystem::path solutionPath;
		std::vector<ConfigEntry> solutionConfigurations;
		std::map<std::string, std::string, H::CaseInsensitiveComparer> mapSolutionPropertyKeyToValue;
		std::map<H::Guid, std::ex::shared_ptr<SolutionNode>> mapGuidToSolutionNode;
	};
}