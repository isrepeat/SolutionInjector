#pragma once
#include <random>

namespace Core {
	class GuidGenerator {
	public:
		static std::string Generate() {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			static std::uniform_int_distribution<> dis(0, 15);

			const char* hex = "0123456789ABCDEF";
			std::string guid = "{";
			for (int i = 0; i < 32; ++i) {
				if (i == 8 || i == 12 || i == 16 || i == 20) guid += "-";
				guid += hex[dis(gen)];
			}
			guid += "}";
			return guid;
		}
	};
}