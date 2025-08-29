#pragma once
#include <string>

namespace Core {
	namespace Model {
		struct ISerializable {
			virtual ~ISerializable() = default;
			virtual std::string Serialize() const = 0;
		};
	}
}