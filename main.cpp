#include <Helpers/Logger.h>
#include "SolutionStructure.h"
#include "SolutionMerger.h"
#include <unordered_set>
#include <iostream>
#include <string>


int main(int argc, char* argv[]) {
#ifdef _DEBUG
	const char* debugArgs[] = {
		"Path_to_executable",
		"d:\\WORK\\C++\\Cpp\\UtilityHelpersLib\\UtilityHelpersLib.sln",
		"d:\\WORK\\C++\\Cpp\\Cpp.sln",
		//"-f", "3rdParty",
		//"-p", "ComAPI",
		//"-p", "ComAPI.Shared",
		//"-p", "Helpers.Raw",
		//"-p", "Helpers.Shared",
		//"-p", "Helpers.Includes",
		//"-p", "HelpersCs",
		"-p", "HelpersCs.Visual",
	};
	argc = sizeof(debugArgs) / sizeof(debugArgs[0]);
	argv = const_cast<char**>(debugArgs);
#endif

	// Init logger
	H::Flags<lg::InitFlags> loggerInitFlags =
		lg::InitFlags::DefaultFlags |
		lg::InitFlags::EnableLogToStdout |
		lg::InitFlags::CreateInExeFolderForDesktop;

	lg::DefaultLoggers::Init(L"SolutionInjector.log", loggerInitFlags);

	LOG_DEBUG_D("Received arguments (argc = {}):", argc);
	for (int i = 0; i < argc; ++i) {
		LOG_DEBUG_D("  argv[{}] = '{}'", i, argv[i]);
	}

	if (argc < 4) {
		LOG_DEBUG_D("Usage: SlnMerger <target.sln> <source.sln> [-f FolderName]* [-p ProjectName]*");
		return 1;
	}

	std::string sourceSlnPath = argv[1];
	std::string targetSlnPath = argv[2];
	std::unordered_set<std::string> projectsToInsert;
	std::unordered_set<std::string> foldersToInsert;
	std::optional<std::string> rootFolderNameOpt;

	for (int i = 3; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-f" && i + 1 < argc) {
			foldersToInsert.insert(argv[++i]);
		}
		else if (arg == "-p" && i + 1 < argc) {
			projectsToInsert.insert(argv[++i]);
		}
		else if (arg == "--rootName" && i + 1 < argc) {
			rootFolderNameOpt = argv[++i];
		}
		else {
			LOG_ERROR_D("Unknown or incomplete argument: {}", arg);
		}
	}

	auto sourceSlnStructure = Core::SolutionStructure{ sourceSlnPath };
	if (!sourceSlnStructure.Parse()) {
		LOG_ERROR_D("Failed to parse source solution");
		return 1;
	}

	auto targetSlnStructure = Core::SolutionStructure{ targetSlnPath };
	if (!targetSlnStructure.Parse()) {
		LOG_ERROR_D("Failed to parse target solution");
		return 1;
	}

	LOG_DEBUG_D("targetSlnStructure:");
	targetSlnStructure.LogSerializedSolution();
	targetSlnStructure.Save();

	//auto solutionMerger = Core::SolutionMerger{
	//	sourceSlnStructure
	//};

	//auto targetSlnStructureNew = solutionMerger.Merge(
	//	targetSlnStructure,
	//	projectsToInsert,
	//	foldersToInsert,
	//	Core::SolutionMerger::MergeFlags::None,
	//	rootFolderNameOpt
	//);
	//if (!targetSlnStructureNew) {
	//	LOG_ERROR_D("Merge failed");
	//	return 1;
	//}

	//LOG_DEBUG_D("targetSlnStructure new:");
	//targetSlnStructureNew->LogSerializedSolution();
	//targetSlnStructureNew->Save();

#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}