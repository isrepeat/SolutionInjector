#include <Helpers/CommandLineParser.h>
#include <Helpers/Logger.h>

#include "SolutionFileAnalyzer.h"
#include "SolutionStructure.h"
#include "SolutionMerger.h"

#include <unordered_set>
#include <iostream>
#include <string>

struct CmdOptions {
	using CommandLineParser_t = H::CommandLineParserA;
	using string_t = typename CommandLineParser_t::string_t;

	std::filesystem::path sourceSlnPath;
	std::filesystem::path targetSlnPath;
	bool normalize = false;
	std::vector<string_t> folders;
	std::vector<string_t> projects;
	std::optional<string_t> rootName;

	static std::vector<typename CommandLineParser_t::FlagDesc> Description() {
		using EVC = typename CommandLineParser_t::ExpectedValuesCount;
		return {
			{ "--normalize", EVC::None, false },
			{ "--rootName", EVC::Single, false },
			{ "-f", EVC::Multiple, false },
			{ "-p", EVC::Multiple, false },
		};
	}

	static void MapParsedResults(const CommandLineParser_t& cmdLineParser, CmdOptions& out) {
		const auto& positional = cmdLineParser.GetPositional();

		if (positional.size() > 0) {
			out.sourceSlnPath = positional[0];
		}
		if (positional.size() > 1) {
			out.targetSlnPath = positional[1];
		}

		out.normalize = cmdLineParser.Has("--normalize");
		out.folders = cmdLineParser.GetAll("-f");
		out.projects = cmdLineParser.GetAll("-p");
		out.rootName = cmdLineParser.Get("--rootName");
	}

	void Validate() const {
		if (this->sourceSlnPath.empty()) {
			throw std::runtime_error{
				"Missing <source.sln>"
			};
		}

		if (this->targetSlnPath.empty()) {
			throw std::runtime_error{
				"Missing <target.sln>"
			};
		}
	}
};


int main(int argc, char* argv[]) {
#ifdef _DEBUG
	const char* debugArgs[] = {
		"Path_to_executable",
		"d:\\WORK\\C++\\Cpp\\UtilityHelpersLib\\UtilityHelpersLib.sln",
		"d:\\WORK\\C++\\Cpp\\Cpp.sln",
		"--normalize",
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

	if (argc < 2) {
		LOG_DEBUG_D("Usage: SlnMerger <source.sln> <target.sln> [--normalize] [--rootName RootName] [-f FolderName]* [-p ProjectName]*");
		return 1;
	}

	try {
		auto cmdOptions = H::CommandLineParserA::ParseTo<CmdOptions>(argc, argv);
		
		auto sourceSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdOptions.sourceSlnPath);
		LOG_DEBUG_D("sourceSlnStructure:");
		sourceSlnStructure->LogSerializedSolution();

		auto targetSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdOptions.targetSlnPath);
		LOG_DEBUG_D("targetSlnStructure:");
		targetSlnStructure->LogSerializedSolution();

		//
		// ░ Normalization
		// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
		//
		if (cmdOptions.normalize) {
			// Просто пересохраняем target.sln, чтобы при сериализации произошла нормализация строк.
			targetSlnStructure->Save();
			LOG_DEBUG_D("Normalized (re-saved) target: {}", cmdOptions.targetSlnPath);
			return 0;
		}

		//
		// ░ Merging
		// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
		//

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

		return 0;
	}
	catch (const std::exception& ex) {
		LOG_ERROR_D("exception = {}", ex.what());
		return 1;
	}

#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}