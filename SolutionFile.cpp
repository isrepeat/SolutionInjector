#include "Singletons/Constants.h"
#include "SolutionFile.h"
#include <iostream>
#include <fstream>
#include <regex>

namespace Core {
	bool SolutionFile::Load(const std::string& path) {
		std::ifstream in(path);
		if (!in) {
			return false;
		}
		std::string line;
		this->lines.clear();
		while (std::getline(in, line)) {
			this->lines.push_back(line);
		}
		return true;
	}

	void SolutionFile::Save(const std::string& path) const {
		std::ofstream out(path);
		for (const auto& line : this->lines) {
			out << line << "\n";
		}
	}

	const std::vector<std::string>& SolutionFile::GetLines() const {
		return this->lines;
	}

	std::unordered_set<std::string> SolutionFile::GetGuids() const {
		std::unordered_set<std::string> guids;
		std::smatch m;
		for (const auto& line : this->lines) {
			if (std::regex_search(line, m, Singletons::Constants::GetInstance().projectRe)) {
				guids.insert(m[1]);
			}
		}
		return guids;
	}

	std::vector<ProjectBlock> SolutionFile::GetAllProjects() const {
		std::vector<ProjectBlock> result;
		for (size_t i = 0; i < this->lines.size(); ++i) {
			if (auto parsed = ProjectBlock::Parse(this->lines, i)) {
				result.push_back(*parsed);
			}
		}
		return result;
	}

	std::unordered_map<std::string, std::string> SolutionFile::GetNestedMap() const {
		std::unordered_map<std::string, std::string> map;
		bool found = false;
		for (size_t i = 0; i < this->lines.size(); ++i) {
			if (this->lines[i].find("GlobalSection(NestedProjects)") != std::string::npos) {
				found = true;
				for (++i; i < this->lines.size() && this->lines[i].find("EndGlobalSection") == std::string::npos; ++i) {
					auto line = this->lines[i];
					size_t eq = line.find('=');
					if (eq != std::string::npos) {
						std::string child = line.substr(0, eq);
						std::string parent = line.substr(eq + 1);
						child.erase(remove(child.begin(), child.end(), '\t'), child.end());
						child.erase(remove(child.begin(), child.end(), ' '), child.end());
						parent.erase(remove(parent.begin(), parent.end(), ' '), parent.end());
						map[child] = parent;
					}
				}
				break;
			}
		}
		return map;
	}

	void SolutionFile::InsertProjects(const std::vector<ProjectBlock>& blocks) {
		int insertIndex = this->lines.size();
		for (int i = this->lines.size() - 1; i >= 0; --i) {
			if (this->lines[i].find("EndProject") != std::string::npos) {
				insertIndex = i + 1;
				break;
			}
		}
		for (const auto& block : blocks) {
			this->lines.insert(this->lines.begin() + insertIndex, block.lines.begin(), block.lines.end());
			insertIndex += block.lines.size();
		}
	}

	void SolutionFile::InsertNestedLines(const std::vector<std::string>& nestedLines) {
		for (auto it = this->lines.begin(); it != this->lines.end(); ++it) {
			if (it->find("GlobalSection(NestedProjects)") != std::string::npos) {
				++it;
				this->lines.insert(it, nestedLines.begin(), nestedLines.end());
				return;
			}
		}
		auto endIt = std::find(this->lines.rbegin(), this->lines.rend(), "EndGlobal").base();

		std::vector<std::string> section = { "\tGlobalSection(NestedProjects) = preSolution" };
		section.insert(section.end(), nestedLines.begin(), nestedLines.end());
		section.push_back("\tEndGlobalSection");

		this->lines.insert(endIt, section.begin(), section.end());
	}

	void SolutionFile::InsertConfigurations(const std::vector<std::string>& configLines) {
		for (auto it = this->lines.begin(); it != this->lines.end(); ++it) {
			if (it->find("GlobalSection(ProjectConfigurationPlatforms)") != std::string::npos) {
				++it;
				this->lines.insert(it, configLines.begin(), configLines.end());
				return;
			}
		}
	}
}