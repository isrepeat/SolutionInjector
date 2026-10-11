#include "CleanupCommand.h"

#include "../Dependencies/ProjectDependencyAnalyzer.h"
#include "../Operations/SolutionCleanupPlanner.h"
#include "../Helpers/TextHelpers.h"
#include "../IO/FileSnapshot.h"
#include "../SolutionFileAnalyzer.h"

#include "JsonSerializer.h"

#include <stdexcept>
#include <iostream>

namespace Core {
	namespace Cli {
		int RunCleanupCommand(const CleanupArgs& cleanupArgs) {
			try {
				// 1. Проверяем параметры до чтения файлов.
				cleanupArgs.Validate();
				const auto& solutionPath = cleanupArgs.solutionPath;
				const auto& scope = cleanupArgs.scope;
				const auto& roots = cleanupArgs.roots;
				const bool apply = cleanupArgs.apply;
				const bool json = cleanupArgs.json;

				// 2. Фиксируем исходные файлы и загружаем модель решения.
				IO::FileSnapshot fileSnapshot;
				const auto original = fileSnapshot.Read(solutionPath);
				if (original.find("Microsoft Visual Studio Solution File, Format Version") == std::string::npos) {
					throw std::runtime_error("Invalid solution header");
				}
				auto solutionStructure = SolutionFileAnalyzer::BuildSolutionStructure(solutionPath);

				// 3. Собираем зависимости и рассчитываем план без изменения решения.
				const auto dependencyGraph = Dependencies::ProjectDependencyAnalyzer::Analyze(*solutionStructure, fileSnapshot);
				const auto solutionCleanupPlan = Operations::SolutionCleanupPlanner::Create(*solutionStructure, dependencyGraph, scope, roots);

				// 4. По запросу применяем допустимый план и сохраняем решение с резервной копией.
				bool applied = false;
				if (apply && solutionCleanupPlan.issues.empty()) {
					fileSnapshot.Verify();
					if (!solutionCleanupPlan.removedProjects.empty() || !solutionCleanupPlan.removedFolders.empty()) {
						Operations::SolutionCleanupPlanner::Apply(*solutionStructure, solutionCleanupPlan);
						auto content = solutionStructure->Serialize(std::nullopt);
						if (original.starts_with("\xEF\xBB\xBF")) {
							content.insert(0, "\xEF\xBB\xBF");
						}
						IO::ReplaceWithBackup(solutionPath, content, fileSnapshot);
					}
					applied = true;
				}

				// 5. Выводим результат в выбранном формате и возвращаем статус.
				if (json) {
					std::cout << JsonSerializer::SerializeCleanupReport(dependencyGraph, solutionCleanupPlan, applied) << '\n';
				} else {
					std::cout << "Cleanup candidates: " << solutionCleanupPlan.removedProjects.size() << " projects, "
						<< solutionCleanupPlan.removedFolders.size() << " folders\n";
					for (const auto& guid : solutionCleanupPlan.removedProjects) {
						std::cout << "REMOVE: " << guid << '\n';
					}
					for (const auto& guid : solutionCleanupPlan.retainedProjects) {
						std::cout << "KEEP: " << guid << '\n';
					}
					for (const auto& issue : solutionCleanupPlan.issues) {
						std::cout << "BLOCKED: " << issue << '\n';
					}
					if (applied) {
						std::cout << "Applied. Project and source files were not deleted.\n";
					}
				}
				return solutionCleanupPlan.issues.empty() ? 0 : 2;
			} catch (const std::exception& exception) {
				std::cerr << exception.what() << '\n';
				return 1;
			}
		}
	}
}