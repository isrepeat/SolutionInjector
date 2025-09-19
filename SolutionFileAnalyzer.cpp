#include "SolutionFileAnalyzer.h"
#include <Helpers/Logger.h>
#include <Helpers/Regex.h>

namespace Core {
	std::unique_ptr<SolutionStructure> SolutionFileAnalyzer::BuildSolutionStructure(std::filesystem::path solutionPath) {
		auto resultSolutionStructure = std::make_unique<SolutionStructure>(
			SolutionFileAnalyzer::BuildSolutionDocument(solutionPath)
		);
		return resultSolutionStructure;
	}


	std::unique_ptr<Model::Raw::SolutionDocument> SolutionFileAnalyzer::BuildSolutionDocument(std::filesystem::path solutionPath) {
		static const std::regex rxProjectStart(R"_rx_(^\s*Project\(.*$)_rx_");
		static const std::regex rxProjectEnd(R"_rx_(^\s*EndProject\s*$)_rx_");

		static const std::regex rxProjectSectionStart(
			R"_rx_(^\s*ProjectSection\(\s*([^)]+?)\s*\)\s*=\s*(preProject|postProject)\s*$)_rx_"
			//                            └(1)────────┘       └(2)───────────────────┘
			// (1): Section name
			// (2): Position relative to project (preProject or postProject)
		);
		static const std::regex rxProjectSectionEnd(R"_rx_(^\s*EndProjectSection\s*$)_rx_");

		static const std::regex rxGlobalStart(R"_rx_(^\s*Global\s*$)_rx_");
		static const std::regex rxGlobalEnd(R"_rx_(^\s*EndGlobal\s*$)_rx_");

		static const std::regex rxGlobalSectionStart(
			R"_rx_(^\s*GlobalSection\(\s*([^)]+?)\s*\)\s*=\s*(preSolution|postSolution)\s*$)_rx_"
			//                           └(1)────────┘       └(2)─────────────────────┘
			// (1): Section name
			// (2): Position relative to solution (preSolution or postSolution)
		);
		static const std::regex rxGlobalSectionEnd(R"_rx_(^\s*EndGlobalSection\s*$)_rx_");

		auto solutionDocument = Model::Raw::SolutionDocument::FromFile(solutionPath);
		Model::Raw::Block* currentBlock = nullptr;
		Model::Raw::Section* currentSection = nullptr;

		int lineIdx = 0;
		for (auto line : solutionDocument->streamLineViewer.GetAllLines()) {
			// Обновляем «конец файла» (полуоткрытый интервал)
			const int nextLineIdx = lineIdx + 1; // [start, end)

			// Project
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxProjectStart)) {
				LOG_ASSERT(currentBlock == nullptr);

				currentBlock = &solutionDocument->projectBlocks.emplace_back();
				currentBlock->startLine = lineIdx;
			}

			// ProjectSection
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxProjectSectionStart)) {
				LOG_ASSERT(currentBlock != nullptr);
				LOG_ASSERT(currentSection == nullptr);
				LOG_ASSERT(rxMatchResult->capturedGroups.size() > 2);

				auto sectionName = rxMatchResult->capturedGroups[1];
				auto sectionRole = rxMatchResult->capturedGroups[2];

				currentSection = &currentBlock->sectionMap[std::string{ sectionName }];
				currentSection->startLine = lineIdx;
				currentSection->name = sectionName;
				currentSection->role = sectionRole;
			}

			// EndProjectSection
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxProjectSectionEnd)) {
				LOG_ASSERT(currentSection != nullptr);

				currentSection->endLine = nextLineIdx;
				currentSection->lines = solutionDocument->streamLineViewer.GetLinesSpan(
					static_cast<std::size_t>(currentSection->startLine),
					static_cast<std::size_t>(currentSection->endLine)
				);
				currentSection = nullptr;
			}

			// EndProject
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxProjectEnd)) {
				LOG_ASSERT(currentBlock != nullptr);

				currentBlock->endLine = nextLineIdx;
				currentBlock->lines = solutionDocument->streamLineViewer.GetLinesSpan(
					static_cast<std::size_t>(currentBlock->startLine),
					static_cast<std::size_t>(currentBlock->endLine)
				);
				currentBlock = nullptr;
			}

			// Global	
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxGlobalStart)) {
				LOG_ASSERT(currentBlock == nullptr);

				currentBlock = &solutionDocument->globalBlock;
				currentBlock->startLine = lineIdx;
			}

			// GlobalSection
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxGlobalSectionStart)) {
				LOG_ASSERT(currentBlock != nullptr);
				LOG_ASSERT(currentSection == nullptr);
				LOG_ASSERT(rxMatchResult->capturedGroups.size() > 2);

				auto sectionName = rxMatchResult->capturedGroups[1];
				auto sectionRole = rxMatchResult->capturedGroups[2];

				currentSection = &currentBlock->sectionMap[std::string{ sectionName }];
				currentSection->startLine = lineIdx;
				currentSection->name = sectionName;
				currentSection->role = sectionRole;
			}

			// EndGlobalSection
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxGlobalSectionEnd)) {
				LOG_ASSERT(currentSection != nullptr);

				currentSection->endLine = nextLineIdx;
				currentSection->lines = solutionDocument->streamLineViewer.GetLinesSpan(
					static_cast<std::size_t>(currentSection->startLine),
					static_cast<std::size_t>(currentSection->endLine)
				);
				currentSection = nullptr;
			}

			// EndGlobal
			if (auto rxMatchResult = H::Regex::GetRegexMatch(line, rxGlobalEnd)) {
				LOG_ASSERT(currentBlock != nullptr);

				currentBlock->endLine = nextLineIdx;
				currentBlock->lines = solutionDocument->streamLineViewer.GetLinesSpan(
					static_cast<std::size_t>(currentBlock->startLine),
					static_cast<std::size_t>(currentBlock->endLine)
				);
				currentBlock = nullptr;
			}

			lineIdx++;
		}

		return solutionDocument;
	}
}