#pragma once
#include <Helpers/Singleton.hpp>
#include <regex>

namespace Core::Singletons {
	struct Constants : public H::Singleton<class Constants> {
		friend SingletonInherited_t; // to have access to private Ctor

		Constants();
	public:
		~Constants() = default;

	public:
	};
}