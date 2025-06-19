//// C++ console app that merges selected projects from one .sln file into another
//// Ensures new GUIDs for conflicting projects
//#include <iostream>
//#include <fstream>
//#include <string>
//#include <vector>
//#include <unordered_set>
//#include <unordered_map>
//#include <regex>
//#include <filesystem>
//#include <random>
//#include <algorithm>
//
//const std::regex projectRe("Project\\(\"\\{([^\"]+)\\}\"\\) = \"([^\"]+)\", \"([^\"]+)\", \"\\{([^\"]+)\\}\"");
//
//std::string GenerateGuid() {
//	static std::random_device rd;
//	static std::mt19937 gen(rd());
//	static std::uniform_int_distribution<> dis(0, 15);
//
//	const char *hex = "0123456789ABCDEF";
//	std::string guid = "{";
//	for (int i = 0; i < 32; ++i) {
//		if (i == 8 || i == 12 || i == 16 || i == 20) guid += "-";
//		guid += hex[dis(gen)];
//	}
//	guid += "}";
//	return guid;
//}
//
//std::vector<std::string> ReadLines(const std::string &path) {
//	std::ifstream in(path);
//	std::vector<std::string> lines;
//	std::string line;
//	while (std::getline(in, line)) lines.push_back(line);
//	return lines;
//}
//
//void WriteLines(const std::string &path, const std::vector<std::string> &lines) {
//	std::ofstream out(path);
//	for (const auto &line : lines) out << line << "\n";
//}
//
//std::string EscapeForRegex(const std::string& text) {
//	std::string escaped;
//	for (char c : text) {
//		if (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')' ||
//			c == '.' || c == '+' || c == '*' || c == '?' || c == '^' || c == '$' || c == '\\') {
//			escaped += '\\';
//		}
//		escaped += c;
//	}
//	return escaped;
//}
//
//std::unordered_set<std::string> ExtractGuids(const std::vector<std::string> &lines) {
//	std::unordered_set<std::string> guids;
//	std::smatch m;
//	for (const auto &line : lines) {
//		if (std::regex_search(line, m, projectRe)) guids.insert(m[1]);
//	}
//	return guids;
//}
//
//int FindInsertIndex(const std::vector<std::string> &lines) {
//	for (int i = lines.size() - 1; i >= 0; --i) {
//		if (lines[i].find("EndProject") != std::string::npos)
//			return i + 1;
//	}
//	return lines.size();
//}
//
//std::vector<std::string> ExtractProjectConfigurations(const std::vector<std::string> &lines, const std::string &oldGuid, const std::string &newGuid) {
//	std::vector<std::string> result;
//	bool inBlock = false;
//	for (const auto &line : lines) {
//		if (line.find("GlobalSection(ProjectConfigurationPlatforms) = postSolution") != std::string::npos)
//			inBlock = true;
//		else if (inBlock && line.find("EndGlobalSection") != std::string::npos)
//			break;
//		else if (inBlock && line.find(oldGuid) != std::string::npos)
//			result.push_back(std::regex_replace(line, std::regex(EscapeForRegex(oldGuid)), newGuid));
//	}
//	return result;
//}
//
//int main(int argc, char *argv[]) {
//	if (argc < 4) {
//		std::cout << "Usage: SlnMerger <target.sln> <source.sln> <ProjectName1> [ProjectName2] ...\n";
//		return 1;
//	}
//
//	// Support for listing available projects
//	bool listMode = false;
//
//	std::string targetPath = argv[1];
//	std::string sourcePath = argv[2];
//	std::unordered_set<std::string> selectedProjects;
//	for (int i = 3; i < argc; ++i) {
//		std::string arg = argv[i];
//		if (arg == "--list") {
//			listMode = true;
//		}
//		else {
//			selectedProjects.insert(arg);
//		}
//	}
//	try {
//		std::cout << "Reading target solution: " << targetPath << std::endl;
//		auto targetLines = ReadLines(targetPath);
//
//		std::cout << "Reading source solution: " << sourcePath << std::endl;
//		auto sourceLines = ReadLines(sourcePath);
//
//		std::cout << "Extracting existing GUIDs..." << std::endl;
//		auto existingGuids = ExtractGuids(targetLines);
//
//		std::smatch match;
//
//		std::unordered_map<std::string, std::string> guidMap;
//		std::vector<std::string> newConfigLines;
//		int insertIndex = FindInsertIndex(targetLines);
//
//		if (listMode) {
//			std::cout << "Projects in source solution:" << std::endl;
//			for (const auto& line : sourceLines) {
//				if (std::regex_search(line, match, projectRe)) {
//					if (match.size() >= 3)
//						std::cout << " - " << match[2] << std::endl;
//					else
//						std::cout << " - (incomplete match) => " << match[0] << std::endl;
//				}
//			}
//			system("Pause");
//			return 0;
//		}
//
//		std::cout << "Merging selected projects..." << std::endl;
//		for (size_t i = 0; i < sourceLines.size(); ++i) {
//			if (std::regex_search(sourceLines[i], match, projectRe)) {
//				std::string typeGuid = "{" + match[1].str() + "}";
//				std::string name = match[2];
//				std::string path = match[3];
//				std::string oldGuid = "{" + match[4].str() + "}";
//
//				std::filesystem::path targetDir = std::filesystem::absolute(targetPath).parent_path();
//				std::filesystem::path sourceDir = std::filesystem::absolute(sourcePath).parent_path();
//				std::filesystem::path projectPath = std::filesystem::absolute(sourceDir / path);
//				std::string newPath = std::filesystem::relative(projectPath, targetDir).generic_string();
//
//				if (!selectedProjects.count(name)) continue;
//
//				std::cout << "Adding project: " << name << " (" << newPath << ")" << std::endl;
//
//				std::string newGuid = oldGuid;
//				while (existingGuids.count(newGuid)) {
//					std::cout << "GUID conflict detected for " << oldGuid << ", generating new GUID..." << std::endl;
//					newGuid = GenerateGuid();
//				}
//				existingGuids.insert(newGuid);
//				guidMap[oldGuid] = newGuid;
//
//				std::vector<std::string> block;
//				block.push_back("Project(\"" + typeGuid + "\") = \"" + name + "\", \"" + newPath + "\", \"" + newGuid + "\"");
//				while (++i < sourceLines.size() && sourceLines[i].find("EndProject") == std::string::npos)
//					block.push_back(sourceLines[i]);
//				block.push_back("EndProject");
//
//				targetLines.insert(targetLines.begin() + insertIndex, block.begin(), block.end());
//				insertIndex += block.size();
//
//				auto configs = ExtractProjectConfigurations(sourceLines, oldGuid, newGuid);
//				newConfigLines.insert(newConfigLines.end(), configs.begin(), configs.end());
//			}
//		}
//
//		std::cout << "Injecting project configurations..." << std::endl;
//		auto globalEnd = std::find(targetLines.rbegin(), targetLines.rend(), "EndGlobal").base();
//		targetLines.insert(globalEnd, newConfigLines.begin(), newConfigLines.end());
//
//		std::cout << "Writing updated solution: " << targetPath << std::endl;
//		WriteLines(targetPath, targetLines);
//		std::cout << "Done.\n";
//	}
//	//catch (const std::regex_error& e) {
//	//	std::cerr << "Regex error in ExtractGuids: " << e.what() << std::endl;
//	//}
//	catch (int) {
//	}
//
//	system("Pause");
//	return 0;
//}



