#include "SolutionCleanupPlanner.h"

#include "../Helpers/TextHelpers.h"

#include <functional>
#include <stdexcept>

namespace Core {
	namespace Operations {
		SolutionCleanupPlan SolutionCleanupPlanner::Create(
			const SolutionStructure& solutionStructure,
			const Dependencies::DependencyGraph& dependencyGraph,
			const std::string& scope,
			const std::vector<std::string>& roots
		) {
			// 1. Переносим проблемы анализа и проверяем дерево вложенности.
			SolutionCleanupPlan solutionCleanupPlan;
			solutionCleanupPlan.issues = dependencyGraph.issues;
			const auto view = solutionStructure.GetView();
			for (const auto& entry : view->mapGuidToSolutionNode) {
				std::set<std::string> parents;
				auto current = entry.second;
				while (current) {
					if (!parents.insert(current->guid.ToString()).second) {
						throw std::runtime_error("Cycle in NestedProjects");
					}
					current = current->GetParent();
				}
			}

			// 2. Находим единственную папку области очистки.
			std::ex::shared_ptr<Model::Project::SolutionFolder> scopeFolder;
			for (const auto& entry : view->mapGuidToSolutionNode) {
				if (entry.second.Is<Model::Project::SolutionFolder>() &&
					(entry.second->name == scope || entry.first.ToString() == Helpers::ToUpperAscii(scope))) {
					if (scopeFolder) {
						throw std::runtime_error("Ambiguous scope; use folder GUID");
					}
					scopeFolder = entry.second.Cast<Model::Project::SolutionFolder>();
				}
			}
			if (!scopeFolder) {
				throw std::runtime_error("Scope folder not found");
			}

			// 3. Собираем узлы, которые разрешено удалять.
			std::set<std::string> managed;
			std::set<std::string> visited;
			std::function<void(const std::ex::shared_ptr<Model::Project::SolutionNode>&)> collect;
			collect = [&](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				if (!visited.insert(solutionNode->guid.ToString()).second) {
					throw std::runtime_error("Cycle in solution folders");
				}
				managed.insert(solutionNode->guid.ToString());
				if (const auto folder = solutionNode.As<Model::Project::SolutionFolder>()) {
					for (const auto& child : folder->GetChildren()) {
						collect(child);
					}
				}
			};
			collect(scopeFolder);

			// 4. Сохраняем проекты вне области и явно указанные корни.
			std::set<std::string> graphRoots;
			for (const auto& entry : view->mapGuidToSolutionNode) {
				if (entry.second.Is<Model::Project::ProjectNode>() && !managed.count(entry.first.ToString())) {
					graphRoots.insert(entry.first.ToString());
				}
			}
			for (const auto& root : roots) {
				size_t count = 0;
				for (const auto& entry : view->mapGuidToSolutionNode) {
					if (entry.second.Is<Model::Project::ProjectNode>() &&
						(entry.second->name == root || entry.first.ToString() == Helpers::ToUpperAscii(root))) {
						graphRoots.insert(entry.first.ToString());
						++count;
					}
				}
				if (count != 1) {
					throw std::runtime_error("Root missing or ambiguous: " + root);
				}
			}

			// 5. Отделяем используемые проекты от недостижимых.
			const auto reachable = dependencyGraph.Reachable(graphRoots);
			for (const auto& guid : managed) {
				const auto node = view->mapGuidToSolutionNode.at(H::Guid::Parse(guid));
				if (node.Is<Model::Project::ProjectNode>()) {
					(reachable.count(guid) ? solutionCleanupPlan.retainedProjects : solutionCleanupPlan.removedProjects).insert(guid);
				}
			}

			// 6. Вычисляем опустевшие папки снизу вверх, сохраняя папки с секциями.
			std::function<bool(const std::ex::shared_ptr<Model::Project::SolutionNode>&)> emptyAfterRemoval;
			emptyAfterRemoval = [&](const std::ex::shared_ptr<Model::Project::SolutionNode>& solutionNode) {
				if (solutionNode.Is<Model::Project::ProjectNode>()) {
					return solutionCleanupPlan.removedProjects.count(solutionNode->guid.ToString()) != 0;
				}
				const auto folder = solutionNode.Cast<Model::Project::SolutionFolder>();
				bool empty = folder->GetProjectBlock()->sectionMap.empty();
				for (const auto& child : folder->GetChildren()) {
					// Всегда обходим детей, даже если сама папка содержит SolutionItems.
					const bool childEmpty = emptyAfterRemoval(child);
					empty = empty && childEmpty;
				}
				if (empty) {
					solutionCleanupPlan.removedFolders.push_back(folder->guid);
				}
				return empty;
			};
			emptyAfterRemoval(scopeFolder);
			return solutionCleanupPlan;
		}

		void SolutionCleanupPlanner::Apply(SolutionStructure& solutionStructure, const SolutionCleanupPlan& solutionCleanupPlan) {
			// 1. Не применяем план с неразрешёнными зависимостями.
			if (!solutionCleanupPlan.issues.empty()) {
				throw std::runtime_error("Cleanup is blocked by unresolved dependencies");
			}

			// 2. Удаляем записи проектов через существующую модель решения.
			for (const auto& guid : solutionCleanupPlan.removedProjects) {
				solutionStructure.RemoveProjectBlock(H::Guid::Parse(guid));
			}

			// 3. Удаляем пустые папки после их детей.
			for (const auto& guid : solutionCleanupPlan.removedFolders) {
				solutionStructure.RemoveProjectBlock(guid);
			}
		}
	}
}