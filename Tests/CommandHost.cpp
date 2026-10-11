#include "../SolutionInjector/Cli/CleanupCommand.h"

int main(int argc, char* argv[]) {
	try {
		const auto cleanupArgs = H::CommandLineParserA::ParseTo<Core::Cli::CleanupArgs>(argc, argv);
		return Core::Cli::RunCleanupCommand(cleanupArgs);
	} catch (const std::exception&) {
		return 1;
	}
}