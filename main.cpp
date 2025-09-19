#include <Helpers/Std/Extensions/rangesEx.h>
#include <Helpers/CommandLineParser.h>
#include <Helpers/Logger.h>

#include "SolutionFileAnalyzer.h"
#include "SolutionStructure.h"

#include <unordered_set>
#include <iostream>
#include <string>

struct CmdArgs {
	using CommandLineParser_t = H::CommandLineParserA;
	using string_t = typename CommandLineParser_t::string_t;

	std::filesystem::path sourceSlnPath;
	std::filesystem::path targetSlnPath;
	bool normalize = false;
	std::unordered_set<string_t> folders;
	std::unordered_set<string_t> projects;
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

	static void MapParsedResults(const CommandLineParser_t& cmdLineParser, CmdArgs& out) {
		const auto& positional = cmdLineParser.GetPositional();

		if (positional.size() > 0) {
			out.sourceSlnPath = positional[0];
		}
		if (positional.size() > 1) {
			out.targetSlnPath = positional[1];
		}

		out.normalize = cmdLineParser.Has("--normalize");
		
		const auto& folders = cmdLineParser.GetAll("-f");
		out.folders = std::unordered_set<string_t>{ folders.begin(), folders.end() };

		const auto& projects = cmdLineParser.GetAll("-p");
		out.projects = std::unordered_set<string_t>{ projects.begin(), projects.end() };

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


void Merge(
	const CmdArgs& cmdArgs,
	const std::unique_ptr<Core::SolutionStructure>& sourceSlnStructure,
	std::unique_ptr<Core::SolutionStructure>& targetSlnStructure
);


int main(int argc, char* argv[]) {
#ifdef _DEBUG
	const char* debugArgs[] = {
		"Path_to_executable",
		"d:\\WORK\\C++\\Cpp\\UtilityHelpersLib\\UtilityHelpersLib.sln",
		"d:\\WORK\\C++\\Cpp\\Cpp.sln",
		"--normalize",
		//"--rootName", "UtilityHelpersLib [submodule]",
		//"-f", "3rdParty",
		//"-f", "Helpers",
		//"-p", "ComAPI",
		//"-p", "ComAPI.Shared",
		//"-p", "Helpers.Raw",
		//"-p", "Helpers.Shared",
		//"-p", "Helpers.Includes",
		//"-f", "HelpersCs",
		//"-p", "HelpersCs",
		//"-p", "HelpersCs.Visual",
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
		const auto cmdArgs = H::CommandLineParserA::ParseTo<CmdArgs>(argc, argv);
		
		auto sourceSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdArgs.sourceSlnPath);
		//LOG_DEBUG_D("sourceSlnStructure:");
		//sourceSlnStructure->LogSerializedSolution();

		auto targetSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdArgs.targetSlnPath);
		LOG_DEBUG_D("targetSlnStructure:");
		targetSlnStructure->LogSerializedSolution();

		//
		// ░ Normalization
		// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
		//
		if (cmdArgs.normalize) {
			// Просто пересохраняем target.sln, чтобы при сериализации произошла нормализация строк.
			targetSlnStructure->Save();
			LOG_DEBUG_D("Normalized (re-saved) target: {}", cmdArgs.targetSlnPath);
			return 0;
		}

		//
		// ░ Merging
		// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 
		//
		Merge(
			cmdArgs,
			sourceSlnStructure,
			targetSlnStructure
		);

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


void Merge(
	const CmdArgs& cmdArgs,
	const std::unique_ptr<Core::SolutionStructure>& sourceSlnStructure,
	std::unique_ptr<Core::SolutionStructure>& targetSlnStructure
) {
	using namespace Core;

	if (cmdArgs.projects.empty() && cmdArgs.folders.empty()) {
		LOG_DEBUG_D("No -p / -f filters provided. Nothing to merge.");
		return;
	}

	// Собираем выбранные SolutionFolder's.
	auto selectedSolutionFolders =
		sourceSlnStructure->GetView()->mapGuidToSolutionNode
		| std::ranges::views::values
		| std::ranges::views::filter(
			[&](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				if (solutionNode.Is<Model::Project::SolutionFolder>() &&
					cmdArgs.folders.contains(solutionNode->name)
					) {
					return true;
				}
				return false;
			})
		| std::ex::ranges::views::to<std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>>();

	// Собираем множество дочерних проектов из selectedFolders, чтоб далее исключить их 
	// из cmdArgs.projects, т.к. эти проекты будут вставлены все равно рекурсивно.
	auto projectNamesUnderSelectedFolders =
		selectedSolutionFolders
		| std::ex::ranges::views::flatten_tree(
			[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				if (auto solutionFolder = solutionNode.As<Model::Project::SolutionFolder>()) {
					return solutionFolder->GetChildren();
				}
				return std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>{};
			})
		| std::ranges::views::filter(
			[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				return solutionNode.Is<Model::Project::ProjectNode>();
			})
		| std::ranges::views::transform(
			[](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				return solutionNode->name;
			})
		| std::ex::ranges::views::to<std::unordered_set<std::string>>();

	// Собираем выбранные ProjectNode's, исключаем те что уже находятся в projectNamesUnderSelectedFolders.
	auto selectedProjectNodes =
		sourceSlnStructure->GetView()->mapGuidToSolutionNode
		| std::ranges::views::values
		| std::ranges::views::filter(
			[&](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				if (solutionNode.Is<Model::Project::ProjectNode>() &&
					cmdArgs.projects.contains(solutionNode->name) &&
					!projectNamesUnderSelectedFolders.contains(solutionNode->name)
					) {
					return true;
				}
				return false;
			})
		| std::ex::ranges::views::to<std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>>();

	auto srcSolutionNodesToInsert =
		selectedSolutionFolders
		| std::ex::ranges::views::concat(selectedProjectNodes)
		| std::ex::ranges::views::to<std::vector<std::ex::shared_ptr<Model::Project::SolutionNode>>>();

	for (const auto& srcSolutionNode : srcSolutionNodesToInsert) {
		targetSlnStructure->AddProjectBlock(srcSolutionNode->GetProjectBlock());
	}

	const auto targetSolutionInfo = targetSlnStructure->GetSolutionInfo();

	auto rootFolderName = cmdArgs.rootName
		? *cmdArgs.rootName
		: std::format("[Inserted from {}.sln]", targetSolutionInfo.solutionFile.stem().string());

	auto rootFolder = targetSlnStructure->MakeFolder(rootFolderName);

	// TODO: вообще после вставки могут поменяться guids у вставленных solutionNodes,
	// поэтому нужно вытащить вставленные узлы уже из targetSlnStructure view;

	// TODO: добавь возможность в SolutionStructure удалять solution configuration
	for (const auto& srcSolutionNode : srcSolutionNodesToInsert) {
		targetSlnStructure->AttachChild(rootFolder->guid, srcSolutionNode->guid);
	}

	LOG_DEBUG_D("targetSlnStructure (new):");
	targetSlnStructure->LogSerializedSolution();
	targetSlnStructure->Save();

	LOG_DEBUG_D("Merging finished. Target saved.");
}