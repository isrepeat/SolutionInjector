#include "SolutionStructure.h"
#include <Helpers/Logger.h>

#include <algorithm>
#include <fstream>
#include <format>
#include <regex>


namespace Core {
	SolutionStructure::SolutionStructure(std::filesystem::path solutionPath)
		: solutionPath{ solutionPath } {
	}


	bool SolutionStructure::Parse() {
		std::ifstream file(this->solutionPath);
		if (!file.is_open()) {
			return false;
		}

		std::regex projectLineParser(R"_rx_(Project\("\{([^\"]+)\}"\) = "([^\"]+)", "([^\"]+)", "\{([^\"]+)\}")_rx_");

		std::regex solutionConfigsSectionStart(R"_rx_(\s*GlobalSection\(SolutionConfigurationPlatforms\) = preSolution)_rx_");
		std::regex solutionConfigLineParser(R"_rx_(^\s*([^\|]+)\|([^\.\s]+)\s*=\s*(\S+))_rx_");

		std::regex projectConfigsSectionStart(R"_rx_(\s*GlobalSection\(ProjectConfigurationPlatforms\) = postSolution)_rx_");
		std::regex projectConfigLineParser(R"_rx_(^\s*\{([A-F0-9\-]+)\}\.([^\|]+)\|([^\.\s]+)\.([A-Za-z]+)(?:\.(\d+))?\s*=\s*(\S+))_rx_");

		std::regex nestedProjectsSectionStart(R"_rx_(\s*GlobalSection\(NestedProjects\) = preSolution)_rx_");
		std::regex nestedProjectsLineParser(R"_rx_(\s*\{([0-9A-F\-]+)\} = \{([0-9A-F\-]+)\})_rx_");

		std::regex solutionPropertiesSectionStart(R"_rx_(\s*GlobalSection\(SolutionProperties\) = preSolution)_rx_");

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

					std::ex::shared_ptr<SolutionNode> solutionNode;
					if (typeGuid == SolutionStructure::SolutionFolderGuid) {
						solutionNode = std::ex::make_shared_ex<SolutionFolder>(typeGuid, guid, name, path);
					}
					else {
						solutionNode = std::ex::make_shared_ex<ProjectNode>(typeGuid, guid, name, path);
					}

					this->mapGuidToSolutionNode[guid] = solutionNode;
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
				else if (std::regex_search(line, match, solutionConfigLineParser)) {
					std::string config = match[1].str();
					std::string platform = match[2].str();
					std::string value = match[3].str();

					ConfigEntry configEntry;
					configEntry.key = std::format("{}|{}", config, platform);
					configEntry.value = value;
					configEntry.configuration = config;
					configEntry.platform = platform;
					configEntry.configurationAndPlatform = std::format("{}|{}", config, platform);

					this->solutionConfigurations.push_back(configEntry);
				}
				break;

			case ParseSection::ProjectConfigurations:
				if (line.find("EndGlobalSection") != std::string::npos) {
					currentSection = ParseSection::None;
				}
				else if (std::regex_search(line, match, projectConfigLineParser)) {
					H::Guid guid = H::Guid::Parse(match[1].str());
					std::string config = match[2].str();
					std::string platform = match[3].str();
					std::string action = match[4].str();
					std::string indexStr = match[5].matched ? match[5].str() : "0";
					std::string value = match[6].str();

					ConfigEntry configEntry;
					configEntry.key = std::format("{}|{}.{}{}",
						config,
						platform,
						action,
						match[5].matched ? std::format(".{}", indexStr) : ""
					);
					configEntry.value = value;
					configEntry.configuration = config;
					configEntry.platform = platform;
					configEntry.action = action;
					configEntry.index = std::stoi(indexStr);

					configEntry.configurationAndPlatform = std::format("{}|{}", config, platform);

					auto it = this->mapGuidToSolutionNode.find(guid);
					if (it != this->mapGuidToSolutionNode.end()) {
						auto projectNode = it->second.Cast<ProjectNode>();
						projectNode->configurations.push_back(configEntry);
					}
				}
				break;

			case ParseSection::NestedProjects:
				if (line.find("EndGlobalSection") != std::string::npos) {
					currentSection = ParseSection::None;
				}
				else if (std::regex_search(line, match, nestedProjectsLineParser)) {
					auto childGuid = H::Guid::Parse(match[1].str());
					auto parentGuid = H::Guid::Parse(match[2].str());

					auto itChildNode = this->mapGuidToSolutionNode.find(childGuid);
					auto itParentNode = this->mapGuidToSolutionNode.find(parentGuid);

					if (itChildNode != this->mapGuidToSolutionNode.end() &&
						itParentNode != this->mapGuidToSolutionNode.end()
						) {
						auto solutionFolder = itParentNode->second.Cast<SolutionFolder>();
						solutionFolder->LinkChildNode(itChildNode->second);
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

					this->mapSolutionPropertyKeyToValue[key] = value;
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

					auto itSolutionNode = this->mapGuidToSolutionNode.find(guid);
					if (itSolutionNode != this->mapGuidToSolutionNode.end()) {
						auto projectNode = itSolutionNode->second.Cast<ProjectNode>();
						projectNode->sharedMsBuildProjectFiles.emplace_back(
							relativePath,
							guid,
							key,
							value,
							itSolutionNode->second
						);
					}
				}
				break;
			}
		}

		return true;
	}

	bool SolutionStructure::IsParsed() const {
		return !this->mapGuidToSolutionNode.empty();
	}


	std::vector<std::string> SolutionStructure::SerializeToSln() const {
		std::vector<std::string> result;

		result.push_back("Microsoft Visual Studio Solution File, Format Version 12.00");
		result.push_back("# Visual Studio Version 17");
		result.push_back("VisualStudioVersion = 17.11.35312.102");
		result.push_back("MinimumVisualStudioVersion = 10.0.40219.1");

		// —обираем корневые проекты (у которых нет родителей)
		std::vector<std::ex::shared_ptr<SolutionNode>> rootSolutionNodes;
		for (const auto& [guid, solutionNode] : this->mapGuidToSolutionNode) {
			if (solutionNode->parentNodeWeak.expired()) { // корневые проекты (без родител€)
				rootSolutionNodes.push_back(solutionNode);
			}
		}

		// —ортируем корневые проекты рекурсивно
		this->SortSolutionNodesRecursively(rootSolutionNodes);

		for (const auto& rootSolutionNode : rootSolutionNodes) {
			this->SerializeSolutionNodeRecursively(rootSolutionNode, result);
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
		for (const auto& [guid, solutionNode] : this->mapGuidToSolutionNode) {
			if (auto projectNode = solutionNode.As<ProjectNode>()) {
				for (const auto& configEntry : projectNode->configurations) {
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
		}
		result.push_back("\tEndGlobalSection");


		// NestedProjects
		result.push_back(std::format("\tGlobalSection(NestedProjects) = preSolution"));
		for (const auto& [guid, solutionNode] : this->mapGuidToSolutionNode) {
			if (auto parentNode = solutionNode->parentNodeWeak.lock()) {
				result.push_back(
					std::format(
						"\t\t{} = {}",
						solutionNode->guid.ToString(),
						parentNode->guid.ToString()
					)
				);
			}
		}
		result.push_back("\tEndGlobalSection");


		// SolutionProperties
		result.push_back("\tGlobalSection(SolutionProperties) = preSolution");
		for (const auto& [key, value] : this->mapSolutionPropertyKeyToValue) {
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

		for (const auto& [guid, solutionNode] : this->mapGuidToSolutionNode) {
			if (auto projectNode = solutionNode.As<ProjectNode>()) {
				for (const auto& sharedEntry : projectNode->sharedMsBuildProjectFiles) {
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


	std::filesystem::path SolutionStructure::GetSolutionPath() const {
		return this->solutionPath;
	}

	const std::vector<ConfigEntry>& SolutionStructure::GetSolutionConfigurations() const {
		return this->solutionConfigurations;
	}

	const std::map<H::Guid, std::ex::shared_ptr<SolutionNode>>& SolutionStructure::GetSolutionNodes() const {
		return this->mapGuidToSolutionNode;
	}


	void SolutionStructure::AddProjectNode(const std::ex::shared_ptr<ProjectNode>& projectNode) {
		this->mapGuidToSolutionNode[projectNode->guid] = projectNode;
	}

	void SolutionStructure::AddSolutionFolder(const std::ex::shared_ptr<SolutionFolder>& solutionFolder) {
		this->AddSolutionNodeRecursively(solutionFolder);
	}

	void SolutionStructure::AddSolutionNodeRecursively(const std::ex::shared_ptr<SolutionNode>& solutionNode) {
		this->mapGuidToSolutionNode[solutionNode->guid] = solutionNode;
		
		if (auto solutionFolder = solutionNode.As<SolutionFolder>()) {
			for (const auto& child : solutionFolder->childNodes) {
				this->AddSolutionNodeRecursively(child);
			}
		}	
	}


	void SolutionStructure::Save() const {
		this->Save(this->solutionPath);
	}

	void SolutionStructure::Save(const std::filesystem::path& savePath) const {
		std::ofstream outFileStream(savePath);
		for (const auto& line : this->SerializeToSln()) {
			outFileStream << line << "\n";
		}
	}


	void SolutionStructure::LogSerializedSolution() const {
		auto lines = this->SerializeToSln();
		for (const auto& line : lines) {
			LOG_DEBUG_D("{}", line);
		}
	}


	void SolutionStructure::SerializeSolutionNodeRecursively(
		const std::ex::shared_ptr<SolutionNode>& solutionNode,
		std::vector<std::string>& outLines,
		int indentLevel
	) const {
		std::string indent(indentLevel, '\t');

		outLines.push_back(std::format(
			"{}Project(\"{}\") = \"{}\", \"{}\", \"{}\"",
			indent,
			solutionNode->typeGuid.ToString(),
			solutionNode->name,
			solutionNode->uniquePath.string(),
			solutionNode->guid.ToString()
		));
		outLines.push_back(indent + "EndProject");

		if (auto solutionFolder = solutionNode.As<SolutionFolder>()) {
			if (!solutionFolder->childNodes.empty()) {
				auto childNodesCopy = solutionFolder->childNodes;
				this->SortSolutionNodesRecursively(childNodesCopy);

				for (const auto& childNode : childNodesCopy) {
					this->SerializeSolutionNodeRecursively(childNode, outLines, 0); // or indentLevel + 1
				}
			}
		}
	}


	void SolutionStructure::SortSolutionNodesRecursively(std::vector<std::ex::shared_ptr<SolutionNode>>& solutionNodesToSort) const {
		std::sort(solutionNodesToSort.begin(), solutionNodesToSort.end(),
			[this](const auto& a, const auto& b) {
				return this->CompareSolutionNodes(a, b);
			});

		for (auto& solutionNode : solutionNodesToSort) {
			if (auto solutionFolder = solutionNode.As<SolutionFolder>()) {
				if (!solutionFolder->childNodes.empty()) {
					this->SortSolutionNodesRecursively(solutionFolder->childNodes);
				}
			}
		}
	}


	bool SolutionStructure::CompareSolutionNodes(
		const std::ex::shared_ptr<SolutionNode>& solutionNodeA,
		const std::ex::shared_ptr<SolutionNode>& solutionNodeB
	) const {
		ProjectTypePriority prioA = this->GetProjectTypePriorityByPath(solutionNodeA);
		ProjectTypePriority prioB = this->GetProjectTypePriorityByPath(solutionNodeB);

		if (prioA != prioB) {
			return static_cast<int>(prioA) < static_cast<int>(prioB);
		}
		return H::CaseInsensitiveComparer::IsLess(solutionNodeA->name, solutionNodeB->name);
	}


	SolutionStructure::ProjectTypePriority SolutionStructure::GetProjectTypePriorityByPath(std::ex::shared_ptr<SolutionNode> solutionNode) const {
		if (solutionNode->typeGuid == SolutionStructure::SolutionFolderGuid) {
			return ProjectTypePriority::SolutionFolder;
		}

		std::string ext = solutionNode->uniquePath.extension().string();

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
}