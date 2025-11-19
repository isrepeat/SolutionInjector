#pragma once
#include <Helpers/Stream.h>

#include "Model/Raw/SolutionDocument.h"
#include "SolutionStructure.h"
#include <filesystem>

namespace Core {
	class SolutionFileAnalyzer {
	public:
		static std::unique_ptr<SolutionStructure> BuildSolutionStructure(std::filesystem::path solutionPath);

	private:
		static std::unique_ptr<Model::Raw::SolutionDocument> BuildSolutionDocument(std::filesystem::path solutionPath);
	};
}