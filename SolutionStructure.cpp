//#include <Helpers/Logger.h>
//#include "GuidGenerator.h"
//#include "SolutionFile.h"
//#include "SlnTreeNode.h"
//
//namespace Core {
//	std::shared_ptr<SlnTreeNode> SlnTreeNode::CreateProject(const ProjectBlock& block) {
//		auto node = std::make_shared<SlnTreeNode>();
//		node->guid = block.guid;
//		node->name = block.name;
//		node->path = block.path;
//		node->typeGuid = block.typeGuid;
//		node->isFolder = false;
//		return node;
//	}
//
//	std::shared_ptr<SlnTreeNode> SlnTreeNode::CreateFolder(const std::string& name) {
//		auto node = std::make_shared<SlnTreeNode>();
//		node->guid = GuidGenerator::Generate();
//		node->name = name;
//		node->path = name;
//		node->typeGuid = SolutionFile::SolutionFolderTypeGuid;
//		node->isFolder = true;
//		return node;
//	}
//
//	ProjectBlock SlnTreeNode::ToProjectBlock() const {
//		ProjectBlock block;
//		block.guid = this->guid;
//		block.name = this->name;
//		block.path = this->path;
//		block.typeGuid = this->typeGuid;
//		block.RebuildLines();
//		return block;
//	}
//
//	void SlnTreeNode::Recurse(
//		std::vector<ProjectBlock>& outProjects,
//		std::vector<NestedProjectItem>& outNested
//	) const {
//		outProjects.push_back(this->ToProjectBlock());
//
//		if (auto p = this->parent.lock()) {
//			outNested.emplace_back(this->guid, p->guid);
//		}
//
//		for (const auto& child : this->children) {
//			child->Recurse(outProjects, outNested);
//		}
//	}
//
//	void SlnTreeNode::LogSlnTree(const std::shared_ptr<SlnTreeNode>& node, int indent) {
//		std::string prefix(indent * 2, ' ');
//		std::string kind = node->isFolder ? "[Folder]" : "[Project]";
//		LOG_DEBUG_D("{}{} {} ({})", prefix, kind, node->name, node->guid);
//
//		for (const auto& child : node->children) {
//			SlnTreeNode::LogSlnTree(child, indent + 1);
//		}
//	}
//}