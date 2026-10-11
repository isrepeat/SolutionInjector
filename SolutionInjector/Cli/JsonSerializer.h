#pragma once
#include "../Dependencies/DependencyGraph.h"
#include "../Operations/SolutionCleanupPlanner.h"

#include <string>

namespace Core {
	namespace Cli {
		class JsonSerializer {
		public:
			static std::string SerializeCleanupReport(
				const Dependencies::DependencyGraph& dependencyGraph,
				const Operations::SolutionCleanupPlan& solutionCleanupPlan,
				bool applied
			);
		};
	}
}