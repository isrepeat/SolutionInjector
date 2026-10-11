#include <Helpers/Std/Extensions/rangesEx.h>
#include <Helpers/CommandLineParser.h>
#include <Helpers/Pipeline.h>
#include <Helpers/Logger.h>

#include "Cli/CleanupCommand.h"
#include "SolutionFileAnalyzer.h"
#include "SolutionStructure.h"

#include <unordered_set>
#include <system_error>
#include <iostream>
#include <string>

//
// ░ CmdArgs
// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
//
struct CmdArgs {
	using CommandLineParser_t = H::CommandLineParserA;
	using string_t = typename CommandLineParser_t::string_t;

	std::filesystem::path sourceSlnPath;
	std::filesystem::path targetSlnPath;
	bool normalize = false;
	std::optional<string_t> rootName;
	std::unordered_set<string_t> folders;
	std::unordered_set<string_t> projects;
	std::unordered_set<string_t> solutionConfigKeysToRemove;

	static std::vector<typename CommandLineParser_t::FlagDesc> Description() {
		using EVC = typename CommandLineParser_t::ExpectedValuesCount;
		return {
			{ "--normalize", EVC::None, false },
			{ "--rootName", EVC::Single, false },
			{ "-f", EVC::Multiple, false },
			{ "-p", EVC::Multiple, false },
			{ "-rmCfg", EVC::Multiple, false }, // Удаляем конфигурацию решения: "<Cfg>|<Platform>".
		};
	}

