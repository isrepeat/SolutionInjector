#pragma once
#include "Raw/SolutionDocument.h"
#include "ParsedSectionBase.h"

namespace Core {
	namespace Model {
		// Неизвестные секции сохраняются в существующей модели, а не отбрасываются парсером.
		template<typename TScope>
		struct RawSection : TScope::template DefaultCloneableImpl_t<RawSection<TScope>> {
			using Base = typename TScope::template DefaultCloneableImpl_t<RawSection<TScope>>;
			std::vector<std::string> lines;

			explicit RawSection(const Raw::Section& section) : Base(section.name, section.role) {
				for (const auto& line : section.lines) {
					lines.emplace_back(line);
				}
			}

			std::string Serialize(std::ex::optional_ref<const H::IServiceProvider>) const override {
				// Возвращаем сохранённые строки неизвестной секции без разбора её содержимого.
				std::string result;
				for (const auto& line : lines) {
					result += line + "\n";
				}
				return result;
			}

		protected:
			std::vector<std::string> SerializeBody(std::ex::optional_ref<const H::IServiceProvider>) const override {
				return {};
			}
		};

		using RawProjectSection = RawSection<ParsedProjectSectionBase>;
		using RawGlobalSection = RawSection<ParsedGlobalSectionBase>;
	}
}