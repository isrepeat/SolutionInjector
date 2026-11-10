#include "../SolutionInjector/Cli/CleanupCommand.h"
#include "../SolutionInjector/Cli/SyncCommand.h"

int main(int argc, char* argv[]) {
	if (argc > 1 && std::string_view(argv[1]) == "sync") {
		return Core::Cli::RunSyncCommand(argc, argv);
	}
	try {
		const auto cleanupArgs = H::CommandLineParserA::ParseTo<Core::Cli::CleanupArgs>(argc, argv);
		return Core::Cli::RunCleanupCommand(cleanupArgs);
	} catch (const std::exception&) {
		return 1;
	}
}