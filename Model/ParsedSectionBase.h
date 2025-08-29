#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/ICloneable.h>
#include <Helpers/Logger.h>

#include "ISerializable.h"

#include <string_view>
#include <concepts>
#include <string>
#include <vector>
#include <format>

namespace Core {
	namespace Model {
		struct ParsedSectionBase :
			H::ICloneable<ParsedSectionBase>,
			ISerializable {

			const std::string sectionName;
			const std::string sectionRole;

			virtual ~ParsedSectionBase() {}

			std::string Serialize() const override {
				auto bodyRows = this->SerializeBody();

				std::string out;
				out += std::format(
					"\t{}Section({}) = {}\n",
					this->GetSectionScopeName(),
					this->sectionName,
					this->sectionRole
				);

				for (const auto& row : bodyRows) {
					out += std::format("\t\t{}\n", row);
				}

				out += std::format("\tEnd{}Section\n", this->GetSectionScopeName());
				return out;
			}

		protected:
			ParsedSectionBase(
				std::string sectionName,
				std::string sectionRole)
				: sectionName{ sectionName }
				, sectionRole{ sectionRole } {
			}

			virtual std::vector<std::string> SerializeBody() const = 0;
			virtual std::string_view GetSectionScopeName() const = 0;
		};


		struct ParsedProjectSectionBase : ParsedSectionBase {
			template <typename TDerived>
			using DefaultCloneableImpl_t = ParsedSectionBase::ICloneableInherited_t::DefaultCloneableImpl<
				TDerived,
				ParsedProjectSectionBase>;

		protected:
			using ParsedSectionBase::ParsedSectionBase;

			std::string_view GetSectionScopeName() const override {
				return "Project";
			}
		};


		struct ParsedGlobalSectionBase : ParsedSectionBase {
			template <typename TDerived>
			using DefaultCloneableImpl_t = ParsedSectionBase::ICloneableInherited_t::DefaultCloneableImpl<
				TDerived,
				ParsedGlobalSectionBase>;

		protected:
			using ParsedSectionBase::ParsedSectionBase;

			std::string_view GetSectionScopeName() const override {
				return "Global";
			}
		};


		namespace details {
			template <typename ParsedSectionImplT>
				requires std::derived_from<ParsedSectionImplT, ParsedSectionBase>&&
				requires { { ParsedSectionImplT::SectionName } -> std::convertible_to<std::string_view>; }
			static std::ex::shared_ptr<ParsedSectionImplT> MakeSharedParsedSection(const Raw::Section& section) {
				LOG_ASSERT(section.name == ParsedSectionImplT::SectionName);
				return std::ex::make_shared_ex<ParsedSectionImplT>(section);
			}
		}
	}
}