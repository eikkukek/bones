#pragma once

#include <cstdint>

namespace engine {

	template<typename T>
	inline void XorSwap(T& a, T& b) {
		a = a ^ b;
		b = a ^ b;
		a = a ^ b;
	}

	template<typename T>
	inline void Swap(T& a, T& b) {
		T temp = a;
		a = b;
		b = temp;
	}

	template<typename T>
	inline T Clamp(T val, T min, T max) {
		val = val < min ? min : val;
		return val > max ? max : val;
	}

	template<typename T>
	inline void Clamp(T* pVal, T min, T max) {
		*pVal = *pVal < min ? min : *pVal;
		*pVal = *pVal > max ? max : *pVal;
	}

	template<typename T>
	inline T Max(T a, T b) {
		return a > b ? a : b;
	}

	template<typename T>
	inline T Min(T a, T b) {
		return a < b ? a : b;
	}
	
	template<typename T, typename Iter, typename ConstIter>
	inline Iter Find(const T& val, Iter first, ConstIter end) {
		for (; first != end;) {
			if (val == *first) {
				break;
			}
			++first;
		}
		return first;
	}
}
