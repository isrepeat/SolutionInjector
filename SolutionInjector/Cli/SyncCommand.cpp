#include "SyncCommand.h"

#include <JsonParser/json_struct/json_struct.h>
#include <Helpers/CommandLineParser.h>

#include "../Helpers/TextHelpers.h"
#include "../IO/FileSnapshot.h"
#include "../SolutionFileAnalyzer.h"

#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

namespace Core {
	namespace Cli {
		namespace details {
			struct SyncReport {
				int schemaVersion = 1;
				bool changed = false;
				bool applied = false;
				bool blocked = false;
				std::vector<std::string> issues;
				JS_OBJECT(JS_MEMBER(schemaVersion), JS_MEMBER(changed), JS_MEMBER(applied), JS_MEMBER(blocked), JS_MEMBER(issues));
			};

			std::string TrimNewlines(std::string value) {
				while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
					value.pop_back();
				}
				return value;
			}
		} // namespace details

		int RunSyncCommand(int argc, char* argv[]) {
			using Count = H::CommandLineParserA::ExpectedValuesCount;
			H::CommandLineParserA commandLineParser({
				{ "--owner", Count::Single, true },
				{ "--adopt", Count::Multiple, false },
				{ "--apply", Count::None, false },
				{ "--json", Count::None, false }
			});

			try {
				commandLineParser.Parse(argc, argv);
			} catch (const std::exception& exception) {
				std::cerr << exception.what() << '\n';
				return 1;
			}

			const auto& positional = commandLineParser.GetPositional();
			if (positional.size() != 3 || positional[0] != "sync") {
				std::cerr << "Expected sync <desired.sln> <target.sln> --owner <id> [--adopt <GUID>] [--apply] [--json]\n";
				return 1;
			}

			details::SyncReport syncReport;
			try {
				// 1. Фиксируем оба входа и проверяем, что решение назначения не является источником.
				const auto sourcePath = std::filesystem::absolute(Helpers::Utf8ToWide(positional[1]));
				const auto targetPath = std::filesystem::absolute(Helpers::Utf8ToWide(positional[2]));
				if (sourcePath.extension() != ".sln" || targetPath.extension() != ".sln" ||
					IO::FileSnapshot::PathKey(sourcePath) == IO::FileSnapshot::PathKey(targetPath)) {
					throw std::runtime_error("Specify distinct .sln paths");
				}
				IO::FileSnapshot fileSnapshot;
				const auto source = fileSnapshot.Read(sourcePath);
				const auto original = fileSnapshot.Read(targetPath);
				if (source.find("Microsoft Visual Studio Solution File, Format Version") == std::string::npos ||
					original.find("Microsoft Visual Studio Solution File, Format Version") == std::string::npos) {
					throw std::runtime_error("Invalid solution header");
				}
				auto sourceStructure = SolutionFileAnalyzer::BuildSolutionStructure(sourcePath);
				auto targetStructure = SolutionFileAnalyzer::BuildSolutionStructure(targetPath);
				fileSnapshot.Verify();
				const auto baseline = details::TrimNewlines(targetStructure->Serialize(std::nullopt));
				std::vector<H::Guid> adoptedProjects;
				for (const auto& value : commandLineParser.GetAll("--adopt")) {
					adoptedProjects.push_back(H::Guid::Parse(value));
				}

				// 2. Рассчитываем результат полностью в памяти; preview не записывает файлы.
				targetStructure->SynchronizeFrom(*sourceStructure, commandLineParser.Get("--owner").value_or(""), adoptedProjects, fileSnapshot);
				auto content = details::TrimNewlines(targetStructure->Serialize(std::nullopt));
				syncReport.changed = content != baseline;
				if (commandLineParser.Has("--apply")) {
					if (syncReport.changed) {
						if (original.find("\r\n") != std::string::npos) {
							std::string converted;
							for (const auto character : content) {
								if (character == '\n') {
									converted += '\r';
								}
								converted += character;
							}
							content = std::move(converted);
						}
						if (original.starts_with("\xEF\xBB\xBF")) {
							content.insert(0, "\xEF\xBB\xBF");
						}
						IO::ReplaceWithBackup(targetPath, content, fileSnapshot);
					}
					syncReport.applied = true;
				}
			} catch (const std::exception& exception) {
				syncReport.blocked = true;
				syncReport.issues.push_back(exception.what());
			}

			// 3. Выводим результат и возвращаем код завершения.
			if (commandLineParser.Has("--json")) {
				std::cout << JS::serializeStruct(syncReport) << '\n';
			} else {
				std::cout << "Sync changed=" << syncReport.changed << " applied=" << syncReport.applied << " blocked=" << syncReport.blocked << '\n';
				for (const auto& issue : syncReport.issues) {
					std::cerr << issue << '\n';
				}
			}
			return syncReport.blocked ? 2 : 0;
		}
	}
}