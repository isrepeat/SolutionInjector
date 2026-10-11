#include "DependencyGraph.h"

#include <queue>

namespace Core {
	namespace Dependencies {
		std::set<std::string> DependencyGraph::Reachable(const std::set<std::string>& roots) const {
			// 1. Начинаем обход с проектов, которые должны остаться.
			std::queue<std::string> pending;
			std::set<std::string> reachable;
			for (const auto& root : roots) {
				pending.push(root);
			}

			// 2. Обходим зависимости; повторно посещённые узлы пропускаем, включая циклы.
			while (!pending.empty()) {
				const auto current = pending.front();
				pending.pop();
				if (!reachable.insert(current).second) {
					continue;
				}
				const auto found = edges.find(current);
				if (found != edges.end()) {
					for (const auto& target : found->second) {
						pending.push(target);
					}
				}
			}
			return reachable;
		}
	}
}