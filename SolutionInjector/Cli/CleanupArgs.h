#pragma once
#include <Helpers/CommandLineParser.h>

#include "../Helpers/TextHelpers.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace Core {
	namespace Cli {
		enum class CleanupOperation {
			Analyze,
			Prune
		};

		struct CleanupArgs {
			CleanupOperation cleanupOperation = CleanupOperation::Analyze;
			std::filesystem::path solutionPath;
			std::string scope;
			std::vector<std::string> roots;
			bool apply = false;
			bool json = false;

			static std::vector<H::CommandLineParserA::FlagDesc> Description() {
				// Описываем флаги и число значений для общего парсера.
				using Count = H::CommandLineParserA::ExpectedValuesCount;
				return {
					{ "--scope", Count::Single, true },
					{ "--root", Count::Multiple, false },
					{ "--apply", Count::None, false },
					{ "--json", Count::None, false }
				};
			}

			static void MapParsedResults(const H::CommandLineParserA& commandLineParser, CleanupArgs& cleanupArgs) {
				// 1. Проверяем подкоманду и единственный путь решения.
				const auto& positional = commandLineParser.GetPositional();
				if (positional.size() != 2 || (positional[0] != "analyze" && positional[0] != "prune")) {
					throw std::runtime_error("Expected analyze|prune <consumer.sln>");
				}

				// 2. Преобразуем результаты парсера в типизированные параметры.
				cleanupArgs.cleanupOperation = positional[0] == "prune" ? CleanupOperation::Prune : CleanupOperation::Analyze;
				cleanupArgs.solutionPath = std::filesystem::absolute(Helpers::Utf8ToWide(positional[1]));
				cleanupArgs.scope = commandLineParser.Get("--scope").value_or("");
				cleanupArgs.roots = commandLineParser.GetAll("--root");
				cleanupArgs.apply = commandLineParser.Has("--apply");
				cleanupArgs.json = commandLineParser.Has("--json");
			}

			void Validate() const {
				// Проверяем совместимость параметров перед выполнением команды.
				if (scope.empty() || solutionPath.extension() != ".sln" || (apply && cleanupOperation != CleanupOperation::Prune)) {
					throw std::runtime_error("Specify .sln and --scope; --apply is only valid with prune");
				}
			}
		};
	}
}