#pragma once
#include "../Dependencies/DependencyGraph.h"
#include "../SolutionStructure.h"

#include <string>
#include <vector>
#include <set>

namespace Core {
	namespace Operations {
		struct SolutionCleanupPlan {
			std::set<std::string> removedProjects;
			std::set<std::string> retainedProjects;
			std::set<std::string> issues;
			std::vector<H::Guid> removedFolders;
		};

		class SolutionCleanupPlanner {
		public:
			static SolutionCleanupPlan Create(
				const SolutionStructure& solutionStructure,
				const Dependencies::DependencyGraph& dependencyGraph,
				const std::string& scope,
				const std::vector<std::string>& roots
			);
			static void Apply(SolutionStructure& solutionStructure, const SolutionCleanupPlan& solutionCleanupPlan);
		};
	}
}