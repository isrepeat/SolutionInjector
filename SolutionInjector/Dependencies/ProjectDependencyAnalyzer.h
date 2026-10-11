#pragma once
#include "../IO/FileSnapshot.h"
#include "../SolutionStructure.h"

#include "DependencyGraph.h"

namespace Core {
	namespace Dependencies {
		class ProjectDependencyAnalyzer {
		public:
			static DependencyGraph Analyze(const SolutionStructure& solutionStructure, IO::FileSnapshot& fileSnapshot);
		};
	}
}