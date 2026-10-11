#include "MsBuildDependencyReader.h"

#include "../Helpers/TextHelpers.h"

#include <wrl/client.h>
#include <stdexcept>
#include <windows.h>
#include <msxml6.h>

namespace Core {
	namespace Dependencies {
		namespace {
			using Microsoft::WRL::ComPtr;

			struct ComSession {
				ComSession() {
					if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
						throw std::runtime_error("Cannot initialize COM");
					}
				}

				~ComSession() {
					CoUninitialize();
				}
			};

			struct Bstr {
				BSTR value;

				explicit Bstr(const std::wstring& text) : value(SysAllocStringLen(text.data(), static_cast<UINT>(text.size()))) {
				}

				~Bstr() {
					SysFreeString(value);
				}
			};

			std::string Attribute(IXMLDOMNode* node, const wchar_t* name) {
				ComPtr<IXMLDOMNamedNodeMap> attributes;
				if (FAILED(node->get_attributes(&attributes)) || !attributes) {
					return {};
				}
				ComPtr<IXMLDOMNode> attribute;
				Bstr key(name);
				attributes->getNamedItem(key.value, &attribute);
				if (!attribute) {
					return {};
				}
				BSTR text = nullptr;
				attribute->get_text(&text);
				const auto result = text ? Helpers::WideToUtf8(text) : std::string{};
				SysFreeString(text);
				return result;
			}

			std::string Expand(
				std::string value,
				const std::filesystem::path& path,
				const std::filesystem::path& projectPath,
				const std::filesystem::path& solutionPath
			) {
				const std::map<std::string, std::string> properties{
					{"$(MSBuildThisFileDirectory)", Helpers::WideToUtf8(path.parent_path().wstring()) + "\\"},
					{"$(MSBuildProjectDirectory)", Helpers::WideToUtf8(projectPath.parent_path().wstring())},
					{"$(SolutionDir)", Helpers::WideToUtf8(solutionPath.parent_path().wstring()) + "\\"}
				};
				for (const auto& property : properties) {
					size_t offset = 0;
					while ((offset = value.find(property.first, offset)) != std::string::npos) {
						value.replace(offset, property.first.size(), property.second);
						offset += property.second.size();
					}
				}
				return value;
			}
		}

		MsBuildDependencyReader::MsBuildDependencyReader(IO::FileSnapshot& fileSnapshot) : fileSnapshot(fileSnapshot) {
		}

		void MsBuildDependencyReader::Read(
			const std::filesystem::path& projectPath,
			const std::filesystem::path& solutionPath,
			const std::map<std::string, std::string>& projectPaths
		) {
			// 1. Открываем COM-сессию и анализируем XML проекта.
			ComSession comSession;
			ReadFile(projectPath, projectPath, solutionPath, projectPaths);

			// 2. Учитываем ближайшие общие настройки MSBuild в родительских каталогах.
			for (const auto* name : {L"Directory.Build.props", L"Directory.Build.targets"}) {
				auto directory = projectPath.parent_path();
				while (!directory.empty()) {
					if (std::filesystem::exists(directory / name)) {
						ReadFile(directory / name, projectPath, solutionPath, projectPaths);
						break;
					}
					const auto parent = directory.parent_path();
					if (parent == directory) {
						break;
					}
					directory = parent;
				}
			}
		}

		void MsBuildDependencyReader::ReadFile(
			const std::filesystem::path& path,
			const std::filesystem::path& projectPath,
			const std::filesystem::path& solutionPath,
			const std::map<std::string, std::string>& projectPaths
		) {
			// 1. Исключаем повторный обход импортов и проверяем наличие файла.
			if (!visited.insert(IO::FileSnapshot::PathKey(path)).second) {
				return;
			}
			const auto displayPath = Helpers::WideToUtf8(path.wstring());
			if (!std::filesystem::is_regular_file(path)) {
				issues.insert("Missing file: " + displayPath);
				return;
			}

			// 2. Читаем XML из снимка; внешние сущности и DTD отключены.
			const auto content = fileSnapshot.Read(path);
			ComPtr<IXMLDOMDocument2> xml;
			if (FAILED(CoCreateInstance(__uuidof(DOMDocument60), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&xml)))) {
				throw std::runtime_error("MSXML6 is unavailable");
			}
			xml->put_async(VARIANT_FALSE);
			xml->put_validateOnParse(VARIANT_FALSE);
			xml->put_resolveExternals(VARIANT_FALSE);
			Bstr prohibit(L"ProhibitDTD");
			VARIANT enabled;
			VariantInit(&enabled);
			enabled.vt = VT_BOOL;
			enabled.boolVal = VARIANT_TRUE;
			xml->setProperty(prohibit.value, enabled);
			Bstr source(Helpers::Utf8ToWide(content));
			VARIANT_BOOL loaded = VARIANT_FALSE;
			if (FAILED(xml->loadXML(source.value, &loaded)) || loaded != VARIANT_TRUE) {
				issues.insert("Invalid XML: " + displayPath);
				return;
			}

			// 3. Находим ссылки на проекты и импорты независимо от XML namespace.
			ComPtr<IXMLDOMNodeList> nodes;
			Bstr query(L"//*[local-name()='ProjectReference' or local-name()='Import']");
			if (FAILED(xml->selectNodes(query.value, &nodes))) {
				issues.insert("Cannot inspect XML: " + displayPath);
				return;
			}
			long count = 0;
			nodes->get_length(&count);
			for (long index = 0; index < count; ++index) {
				ComPtr<IXMLDOMNode> node;
				nodes->get_item(index, &node);
				const auto include = Attribute(node.Get(), L"Include");
				const bool reference = !include.empty();
				const auto raw = reference ? include : Attribute(node.Get(), L"Project");
				// Стандартные импорты toolchain не обходятся как проекты решения.
				if (!reference && (raw.rfind("$(VCTargetsPath)", 0) == 0 || raw.rfind("$(MSBuildToolsPath)", 0) == 0 ||
					raw.rfind("$(MSBuildExtensionsPath", 0) == 0 || raw.rfind("$(UserRootDir)", 0) == 0)) {
					continue;
				}

				// 4. Раскрываем известные свойства и фиксируем неоднозначные пути.
				const auto expanded = Expand(raw, path, projectPath, solutionPath);
				// Без выполнения MSBuild неизвестные свойства нельзя считать отсутствующими зависимостями.
				if (expanded.empty() || expanded.find("$(") != std::string::npos || expanded.find("@(") != std::string::npos ||
					expanded.find("%(") != std::string::npos || expanded.find_first_of("*?;") != std::string::npos) {
					issues.insert("Unresolved dependency in " + displayPath + ": " + raw);
					continue;
				}

				// 5. Добавляем ребро к проекту решения либо рекурсивно читаем внешний импорт.
				const auto dependency = std::filesystem::weakly_canonical(path.parent_path() / Helpers::Utf8ToWide(expanded));
				const auto found = projectPaths.find(IO::FileSnapshot::PathKey(dependency));
				if (found != projectPaths.end()) {
					targets.insert(found->second);
				} else {
					ReadFile(dependency, reference ? dependency : projectPath, solutionPath, projectPaths);
				}
			}
		}
	}
}