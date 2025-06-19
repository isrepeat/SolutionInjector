#include "Singletons/Constants.h"
#include "ProjectBlock.h"
#include <regex>

namespace Core {
	std::optional<ProjectBlock> ProjectBlock::Parse(const std::vector<std::string>& lines, size_t& index) {
		std::smatch match;
		if (std::regex_search(lines[index], match, Singletons::Constants::GetInstance().projectRe)) {
			ProjectBlock block;
			block.typeGuid = "{" + match[1].str() + "}";
			block.name = match[2].str();
			block.path = match[3].str();
			block.guid = "{" + match[4].str() + "}";
			block.lines.push_back(lines[index]);

			size_t i = index + 1;
			while (i < lines.size() && lines[i] != "EndProject") {
				block.lines.push_back(lines[i++]);
			}
			if (i < lines.size()) block.lines.push_back("EndProject");
			index = i;
			return block;
		}
		return std::nullopt;
	}

	void ProjectBlock::ReplaceGuid(const std::string& newGuid) {
		for (auto& line : this->lines) {
			size_t pos = line.find(guid);
			if (pos != std::string::npos) {
				line.replace(pos, this->guid.size(), newGuid);
			}
		}
		this->guid = newGuid;
	}
}
