#pragma once
#include <Helpers/Std/Extensions/optional_ref.h>
#include <Helpers/IServiceProvider.h>
#include <optional>
#include <string>
#include <memory>

namespace Core {
	namespace Model {
		struct ISerializable {
		public:
			virtual ~ISerializable() = default;

		public:
			virtual std::string Serialize(
				std::ex::optional_ref<const H::IServiceProvider> serviceProviderOpt
			) const = 0;
		};
	}
}