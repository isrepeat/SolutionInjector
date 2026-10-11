#include "JsonSerializer.h"

#include "JsonScheme.h"

namespace Core {
	namespace Cli {
		std::string JsonSerializer::SerializeCleanupReport(
			const Dependencies::DependencyGraph& dependencyGraph,
			const Operations::SolutionCleanupPlan& solutionCleanupPlan,
			bool applied
		) {
			// 1. Переносим статус и списки из плана в JSON-схему.
			JsonScheme::CleanupReport cleanupReport;
			cleanupReport.blocked = !solutionCleanupPlan.issues.empty();
			cleanupReport.applied = applied;
			cleanupReport.removedProjects.assign(solutionCleanupPlan.removedProjects.begin(), solutionCleanupPlan.removedProjects.end());
			cleanupReport.issues.assign(solutionCleanupPlan.issues.begin(), solutionCleanupPlan.issues.end());

			// 2. Представляем GUID папок строками, а рёбра графа — отдельными записями.
			for (const auto& guid : solutionCleanupPlan.removedFolders) {
				cleanupReport.removedFolders.push_back(guid.ToString());
			}
			for (const auto& entry : dependencyGraph.edges) {
				for (const auto& target : entry.second) {
					cleanupReport.dependencies.push_back({ entry.first, target });
				}
			}

			// 3. Передаём экранирование и запись JSON библиотеке.
			return JS::serializeStruct(cleanupReport);
		}
	}
}