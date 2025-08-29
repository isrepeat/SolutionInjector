#include "SolutionFileAnalyzer.h"
#include <Helpers/Logger.h>
#include <Helpers/Regex.h>

namespace Core {
	std::unique_ptr<SolutionStructure> SolutionFileAnalyzer::BuildSolutionStructure(std::filesystem::path solutionPath) {
		auto solutionDocument = SolutionFileAnalyzer::BuildSolutionDocument(solutionPath);

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

				currentSection = &currentBlock->sectionMap[sectionName];
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

				currentSection = &currentBlock->sectionMap[sectionName];
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


	//void SolutionFileAnalyzer::HandleNestedProjectsSection(const std::shared_ptr<Model::SolutionStructure>& solutionStructure) {
	//	const auto& mapGuidToSolutionNode = solutionStructure->GetView().mapGuidToSolutionNode;

	//	auto itParsedSection = solutionStructure->GetView().globalBlock->sectionMap.find(
	//		Model::Global::ParsedNestedProjectsSection::SectionName
	//	);
	//	if (itParsedSection != solutionStructure->GetView().globalBlock->sectionMap.end()) {
	//		auto parsedNestedProjectsSection = itParsedSection->second
	//			.Cast<Model::Global::ParsedNestedProjectsSection>();

	//		for (const auto& entry : parsedNestedProjectsSection->entries) {
	//			auto itChild = mapGuidToSolutionNode.find(entry.childGuid);
	//			auto itParent = mapGuidToSolutionNode.find(entry.parentGuid);

	//			if (itChild != mapGuidToSolutionNode.end() &&
	//				itParent != mapGuidToSolutionNode.end()) {
	//				const auto& childNode = itChild->second;
	//				const auto& parentNode = itParent->second;
	//				LOG_ASSERT(parentNode.Is<Model::Project::SolutionFolder>());

	//				auto solutionFolder = parentNode.Cast<Model::Project::SolutionFolder>();
	//				solutionFolder->LinkChildNode(childNode);
	//			}
	//		}
	//	}
	//}


	////void SolutionFileAnalyzer::HandleSharedMSBuildProjectFilesSection(const Model::SolutionStructure& solutionStructure) {
	////	auto solutionStructureView = Model::SolutionStructureView(solutionStructure);
	////	auto mapGuidToSolutionNode = solutionStructureView.BuildMapGuidToSolutionNode();

	////	auto itParsedSection = solutionStructure.globalBlock->sectionMap.find(
	////		Parsers::GlobalBlock::SharedMSBuildProjectFilesSectionParser::SectionName
	////	);
	////	if (itParsedSection != solutionStructure.globalBlock->sectionMap.end()) {
	////		auto parsedSharedMSBuildProjectFilesSection = itParsedSection->second
	////			.Cast<Parsers::GlobalBlock::ParsedSharedMSBuildProjectFilesSection>();

	////		for (auto& entry : parsedSharedMSBuildProjectFilesSection->entries) {
	////			auto itSolutionNode = mapGuidToSolutionNode.find(entry.guid);
	////			if (itSolutionNode != mapGuidToSolutionNode.end()) {
	////				entry.solutionNode = itSolutionNode->second;
	////			}
	////		}
	////	}
	////}


	//void SolutionFileAnalyzer::HandleProjectConfigurationPlatformSection(const std::shared_ptr<Model::SolutionStructure>& solutionStructure) {
	//	const auto& mapGuidToSolutionNode = solutionStructure->GetView().mapGuidToSolutionNode;

	//	auto itParsedSection = solutionStructure->GetView().globalBlock->sectionMap.find(
	//		Model::Global::ParsedProjectConfigurationPlatformsSection::SectionName
	//	);
	//	if (itParsedSection != solutionStructure->GetView().globalBlock->sectionMap.end()) {
	//		auto parsedProjectConfigurationPlatformsSection = itParsedSection->second
	//			.Cast<Model::Global::ParsedProjectConfigurationPlatformsSection>();

	//		for (const auto& entry : parsedProjectConfigurationPlatformsSection->entries) {
	//			auto itSolutionNode = mapGuidToSolutionNode.find(entry.guid);
	//			if (itSolutionNode != mapGuidToSolutionNode.end()) {
	//				const auto& solutionNode = itSolutionNode->second;
	//				LOG_ASSERT(solutionNode.Is<Model::Project::ProjectNode>());

	//				auto projectNode = solutionNode.Cast<Model::Project::ProjectNode>();
	//				projectNode->configurations.push_back(entry.configEntry);
	//			}
	//		}
	//	}
	//}
}