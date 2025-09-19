#pragma once
#include <Helpers/Std/Extensions/memoryEx.h>
#include <Helpers/Meta/Concepts.h>
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

			std::string Serialize(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const override {
				auto bodyRows = this->SerializeBody(serviceProviderOpt);

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

			virtual std::vector<std::string> SerializeBody(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const = 0;

			virtual std::string GetSectionScopeName() const = 0;
		};


		struct ParsedProjectSectionBase : ParsedSectionBase {
			template <typename TDerived>
			using DefaultCloneableImpl_t = ParsedSectionBase::ICloneableInherited_t::DefaultCloneableImpl<
				TDerived,
				ParsedProjectSectionBase>;

		protected:
			using ParsedSectionBase::ParsedSectionBase;

			std::string GetSectionScopeName() const override {
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

			std::string GetSectionScopeName() const override {
				return "Global";
			}
		};


		namespace details {
			template <typename TParsedSectionImpl>
			__requires requires { requires
				std::derived_from<TParsedSectionImpl, ParsedSectionBase>&&
				requires { { TParsedSectionImpl::SectionName } -> std::convertible_to<std::string_view>; };
			}
			std::ex::shared_ptr<TParsedSectionImpl> MakeSharedParsedSection(const Raw::Section& section) {
				LOG_ASSERT(section.name == TParsedSectionImpl::SectionName);
				return std::ex::make_shared_ex<TParsedSectionImpl>(section);
			}
		}
	}
}