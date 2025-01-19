#pragma once

#include <random>

namespace engine {

	inline int64_t RandomInt(int64_t low, int64_t high) {
		std::random_device device{};
		std::mt19937_64 mt(device());
		std::uniform_int_distribution<int64_t> dist(low, high);
		return dist(mt);
	}

	inline float RandomFloat(float low, float high) {
		std::random_device device{};
		std::mt19937_64 mt(device());
		std::uniform_real_distribution<float> dist(low, high);
		return dist(mt);
	}
}
