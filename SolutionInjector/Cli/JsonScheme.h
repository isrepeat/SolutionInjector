#pragma once
#include <JsonParser/json_struct/json_struct.h>

#include <string>
#include <vector>

namespace Core {
	namespace Cli {
		namespace JsonScheme {
			struct DependencyEntry {
				std::string from;
				std::string to;
	
				JS_OBJECT(JS_MEMBER(from), JS_MEMBER(to));
			};
	
			// Формат отчёта отделён от модели решения и логики очистки.
			struct CleanupReport {
				int schemaVersion = 1;
				bool blocked = false;
				bool applied = false;
				std::vector<std::string> removedProjects;
				std::vector<std::string> removedFolders;
				std::vector<DependencyEntry> dependencies;
				std::vector<std::string> issues;
	
				JS_OBJECT(
					JS_MEMBER(schemaVersion),
					JS_MEMBER(blocked),
					JS_MEMBER(applied),
					JS_MEMBER(removedProjects),
					JS_MEMBER(removedFolders),
					JS_MEMBER(dependencies),
					JS_MEMBER(issues)
				);
			};
	
		}
	}
}