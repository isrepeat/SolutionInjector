#pragma once
#include <Helpers/Guid.h>
#include <filesystem>

namespace Core {
	namespace Model {
		class ISolutionStructureProvider {
		public:
			virtual ~ISolutionStructureProvider() = default;
		};


		struct SolutionInfo {
			std::filesystem::path solutionFile;
			H::Guid solutionGuid;
		};


		class ISolutionInfoReader : public virtual ISolutionStructureProvider {
		public:
			virtual ~ISolutionInfoReader() = default;
			
			virtual const SolutionInfo GetSolutionInfo() const = 0;
		};
	}
}