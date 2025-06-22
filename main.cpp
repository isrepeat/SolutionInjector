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
		"d:\\WORK\\C++\\KeyboardLayerService\\UtilityHelpersLib\\UtilityHelpersLib.sln",
		"d:\\WORK\\C++\\KeyboardLayerService\\KeyboardLayerService.sln",
		"-f", "Spdlog",
		//"-p", "Helpers.Raw"
	};
	argc = sizeof(debugArgs) / sizeof(debugArgs[0]);
	argv = const_cast<char**>(debugArgs);
#endif

	// Init logger
	H::Flags<lg::InitFlags> loggerInitFlags =
		lg::InitFlags::DefaultFlags |
		lg::InitFlags::EnableLogToStdout;

	lg::DefaultLoggers::Init(L"D:\\SolutionInjector.log", loggerInitFlags);

	if (argc < 4) {
		LOG_RAW("Usage: SlnMerger <target.sln> <source.sln> [-f FolderName]* [-p ProjectName]*");
		return 1;
	}

	std::string sourceSlnPath = argv[1];
	std::string targetSlnPath = argv[2];
	std::unordered_set<std::string> projectsToInsert;
	std::unordered_set<std::string> foldersToInsert;

	for (int i = 3; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-f" && i + 1 < argc) {
			foldersToInsert.insert(argv[++i]);
		} else if (arg == "-p" && i + 1 < argc) {
			projectsToInsert.insert(argv[++i]);
		} else {
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

	LOG_DEBUG_D("Target sln before:");
	targetSlnStructure.LogSerializedSolution();

	auto solutionMerger = Core::SolutionMerger{
		sourceSlnStructure,
		targetSlnStructure
	};

	bool isMergeSuccessfull = solutionMerger.Merge(
		projectsToInsert,
		foldersToInsert,
		Core::SolutionMerger::MergeFlags::None
	);
	if (!isMergeSuccessfull) {
		LOG_ERROR_D("Merge failed");
		return 1;
	}

	LOG_DEBUG_D("Target sln after:");
	targetSlnStructure.LogSerializedSolution();
	//targetSlnStructure.Save();

#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}
