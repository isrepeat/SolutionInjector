#pragma once
#include <Helpers/Stream.h>

#include <unordered_map>
#include <string_view>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <span>

namespace Core {
	namespace Model {
		namespace Raw {
			struct NodeScope {
				int startLine = 0;
				int endLine = 0;

				// view на поддиапазон solutionDocument.streamLineViewer.lines [startLine, endLine)
				std::span<const std::string_view> lines;
			};


			struct Section : NodeScope {
				std::string name;
				std::string role; // preSolution / postSolution / preProject / postProject
			};


			struct Block : NodeScope {
				std::unordered_map<std::string_view, Section> sectionMap;
			};


			struct SolutionDocument {
				const H::Stream::StreamLineViewer streamLineViewer;
				const std::filesystem::path solutionFile;
				std::vector<Block> projectBlocks;
				Block globalBlock;

				static std::unique_ptr<SolutionDocument> FromFile(const std::filesystem::path& solutionFile) {
					std::ifstream inStream(solutionFile, std::ios::binary);
					H::Stream::ThrowIfStreamNotOpened(inStream);
					
					return std::make_unique<SolutionDocument>(inStream, solutionFile);
				}

			//private:
				SolutionDocument(std::istream& inStream, const std::filesystem::path& solutionFile)
					: streamLineViewer{ inStream }
					, solutionFile{ solutionFile } {
				}
			};
		}
	}
}