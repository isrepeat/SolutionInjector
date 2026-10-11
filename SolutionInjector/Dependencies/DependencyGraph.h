#pragma once
#include <string>
#include <map>
#include <set>

namespace Core {
	namespace Dependencies {
		struct DependencyGraph {
			std::map<std::string, std::set<std::string>> edges;
			std::set<std::string> issues;

			std::set<std::string> Reachable(const std::set<std::string>& roots) const;
		};
	}
}