#include "Helpers/Logger.h"
#include "SolutionMerger.h"
#include <unordered_set>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
#ifdef _DEBUG
	const char* debugArgs[] = {
		"Path_to_executable",
		"d:\\WORK\\C++\\KeyboardLayerService\\KeyboardLayerService.sln",
		"d:\\WORK\\C++\\KeyboardLayerService\\UtilityHelpersLib\\UtilityHelpersLib.sln",
		//"-f", "3rd",
		"-p", "Helpers.Raw"
	};
	argc = sizeof(debugArgs) / sizeof(debugArgs[0]);
	argv = const_cast<char**>(debugArgs);
#endif

	if (argc < 4) {
		std::cout << "Usage: SlnMerger <target.sln> <source.sln> [-f FolderName]* [-p ProjectName]*\n";
		return 1;
	}

	// Init logger
	H::Flags<lg::InitFlags> loggerInitFlags =
		lg::InitFlags::DefaultFlags |
		lg::InitFlags::EnableLogToStdout;// |
		//lg::InitFlags::CreateInExeFolderForDesktop;

	lg::DefaultLoggers::Init(L"D:\\SolutionInjector.log", loggerInitFlags);

	std::string targetSln = argv[1];
	std::string sourceSln = argv[2];
	std::unordered_set<std::string> folders;
	std::unordered_set<std::string> projects;

	for (int i = 3; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-f" && i + 1 < argc) {
			folders.insert(argv[++i]);
		} else if (arg == "-p" && i + 1 < argc) {
			projects.insert(argv[++i]);
		} else {
			std::cerr << "Unknown or incomplete argument: " << arg << "\n";
		}
	}

	Core::SolutionMerger merger;
	merger.Merge(targetSln, sourceSln, projects, folders);

#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}