	static void MapParsedResults(const CommandLineParser_t& cmdLineParser, CmdArgs& out) {
		const auto& positional = cmdLineParser.GetPositional();

		if (positional.size() > 0) {
			// NOTE: std::filesystem::canonical() информацию о текущем рабочем каталоге,
			//       которая наследуется процессом из caller'а. Т.е. если программа запускается
			//       через PowerShell / Cmd то они могут менять рабочий каталог.
			out.sourceSlnPath = std::filesystem::canonical(positional[0]);
		}
		if (positional.size() > 1) {
			out.targetSlnPath = std::filesystem::canonical(positional[1]);
		}

		out.normalize = cmdLineParser.Has("--normalize");
		out.rootName = cmdLineParser.Get("--rootName");

		const auto& folders = cmdLineParser.GetAll("-f");
		out.folders = std::unordered_set<string_t>{ folders.begin(), folders.end() };

		const auto& projects = cmdLineParser.GetAll("-p");
		out.projects = std::unordered_set<string_t>{ projects.begin(), projects.end() };

		const auto& solutionConfigKeysToRemove = cmdLineParser.GetAll("-rmCfg");
		out.solutionConfigKeysToRemove = std::unordered_set<string_t>{
			solutionConfigKeysToRemove.begin(),
			solutionConfigKeysToRemove.end()
		};
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


//
// ░ TransformSolutionPipeline
// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
//
class TransformSolutionPipeline : public H::Pipeline<TransformSolutionPipeline> {
public:
	TransformSolutionPipeline(
		const CmdArgs& cmdArgs,
		const std::unique_ptr<Core::SolutionStructure>& sourceSlnStructure,
		std::unique_ptr<Core::SolutionStructure>& targetSlnStructure
	)
		: cmdArgs{ cmdArgs }
		, sourceSlnStructure{ sourceSlnStructure }
		, targetSlnStructure{ targetSlnStructure } {
	}

public:
	std::span<const StepFn_t> GetPipelineSteps() const override {
		static constexpr StepFn_t steps[] = {
			&TransformSolutionPipeline::NormalizeStep,
			&TransformSolutionPipeline::RemoveConfigsStep,
			&TransformSolutionPipeline::MergeStep,
		};
		return std::span<const StepFn_t>{ steps };
	}

private:
	H::PipelineResult NormalizeStep() {
		if (this->cmdArgs.normalize) {
			// Просто выходим чтобы сохранить target.sln как есть, чтобы прошла нормализация.
			// Игнорируем дальнейшие шаги конвеера, т.к. нормализация в приоритете.
			LOG_DEBUG_D("Normalized");
			return H::PipelineResult::EarlyExit();
		}

		return H::PipelineResult::Continue();
	}

	H::PipelineResult RemoveConfigsStep() {
		if (!this->cmdArgs.solutionConfigKeysToRemove.empty()) {
			this->targetSlnStructure->RemoveSolutionConfigurationsByDeclaredKeys(
				this->cmdArgs.solutionConfigKeysToRemove
			);
		}

		return H::PipelineResult::Continue();
	}

	H::PipelineResult MergeStep() {
		using namespace Core;

		if (this->cmdArgs.projects.empty() && cmdArgs.folders.empty()) {
			LOG_DEBUG_D("No -p / -f filters provided. Nothing to merge.");
			return H::PipelineResult::Continue();
		}

		// Собираем выбранные SolutionFolder's.
		auto selectedSolutionFolders =
			this->sourceSlnStructure->GetView()->mapGuidToSolutionNode
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
			this->sourceSlnStructure->GetView()->mapGuidToSolutionNode
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
			this->targetSlnStructure->AddProjectBlock(srcSolutionNode->GetProjectBlock());
		}

		const auto targetSolutionInfo = this->targetSlnStructure->GetSolutionInfo();

		auto rootFolderName = this->cmdArgs.rootName
			? *this->cmdArgs.rootName
			: std::format("[Inserted from {}.sln]", targetSolutionInfo.solutionFile.stem().string());

		auto rootFolder = this->targetSlnStructure->MakeFolder(rootFolderName);

		// TODO: вообще после вставки могут поменяться guids у вставленных solutionNodes,
		// поэтому нужно вытащить вставленные узлы уже из targetSlnStructure view;

		for (const auto& srcSolutionNode : srcSolutionNodesToInsert) {
			this->targetSlnStructure->AttachChild(rootFolder->guid, srcSolutionNode->guid);
		}

		LOG_DEBUG_D("Merging finished.");
		return H::PipelineResult::Continue();
	}

private:
	const CmdArgs& cmdArgs;
	const std::unique_ptr<Core::SolutionStructure>& sourceSlnStructure;
	std::unique_ptr<Core::SolutionStructure>& targetSlnStructure;
};


//
// ░ Main
// ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
//
int main(int argc, char* argv[]) {
	auto descriptions = CmdArgs::Description();
	for (auto flagDesc : Core::Cli::CleanupArgs::Description()) {
		flagDesc.required = false;
		descriptions.push_back(flagDesc);
	}
	descriptions.push_back({ "--version", H::CommandLineParserA::ExpectedValuesCount::None, false });
	H::CommandLineParserA commandLineParser{ descriptions };
	try {
		commandLineParser.Parse(argc, argv);
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		return 1;
	}
	const auto& positional = commandLineParser.GetPositional();
	const bool cleanupCommand = !positional.empty() && (positional[0] == "analyze" || positional[0] == "prune");
	if (commandLineParser.Has("--version")) {
		if (argc != 2) {
			std::cerr << "--version must be used alone\n";
			return 1;
		}
		std::cout << "SolutionInjector (analyze, prune, scoped-cleanup)\n";
		return 0;
	}

	// Инициализируем логирование; JSON-команды не пишут служебные логи в stdout.
	H::Flags<lg::InitFlags> loggerInitFlags =
		lg::InitFlags::DefaultFlags |
		lg::InitFlags::CreateInExeFolderForDesktop;

	if (!cleanupCommand) {
		loggerInitFlags |= lg::InitFlags::EnableLogToStdout;
	}
	lg::DefaultLoggers::Init(L"SolutionInjector.log", loggerInitFlags);
	if (cleanupCommand) {
		try {
			const auto cleanupArgs = H::CommandLineParserA::ParseTo<Core::Cli::CleanupArgs>(argc, argv);
			return Core::Cli::RunCleanupCommand(cleanupArgs);
		} catch (const std::exception& exception) {
			std::cerr << exception.what() << '\n';
			return 1;
		}
	}

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
		cmdArgs.Validate();

		auto sourceSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdArgs.sourceSlnPath);
		//LOG_DEBUG_D("sourceSlnStructure:");
		//sourceSlnStructure->LogSerializedSolution();

		auto targetSlnStructure = Core::SolutionFileAnalyzer::BuildSolutionStructure(cmdArgs.targetSlnPath);
		LOG_DEBUG_D("targetSlnStructure:");
		targetSlnStructure->LogSerializedSolution();

		TransformSolutionPipeline transformSolutionPipeline{
			cmdArgs,
			sourceSlnStructure,
			targetSlnStructure
		};

		auto errorCode = transformSolutionPipeline.Run();
		if (!errorCode) {
			LOG_DEBUG_D("targetSlnStructure (new):");
			targetSlnStructure->LogSerializedSolution();
			targetSlnStructure->Save();
		}

		return errorCode ? 1 : 0;
	}
	catch (const std::exception& ex) {
		LOG_ERROR_D("exception = {}", ex.what());
		return 1;
	}
}