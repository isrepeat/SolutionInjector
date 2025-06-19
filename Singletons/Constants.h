#pragma once
#include <Helpers/Singleton.hpp>
#include <regex>

namespace Core::Singletons {
	struct Constants : public H::_Singleton<class Constants> {
		using _MyBase = H::_Singleton<class Constants>;
		friend _MyBase; // to have access to private Ctor

		Constants();
	public:
		~Constants() = default;

	public:
		const std::regex projectRe;
	};
}