#pragma once
#include "../IO/FileSnapshot.h"

#include <filesystem>
#include <string>
#include <map>
#include <set>

namespace Core {
	namespace Dependencies {
		class MsBuildDependencyReader {
		public:
			explicit MsBuildDependencyReader(IO::FileSnapshot& fileSnapshot);
			void Read(
				const std::filesystem::path& projectPath,
				const std::filesystem::path& solutionPath,
				const std::map<std::string, std::string>& projectPaths
			);

			std::set<std::string> targets;
			std::set<std::string> issues;

		private:
			void ReadFile(
				const std::filesystem::path& path,
				const std::filesystem::path& projectPath,
				const std::filesystem::path& solutionPath,
				const std::map<std::string, std::string>& projectPaths
			);

			IO::FileSnapshot& fileSnapshot;
			std::set<std::string> visited;
		};
	}
}