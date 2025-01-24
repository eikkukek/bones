#pragma once

#include "renderer.hpp"
#include "text_renderer.hpp"
#include "math.hpp"
#include "fmt/printf.h"
#include "fmt/color.h"
#include "vulkan/vulkan_core.h"
#include "imgui.h"
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <chrono>

namespace engine {

	class Engine;
	class UI;

	typedef uint64_t UID;
	typedef std::lock_guard<std::mutex> LockGuard;
	typedef Renderer::MeshData MeshData;

	template<typename T, typename U>
	struct IsSame {
		static constexpr bool value = false;
	};

	template<typename T>
	struct IsSame<T, T> {
		static constexpr bool value = true;
	};

	enum class ErrorOrigin {
		Uncategorized = 0,
		Engine = 1,
		Renderer = 2,
		TextRenderer = 3,
		UI = 4,
		OutOfMemory = 5,
		NullDereference = 6,
		IndexOutOfBounds = 7,
		Vulkan = 8,
		Stb = 9,
		FileParsing = 10,
		GameLogic = 11,
		MaxEnum,
	};

	static const char* ErrorOriginString(ErrorOrigin origin) {
		const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
			"Uncategorized",
			"Engine",
			"Renderer",
			"TextRenderer",
			"UI",
			"OutOfMemory",
			"NullDereference",
			"IndexOutOfBounds",
			"Vulkan",
			"stb",
			"FileParsing",
			"GameLogic",
		};
		if (origin == ErrorOrigin::MaxEnum) {
			return strings[0];
		}
		return strings[(size_t)origin];
	}

	void CriticalError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS);

	inline void PrintError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS) {
		fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
			"Engine called an error!\nError origin: {}s\nError: {}\n", ErrorOriginString(origin), err);
		if (vkErr != VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "Vulkan error code: {}\n", (int)vkErr);
		}
	}

	inline void PrintWarning(const char* warn) {
		fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, 
			"Engine called a warning:\n {}\n", warn);
	}

	template<typename T>
	class DynamicArray {
	public:

		typedef T* Iterator;
		typedef const T* ConstIterator;

	private:

		T* m_Data;
		uint32_t m_Size;
		uint32_t m_Capacity;

	public:

		DynamicArray() : m_Data(nullptr), m_Size(0), m_Capacity(0) {}

		DynamicArray(size_t size) : m_Data(nullptr), m_Size(0), m_Capacity(0) {
			if (!size) {
				return;
			}
			Reserve(size * 2);
			m_Size = size;
		}

		DynamicArray(const DynamicArray& other) noexcept 
			: m_Data(nullptr), m_Size(0), m_Capacity(0) {
			Reserve(other.m_Capacity);
			for (const T& val : other) {
				PushBack(val);
			}
		}

		DynamicArray operator=(const DynamicArray& other) noexcept {
			Clear();
			Reserve(other.m_Capacity);
			for (const T& val : other) {
				PushBack(val);
			}
		}

		DynamicArray(DynamicArray&& other) noexcept 
			: m_Data(other.m_Data), m_Size(other.m_Size), m_Capacity(other.m_Capacity) {
			other.m_Data = nullptr;
			other.m_Size = 0;
			other.m_Capacity = 0;
		}

		DynamicArray& operator=(DynamicArray&& other) noexcept {
			Clear();
			m_Data = other.m_Data;
			m_Size = other.m_Size;
			m_Capacity = other.m_Capacity;
			other.m_Data = 0;
			other.m_Size = 0;
			other.m_Capacity = 0;
		}

		~DynamicArray() {
			for (size_t i = 0; i < m_Size; i++) {
				(&m_Data[i])->~T();
			}
			free(m_Data);
			m_Data = nullptr;
			m_Size = 0;
			m_Capacity = 0;
		}

		size_t Size() const noexcept {
			return m_Size;
		}

		T* Data() noexcept {
			return m_Data;
		}
 
		const T* Data() const noexcept {
			return m_Data;
		}

		DynamicArray& Reserve(size_t capacity) {
			if (capacity < m_Capacity) {
				return *this;
			}
			T* temp = (T*)malloc(sizeof(T) * capacity);
			for (size_t i = 0; i < m_Size; i++) {
				new(&temp[i]) T(std::move(m_Data[i]));
			}
			free(m_Data);
			m_Capacity = capacity;
			m_Data = temp;
			return *this;
		}

		DynamicArray& Resize(size_t size) {
			if (size == m_Size) {
				return *this;
			}
			if (size < m_Size) {
				for (size_t i = size; i < m_Size; i++) {
					(&m_Data[i])->~T();
				}
				m_Size = size;
				return *this;
			}
			if (size >= m_Capacity) {
				Reserve(size * 2);
			}
			for (size_t i = m_Size; i < size; i++) {
				new(&m_Data[i]) T();
			}
			m_Size = size;
			return *this;
		}

		DynamicArray& Clear() {
			free(m_Data);
			m_Data = nullptr;
			m_Size = 0;
			m_Capacity = 0;
			return *this;
		}

		T& PushBack(const T& value) {
			if (m_Size >= m_Capacity) {
				Reserve(m_Capacity ? m_Capacity * 2 : 2);
			}
			new(&m_Data[m_Size]) T(value);
			return m_Data[m_Size++];
		}

		template<typename... Args>
		T& EmplaceBack(Args&&... args) {
			if (m_Size >= m_Capacity) {
				Reserve(m_Capacity ? m_Capacity * 2 : 2);
			}
			new(&m_Data[m_Size]) T(std::forward<Args>(args)...);
			return m_Data[m_Size++];
		}

		Iterator Erase(Iterator where) {
			ptrdiff_t diff = where - m_Data;
			if (diff < 0 || diff >= m_Size) {
				assert(false && "attempting to erase from an array with an invalid iterator");
				return nullptr;
			}
			while (where != &m_Data[m_Size - 1]) {
				where->~T();
				new(where) T(std::move(*(where + 1)));
				++where;
			}
			--m_Size;
			return &m_Data[diff];
		}

		Iterator begin() {
			return m_Data;
		}

		ConstIterator begin() const {
			return m_Data;
		}

		ConstIterator end() const {
			return m_Data ? &m_Data[m_Size] : nullptr;
		}

		T& operator[](size_t index) {
			if (index >= m_Size) {
				CriticalError(ErrorOrigin::IndexOutOfBounds, "index out of bounds (DynamicArray::operator[])!");
			}
			return m_Data[index];
		}

		const T& operator[](size_t index) const {
			if (index >= m_Size) {
				CriticalError(ErrorOrigin::IndexOutOfBounds, "index out of bounds (DynamicArray::operator[])!");
			}
			return m_Data[index];
		}
	};

	template<typename T>
	class DynamicMatrix {

	private:

		Vec2_T<uint32_t> m_Extent;
		T* m_Data;

	public:

		DynamicMatrix() noexcept : m_Extent(), m_Data(nullptr) {}	

		DynamicMatrix(Vec2_T<uint32_t> extent) noexcept
			: m_Extent(extent), m_Data(nullptr) {
			if (m_Extent.x == 0 || m_Extent.y == 0) {
				PrintError(ErrorOrigin::Engine, 
					"attempting to initialize dynamic matrix with extent that's zero (in DynamicMatrix constructor)!");
				m_Extent = 0;
				return;
			}
			m_Data = new T[m_Extent.x * m_Extent.y]{};
			if (!m_Data) {
				PrintError(ErrorOrigin::Engine, 
					"failed to allocate memory for dynamic matrix (operator new in DynamicMatrix constructor)!");
				m_Extent = 0;
			}
		}

		~DynamicMatrix() {
			Clear();
		}

		void Clear() {
			delete[] m_Data;
			m_Data = nullptr;
			m_Extent = 0;
		}

		uint32_t Size() const {
			return m_Extent.x * m_Extent.y;
		}

		Vec2_T<uint32_t> Extent() const {
			return m_Extent;
		}
		
		uint32_t Rows() const {
			return m_Extent.x;
		}

		uint32_t Columns() const {
			return m_Extent.y;
		}

		T* operator[](uint32_t row) {
			if (row >= m_Extent.x) {
				CriticalError(ErrorOrigin::IndexOutOfBounds, 
					"attempting to access row that's outside the bounds of dynamic matrix!");
			}
			uint32_t index = row * m_Extent.x;
			assert(index < Size());
			return m_Data + index;
		}

		const T* operator[](uint32_t row) const {
			if (row >= m_Extent.x) {
				CriticalError(ErrorOrigin::IndexOutOfBounds, 
					"attempting to access row that's outside the bounds of dynamic matrix!");
			}
			uint32_t index = row * m_Extent.x;
			assert(index < Size());
			return m_Data + index;
		}
	};

	template<typename T, typename Hash = T, typename Compare = T, size_t BucketCapacity = 4>
	class Set {
	public:

		typedef T Bucket[BucketCapacity];

		struct Iterator;

		struct ConstIterator {
		private:

			const Set& m_Set;
			DynamicArray<size_t>::ConstIterator m_IndexIter;
			const size_t m_BucketIndex;

		public:

			ConstIterator(const Set& set, DynamicArray<size_t>::ConstIterator indexIter, size_t bucketIndex) noexcept 
				: m_Set(set), m_IndexIter(indexIter), m_BucketIndex(bucketIndex) {}

			ConstIterator(const ConstIterator& other) noexcept 
				: m_Set(other.m_Set), m_IndexIter(other.m_IndexIter), m_BucketIndex(other.m_BucketIndex) {}

			T& operator*() const {
				assert(m_IndexIter != nullptr && m_BucketIndex < BucketCapacity 
					&& "error in Engine::Set::ConstIterator (attempting to dereference an invalid iterator!)");
				return m_Set.m_Buckets[*m_IndexIter][m_BucketIndex];
			}

			friend struct Iterator;
		};

		struct Iterator {
		private:

			const Set& m_Set;
			DynamicArray<size_t>::ConstIterator m_IndexIter;
			size_t m_BucketIndex;

		public:

			Iterator(const Set& set) noexcept 
				: m_Set(set), m_IndexIter(nullptr), m_BucketIndex(BucketCapacity) {
				m_IndexIter = m_Set.m_BucketIndices.begin();
				m_BucketIndex = BucketCapacity;
				while (m_IndexIter != m_Set.m_BucketIndices.end()) {
					if (m_Set.m_BucketSizes[*m_IndexIter]) {
						m_BucketIndex = 0;
						break;
					}
					++m_IndexIter;
				}
			}

			Iterator(const Iterator& other) noexcept
				: m_Set(other.m_Set), m_IndexIter(other.m_IndexIter), m_BucketIndex(other.m_BucketIndex) {}

			Iterator& operator++() noexcept {
				if (m_Set.m_BucketSizes[*m_IndexIter] > m_BucketIndex + 1) {
					++m_BucketIndex;
					return *this;
				}
				++m_IndexIter;
				while (m_IndexIter != m_Set.m_BucketIndices.end()) {
					if (m_Set.m_BucketSizes[*m_IndexIter]) {
						m_BucketIndex = 0;
						return *this;
					}
					++m_IndexIter;
				}
				m_BucketIndex = BucketCapacity;
				return *this;
			}

			Iterator& operator--() noexcept {
				if (m_BucketIndex > 0) {
					--m_BucketIndex;
					return *this;
				}
				while (m_IndexIter != m_Set.m_BucketIndices.begin()) {
					if (m_Set.m_BucketSizes[*m_IndexIter]) {
						m_BucketIndex = m_Set.m_BucketSizes[*m_IndexIter] - 1;
						return *this;
					}
					--m_IndexIter;
				}
				if (m_Set.m_BucketSizes[*m_IndexIter]) {
					m_BucketIndex = m_Set.m_BucketSizes[*m_IndexIter] - 1;
					return *this;
				}
				m_BucketIndex = BucketCapacity;
				m_IndexIter = m_Set.m_BucketIndices.end();
				return *this;
			}

			T& operator*() const {
				assert(m_IndexIter != m_Set.m_BucketIndices.end() && m_BucketIndex < BucketCapacity
					&& "error in Engine::Set::Iterator (attempting to dereference an invalid iterator!)");
				return m_Set.m_Buckets[*m_IndexIter][m_BucketIndex];
			}

			bool operator==(const Iterator& other) const noexcept {
				return &m_Set == &other.m_Set && m_IndexIter == other.m_IndexIter && m_BucketIndex == other.m_BucketIndex
					|| (m_IndexIter == m_Set.m_BucketIndices.end() && other.m_IndexIter == m_Set.m_BucketIndices.end());
			}

			bool operator==(const ConstIterator& other) const noexcept {
				return &m_Set == &other.m_Set && m_IndexIter == other.m_IndexIter && m_BucketIndex == other.m_BucketIndex
					|| (m_IndexIter == m_Set.m_BucketIndices.end() && other.m_IndexIter == m_Set.m_BucketIndices.end());
			};
		};

	private:

		Bucket* m_Buckets;
		size_t* m_BucketSizes;
		DynamicArray<size_t> m_BucketIndices;
		size_t m_Capacity;
		size_t m_Size;
		size_t m_Trash;

	public:

		Set() noexcept 
			: m_Buckets(nullptr), m_BucketSizes(nullptr), m_BucketIndices(), m_Capacity(0), m_Size(0), m_Trash(0) {}

		Set(const Set& other) noexcept
			: m_Buckets(nullptr), m_BucketSizes(nullptr), m_BucketIndices(),
				m_Capacity(0), m_Size(0), m_Trash(0) {
			Reserve(other.m_Capacity);
			for (const T& value : other) {
				Insert(value);
			}
		}

		Set(Set&& other) noexcept
			: m_Buckets(other.m_Buckets), m_BucketSizes(other.m_BucketSizes), 
				m_BucketIndices(std::move(other.m_BucketIndices)), m_Capacity(other.m_Capacity),
				m_Size(other.m_Size), m_Trash(other.m_Trash) {
			other.m_Buckets = nullptr;
			other.m_BucketSizes = nullptr;
			other.m_Capacity = 0;
			other.m_Trash = 0;
			other.m_Size = 0;
		}

		~Set() noexcept {
			for (T& value : *this) {
				(&value)->~T();
			}
			free(m_Buckets);
			delete[] m_BucketSizes;
			m_BucketSizes = 0;
			m_BucketIndices.Clear();
			m_Capacity = 0;
			m_Size = 0;
		}

		void Reserve(size_t capacity) {
			if (capacity <= m_Capacity) {
				return;
			}
			Bucket* tempBuckets = (Bucket*)malloc(sizeof(Bucket) * capacity);
			assert(tempBuckets && "failed to allocate memory for set!");
			size_t* tempBucketSizes = new size_t[capacity]{};
			DynamicArray<size_t> tempBucketIndices{};
			tempBucketIndices.Reserve(capacity);
			for (T& value : *this) {
				uint64_t hash;
				if constexpr (IsSame<T, Hash>::value) {
					hash = value();
				}
				else {
					hash = Hash()(value);
				}
				size_t index = hash & capacity;
				size_t& bucketSize = tempBucketSizes[index];
				Bucket& bucket = tempBuckets[index];
				if (bucketSize) {
					PrintWarning("bad hash (in Engine::Set::Reserve)!");
				}
				new(&bucket[bucketSize++]) T(std::move(value));
				tempBucketIndices.PushBack(index);
			}
			free(m_Buckets);
			m_Buckets = tempBuckets;
			delete[] m_BucketSizes;
			m_BucketSizes = tempBucketSizes;
			m_BucketIndices.Clear();
			new(&m_BucketIndices) DynamicArray<size_t>(std::move(tempBucketIndices));
			m_Capacity = capacity;
		}	

		T* Insert(const T& value) {
			if (!m_Capacity) {
				Reserve(128);
			}
			if ((float)m_BucketIndices.Size() / m_Capacity >= 0.8) {
				Reserve(m_Capacity * 2);
			}
			uint64_t hash;
			if constexpr (IsSame<T, Hash>::value) {
				hash = value();
			}
			else {
				hash = Hash()(value);
			}
			size_t index = hash & (m_Capacity - 1);
			size_t& bucketSize = m_BucketSizes[index];
			Bucket& bucket = m_Buckets[index];
			if (bucketSize) {
				for (size_t i = 0; i < bucketSize; i++) {
					if constexpr (IsSame<T, Compare>::value) {
						if (bucket[i] == value) {
							return nullptr;
						}
					}	
					else {
						if (Compare::Eq(bucket[i], value)) {
							return nullptr;
						}
					}
				}
				PrintWarning("bad hash (in function Engine::Set::Insert)!");
				if (bucketSize == BucketCapacity) {
					return nullptr;
				}
			}
			new(&bucket[bucketSize]) T(value);
			m_BucketIndices.PushBack(index);
			++m_Size;
			return &bucket[bucketSize++];
		}

		template<typename... Args>
		T* Emplace(Args&&... args) {
			if (!m_Capacity) {
				Reserve(128);
			}
			if ((float)m_BucketIndices.Size() / m_Capacity >= 0.8) {
				Reserve(m_Capacity * 2);
			}
			T value(std::forward(args)...);
			uint64_t hash;
			if constexpr (IsSame<T, Hash>::value) {
				hash = value();
			}
			else {
				hash = Hash()(value);
			}
			size_t index = hash & (m_Capacity - 1);
			size_t& bucketSize = m_BucketSizes[index];
			Bucket& bucket = m_Buckets[index];
			if (bucketSize) {
				for (size_t i = 0; i < bucketSize; i++) {
					if constexpr (IsSame<T, Compare>::value) {
						if (bucket[i] == value) {
							return nullptr;
						}
					}	
					else {
						if (Compare::Eq(bucket[i], value)) {
							return nullptr;
						}
					}
				}
				PrintWarning("bad hash (in function Engine::Set::Emplace)!");
				if (bucketSize == BucketCapacity) {
					return nullptr;
				}
			}
			new(&bucket[bucketSize]) T(std::move(value));
			m_BucketIndices.PushBack(index);
			++m_Size;
			return &bucket[bucketSize++];
		}

		void CleanUp() {
			for (auto iter = m_BucketIndices.begin(); iter != m_BucketIndices.end();) {
				if (!m_BucketSizes[*iter]) {
					iter = m_BucketIndices.Erase(iter);
					continue;
				}
				++iter;
			}
			m_Trash = 0;
		}

		bool Erase(const T& value) {
			uint64_t hash;
			if constexpr (IsSame<T, Hash>::value) {
				hash = value();
			}
			else {
				hash = Hash()(value);
			}
			size_t index = hash & (m_Capacity - 1);
			size_t& bucketSize = m_BucketSizes[index];
			Bucket& bucket = m_Buckets[index];
			if (bucketSize) {
				for (size_t i = 0; i < bucketSize; i++) {
					if constexpr (IsSame<T, Compare>::value) {
						if (bucket[i] == value) {
							(&bucket[i])->~T();
							--bucketSize;
							++m_Trash;
							--m_Size;
							if ((float)m_Trash / m_Capacity >= 0.25) {
								CleanUp();
							}
							return true;
						}
					}
					else {
						if (Compare::Eq(bucket[i], value)) {
							(&bucket[i])->~T();
							--bucketSize;
							++m_Trash;
							--m_Size;
							if ((float)m_Trash / m_Capacity >= 0.25) {
								CleanUp();
							}
							return true;
						}
					}
				}
			}
			return false;
		}

		Iterator begin() const noexcept {
			return Iterator(*this);
		}

		ConstIterator end() const noexcept {
			return ConstIterator(*this, m_BucketIndices.end(), BucketCapacity);
		}
	};

	class String {
	private:
		
		static constexpr uint32_t small_string_buffer_size = 16;

		char* m_Data;
		char m_SmallStringBuffer[small_string_buffer_size];
		uint32_t m_Length;
		uint32_t m_Capacity;

	public:

		constexpr String() noexcept : m_Data(nullptr), m_Length(0), m_Capacity(0) {}

		constexpr String(String&& other) noexcept
			: m_Data(other.m_Data), m_Length(other.m_Length), m_Capacity(other.m_Capacity) {
			other.m_Data = nullptr;
			other.m_Length = 0;
			other.m_Capacity = 0;
		}

		String(const char* str) noexcept {
			uint32_t len = strlen(str);
			if (len < small_string_buffer_size) {
				m_Data = m_SmallStringBuffer;
			}
			else {
				Reserve(len * 2);
			}
			for (uint32_t i = 0; i != len; i++) {
				m_Data[i] = str[i];
			}
			m_Length = len;
			m_Data[m_Length] = '\0';
		}

		String(char* buf, uint32_t begin, uint32_t end) {
			assert(begin < end && "erroneous arguments for Engine::String constructor!");
			m_Length = end - begin;
			if (m_Length < small_string_buffer_size) {
				m_Data = m_SmallStringBuffer;
			}
			else {
				Reserve(m_Length * 2);
			}
			for (; begin != end; begin++) {
				m_Data[begin] = buf[begin];
			}
			m_Data[m_Length] = '\0';
		}

		~String() {
			Clear();
		}

		uint32_t Length() const {
			return m_Length;
		}

		const char* CString() const {
			return m_Data;
		}

		String& Reserve(uint32_t capacity) {
			if (capacity < m_Capacity) {
				return *this;
			}
			if (capacity < 16) {
				m_Data = m_SmallStringBuffer;
				return *this;
			}
			char* temp = (char*)malloc(sizeof(char) * capacity);
			for (uint32_t i = 0; i < m_Length; i++) {
				temp[i] = m_Data[i];
			}
			temp[m_Length] = '\0';
			free(m_Data);
			m_Data = temp;
			m_Capacity = capacity;
			return *this;
		}

		String& Clear() {
			if (m_Length >= small_string_buffer_size) {
				free(m_Data);
			}
			m_Data = nullptr;
			m_Length = 0;
			m_Capacity = 0;
			return *this;
		}

		String& Push(char c) {
			if (c == '\0') {
				return *this;
			}
			if (m_Capacity <= m_Length) {
				Reserve(m_Capacity ? m_Capacity * 2 : 15);
			}
			m_Data[m_Length++] = c;
			return *this;
		}

		String& operator=(const char* other) {
			Clear();
			uint32_t len = strlen(other);
			if (len < small_string_buffer_size) {
				m_Data = m_SmallStringBuffer;
			}
			else {
				Reserve(len * 2);
			}
			for (uint32_t i = 0; i < len; i++) {
				m_Data[i] = other[i];
			}
			m_Length = len;
			m_Data[m_Length] = '\0';
			return *this;
		}

		char& operator[](uint32_t index) {
			if (index >= m_Length) {
				CriticalError(ErrorOrigin::IndexOutOfBounds, 
					"string index was out of bounds (in String::operator[])!");
			}
			return m_Data[index];
		}

		char operator[](uint32_t index) const {
			if (index >= m_Length) {
				PrintError(ErrorOrigin::IndexOutOfBounds, 
					"string index was out of bounds (in String::operator[])!");
				return 0;
			}
			return m_Data[index];
		}

		String& operator +=(char c) {
			return Push(c);
		}

		uint64_t operator()() {
			if (!m_Length) {
				return 0;
			}
			uint64_t res = 37;
			for (uint32_t i = 0; i < m_Length; i++) {
				res = (res * 54059) ^ ((uint64_t)m_Data[i] * 76963);
			}
			return res;
		}

		friend bool operator==(const String& a, const char* b) {
			return a.m_Data && !strcmp(a.m_Data, b);
		}
		
		friend bool operator==(const char* a, const String& b) {
			return b.m_Data && !strcmp(a, b.m_Data);
		}

		struct Hash {
			uint64_t operator()(const char* str) {
				if (!str) {
					return 0;
				}
				uint64_t res = 37;
				for (char c = str[0]; c; c++) {
					res = (res * 54059) ^ ((uint64_t)c * 76963);
				}
				return res;
			}
		};	

		struct Compare {

			static bool Eq(const char* a, const char* b) {
				return !strcmp(a, b);
			}

			static bool NotEq(const char* a, const char* b) {
				return strcmp(a, b);
			}
		};
	};

	template<typename T, typename U>
	struct Tuple {
		T first;
		U second;
	};

	template<typename T>
	class Dictionary {
	public:

		static constexpr uint32_t max_bucket_size = 4;

		typedef String KeyType;
		typedef T ValueType;
		typedef KeyType KeyBucket[max_bucket_size];
		typedef ValueType ValueBucket[max_bucket_size];

	private:

		class IteratorBase {

			friend class Dictionary<T>;

		private:

			const Dictionary& m_Dictionary;
			uint32_t m_Index;
			uint32_t m_BucketIndex;

		public:

			IteratorBase(const Dictionary& dictionary, uint32_t index, uint32_t bucketIndex)
				: m_Dictionary(dictionary), m_Index(index), m_BucketIndex(bucketIndex) {
				if (!m_Dictionary.m_BucketSizes[m_Index]) {
					++*this;
				}
			}

			IteratorBase& operator++() {
				if (m_Index < m_Dictionary.m_Capacity) {
					++m_BucketIndex;
					if (m_BucketIndex < m_Dictionary.m_BucketSizes[m_Index]) {
						return *this;
					}
					m_BucketIndex = 0;
					++m_Index;
					while (m_Index < m_Dictionary.m_Capacity && !m_Dictionary.m_BucketSizes[m_Index]) {
						++m_Index;
					}
				}
				return *this;
			}

			bool operator==(const IteratorBase& other) const {
				return m_Index == other.m_Index && m_BucketIndex == other.m_BucketIndex;
			}
		};

	public:

		class Iterator : public Dictionary<T>::IteratorBase {

			friend class Dictionary<T>;

		private:

			Iterator(const Dictionary& dictionary, uint32_t index, uint32_t bucketIndex) 
				: IteratorBase(dictionary, index, bucketIndex) {
				if (!IteratorBase::m_Dictionary.m_BucketSizes[IteratorBase::m_Index]) {
					++*this;
				}
			}

		public:

			Tuple<const KeyType&, T&> operator*() {
				assert(IteratorBase::m_Index < IteratorBase::m_Dictionary.m_Capacity);
				return { 
					IteratorBase::m_Dictionary.m_KeyBuckets[IteratorBase::m_Index][IteratorBase::m_BucketIndex],
					IteratorBase::m_Dictionary.m_ValueBuckets[IteratorBase::m_Index][IteratorBase::m_BucketIndex],
				};
			}
		};

		class ConstIterator : public IteratorBase {

			friend class Dictionary<T>;

		private:

			ConstIterator(const Dictionary& dictionary, uint32_t index, uint32_t bucketIndex)
				: IteratorBase(dictionary, index, bucketIndex) {
				if (!IteratorBase::m_Dictionary.m_BucketSizes[IteratorBase::m_Index]) {
					++*this;
				}
			}

		public:

			Tuple<const KeyType&, const T&> operator*() const {
				assert(IteratorBase::m_Index < IteratorBase::m_Dictionary.m_Capacity);
				return {
					IteratorBase::m_Dictionary.m_KeyBuckets[IteratorBase::m_Index][IteratorBase::m_BucketIndex],
					IteratorBase::m_Dictionary.m_ValueBuckets[IteratorBase::m_Index][IteratorBase::m_BucketIndex],
				};
			}
		};

	private:

		uint32_t m_Capacity;
		KeyBucket* m_KeyBuckets;
		ValueBucket* m_ValueBuckets;
		uint8_t* m_BucketSizes;
		uint32_t m_Size;

	public:

		Dictionary() noexcept 
			: m_Capacity(0), m_KeyBuckets(nullptr), m_ValueBuckets(nullptr),
				m_BucketSizes(nullptr) {}

		Dictionary(uint32_t capacity) 
			: m_Capacity(capacity), m_KeyBuckets((KeyBucket*)malloc(m_Capacity * sizeof(KeyBucket))),
				m_ValueBuckets((ValueBucket*)malloc(m_Capacity * sizeof(ValueBucket))), m_Size(0),
				m_BucketSizes((uint8_t*)malloc(m_Capacity * sizeof(uint8_t)))
		{
			assert(m_KeyBuckets && m_ValueBuckets && m_BucketSizes);
			memset(m_KeyBuckets, 0, m_Capacity * sizeof(KeyBucket));	
			memset(m_ValueBuckets, 0, m_Capacity * sizeof(ValueBucket));
			memset(m_BucketSizes, 0, m_Capacity * sizeof(uint8_t));
		}

		Dictionary(Dictionary&& other) 
			: m_Capacity(other.m_Capacity), m_KeyBuckets(other.m_KeyBuckets), 
				m_ValueBuckets(other.m_ValueBuckets), m_BucketSizes(other.m_BucketSizes), m_Size(other.m_Size) {
			other.m_Capacity = 0;
			other.m_KeyBuckets = nullptr;
			other.m_ValueBuckets = nullptr;
			other.m_BucketSizes = nullptr;
			other.m_Size = 0;
		}

		~Dictionary() {
			Clear();
		}

		Dictionary& Reserve(uint32_t capacity) {
			if (m_Capacity == 0 && capacity == 0) {
				capacity = 128;
			}
			if (capacity < m_Capacity) {
				return *this;
			}
			Dictionary<T> temp(capacity);
			for (size_t i = 0; i < m_Capacity; i++) {
				uint8_t bucketSize = m_BucketSizes[i];
				for (size_t j = 0; j < bucketSize; j++) {
					temp.Emplace(m_KeyBuckets[i][j].CString(), std::move(m_ValueBuckets[i][j]));
				}
			}
			this->~Dictionary();
			new (this) Dictionary(Dictionary(std::move(temp)));
			return *this;
		}

		void Clear() {
			m_Capacity = 0;
			free(m_KeyBuckets);
			m_KeyBuckets = nullptr;
			free(m_ValueBuckets);
			m_ValueBuckets = nullptr;
			free(m_BucketSizes);
			m_BucketSizes = nullptr;
			m_Size = 0;
		}

		bool Contains(const char* key) const {
			uint64_t hash = String::Hash()(key);
			uint32_t index = hash % m_Capacity;
			uint8_t bucketSize = m_BucketSizes[index];
			KeyBucket& keyBucket = m_KeyBuckets[index];
			for (uint32_t i = 0; i < bucketSize; i++) {
				if (key == keyBucket[i]) {
					return true;
				}
			}
			return false;
		}

		ValueType* Find(const char* key) const {
			uint64_t hash = String::Hash()(key);
			uint32_t index = hash % m_Capacity;
			uint8_t bucketSize = m_BucketSizes[index];
			KeyBucket& keyBucket = m_KeyBuckets[index];
			for (uint32_t i = 0; i < bucketSize; i++) {
				if (key == keyBucket[i]) {
					return &m_ValueBuckets[index][i];
				}
			}
			return nullptr;
		}

		template<typename... Args>
		ValueType* Emplace(const char* key, Args&&... args) {
			if ((float)m_Size / m_Capacity > 0.9f) {
				Reserve(m_Capacity * 2);
			}
			uint64_t hash = String::Hash()(key);
			uint32_t index = hash % m_Capacity;
			uint8_t& bucketSize = m_BucketSizes[index];
			KeyBucket& keyBucket = m_KeyBuckets[index];
			ValueBucket& valueBucket = m_ValueBuckets[index];
			if (bucketSize) {
				for (uint32_t i = 0; i < bucketSize; i++) {
					if (key == keyBucket[i]) {
						return nullptr;
					}
				}
				PrintError(ErrorOrigin::UI, "bad hash (in function UI::Dictionary::Emplace)!");
				if (bucketSize >= max_bucket_size) {
					PrintError(ErrorOrigin::UI, "bucket was full (in function UI::Dictionary::Emplace)!");
					return nullptr;
				}
			}
			++m_Size;
			keyBucket[bucketSize] = key;
			return new(&valueBucket[bucketSize++]) ValueType(std::forward<Args>(args)...);
		}

		ValueType* Insert(const char* key, const ValueType& value) {
			if ((float)m_Size / m_Capacity > 0.9f) {
				Reserve(m_Capacity * 2);
			}
			uint64_t hash = String::Hash()(key);
			uint32_t index = hash % m_Capacity;
			uint8_t& bucketSize = m_BucketSizes[index];
			KeyBucket& keyBucket = m_KeyBuckets[index];
			ValueBucket& valueBucket = m_ValueBuckets[index];
			if (bucketSize) {
				for (uint32_t i = 0; i < bucketSize; i++) {
					if (key == keyBucket[i]) {
						return nullptr;
					}
				}
				PrintError(ErrorOrigin::UI, "bad hash (in function UI::Dictionary::Insert)!");
				if (bucketSize >= max_bucket_size) {
					PrintError(ErrorOrigin::UI, "bucket was full (in function UI::Dictionary::Insert)!");
					return nullptr;
				}
			}
			++m_Size;
			keyBucket[bucketSize] = key;
			return &(valueBucket[bucketSize++] = value);
		}

		Iterator begin() {
			return Iterator(*this, 0, 0);
		}

		ConstIterator begin() const {
			return Iterator(*this, 0, 0);
		};

		ConstIterator end() const {
			return ConstIterator(*this, m_Capacity, 0);
		}
	};

	enum class MeshFileType {
		Unrecognized = 0,
		Obj = 1,
	};

	inline MeshFileType GetMeshFileType(const String& string) {
		uint32_t length = string.Length();
		uint32_t i = length - 1;
		for (; i > 0; i--) {
			if (string[i] == '.') {
				break;
			}
		}
		if (string[i] != '.') {
			return MeshFileType::Unrecognized;
		}
		String extension{};
		for (uint32_t j = i + 1; j < length; j++) {
			extension += string[i];
		}
		if (extension == "obj") {
			return MeshFileType::Obj;
		}
		return MeshFileType::Unrecognized;
	}

	class FileHandler {
	public:

		static int Skip(FILE* fs, uint32_t delimiterCount, const char* delimiters) {
			char c = fgetc(fs);
			while (true) {
				if (c == EOF) {
					return EOF;
				}
				for (uint32_t i = 0; i < delimiterCount; i++) {
					if (c == delimiters[i]) {
						return c;
					}
				}	
				c = fgetc(fs);
			}
			return c;
		}

		static int GetLine(FILE* fs, String& os) {
			char c = fgetc(fs);
			for (; c != '\n'; c = fgetc(fs)) {
				if (c == EOF) {
					return EOF;
				}
				os.Push(c);
			}
			return c;
		}
	};

	struct Vertex {

		static const VkPipelineVertexInputStateCreateInfo& GetVertexInputState() {
			static constexpr VkVertexInputBindingDescription binding {
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};
			static constexpr uint32_t attribute_count = 5;
			static constexpr VkVertexInputAttributeDescription attributes[attribute_count] {
				{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Position) },
				{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Normal) },
				{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, m_UV) },
				{ .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Tangent) },
				{ .location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Bitangent) },
			};
			static constexpr VkPipelineVertexInputStateCreateInfo res 
				= Renderer::GraphicsPipelineDefaults::GetVertexInputStateInfo(1, &binding, attribute_count, attributes);
			return res;
		}

		static void SetPosition(Vertex& vertex, const Vec3& pos) {
			vertex.m_Position = pos;
		}

		static void SetUV(Vertex& vertex, const Vec2& UV) {
			vertex.m_UV = UV;
		}

		static void SetNormal(Vertex& vertex, const Vec3& normal) {
			vertex.m_Normal = normal;
		}

		Vec3 m_Position{};
		Vec3 m_Normal{};
		Vec2 m_UV{};
		Vec3 m_Tangent{};
		Vec3 m_Bitangent{};

		bool operator==(const Vertex& other) const noexcept = default;
	};

	struct Vertex2D {

		static const VkPipelineVertexInputStateCreateInfo& GetVertexInputState() {
			static constexpr VkVertexInputBindingDescription binding {
				.binding = 0,
				.stride = sizeof(Vertex2D),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			};
			static constexpr uint32_t attribute_count = 2;
			static constexpr VkVertexInputAttributeDescription attributes[attribute_count] {
				{
					.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex2D, m_Position),
				},
				{
					.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex2D, m_UV),
				},
			};
			static constexpr VkPipelineVertexInputStateCreateInfo res 
				= Renderer::GraphicsPipelineDefaults::GetVertexInputStateInfo(1, &binding, attribute_count, attributes);
			return res;
		}

		Vec3 m_Position{};
		Vec2 m_UV{};
	};

	struct Obj {

		uint32_t m_LinesParsed = 0;

		DynamicArray<Vec3> m_Vs;
		DynamicArray<Vec2> m_Vts;
		DynamicArray<Vec3> m_Vns;

		DynamicArray<uint32_t> m_VIndices{};
		DynamicArray<uint32_t> m_VtIndices{};
		DynamicArray<uint32_t> m_VnIndices{};

		bool Load(FILE* fileStream) {
			if (!fileStream) {
				return false;
			}
			char buf[3];
			buf[2] = '\0';
			uint32_t maxVIndex{}, maxVtIndex{}, maxVnIndex {};
			while (true) {
				buf[0] = fgetc(fileStream);
				if (buf[0] == EOF) {
					break;
				}
				if (buf[0] == '\n') {
					continue;
				}
				++m_LinesParsed;
				buf[1] = fgetc(fileStream);
				if (buf[1] == EOF) {
					break;
				}
				if (buf[1] == ' ') {
					buf[1] = 0;
				}
				if (buf[0] == 'v') {
					if (buf[1]) {
						if (buf[1] == 't') {
							long fPos = ftell(fileStream);
							Vec2& vt = m_Vts.EmplaceBack();
							if (fscanf(fileStream, "%f%f", &vt.x, &vt.y) != 2) {
								fseek(fileStream, fPos, SEEK_SET);
							}
							for (char c = fgetc(fileStream); c != '\n' && c != EOF;) {
								c = fgetc(fileStream);
							}
						}
						else if (buf[1] == 'n') {
							long fPos = ftell(fileStream);
							Vec3& vn = m_Vns.EmplaceBack();
							if (fscanf(fileStream, "%f%f%f", &vn.x, &vn.y, &vn.z) != 3) {
								fseek(fileStream, fPos, SEEK_SET);
							}
							for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
						}
					}
					else {
						long fPos = ftell(fileStream);
						Vec3& v = m_Vs.EmplaceBack();
						if (fscanf(fileStream, "%f%f%f", &v.x, &v.y, &v.z) != 3) {
							fseek(fileStream, fPos, SEEK_SET);
						}
						for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
					}
					continue;
				}
				if (buf[0] == 'f') {
					long fPos = ftell(fileStream);
					bool failed = false;
					for (size_t i = 0; i < 3; i++) {
						uint32_t& vI = m_VIndices.EmplaceBack();
						int res = fscanf(fileStream, "%i", &vI);
						if (res != 1) {
							m_LinesParsed = 0;
							return false;
						}
						--vI;
						maxVIndex = Max(vI, maxVIndex);
						if (fgetc(fileStream) == '/') {
							uint32_t vtI;
							if (fscanf(fileStream, "%i", &vtI) == 1) {
								m_VtIndices.PushBack(--vtI);
								maxVtIndex = Max(vtI, maxVtIndex);
							}
						}
						if (fgetc(fileStream) == '/') {
							uint32_t& vnI = m_VnIndices.EmplaceBack();
							if (fscanf(fileStream, "%i", &vnI) != 1) {
								m_LinesParsed = 0;
								return false;
							}
							--vnI;
							maxVnIndex = Max(vnI, maxVnIndex);
						}
					}
					for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
					continue;
				}
				for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
			}
			if (!((!m_VtIndices.Size() || m_VIndices.Size() == m_VtIndices.Size()) &&
				(!m_VnIndices.Size() || m_VIndices.Size() == m_VnIndices.Size()) && !(m_VtIndices.Size() % 3) &&
				maxVIndex < m_Vs.Size()&& (!m_VtIndices.Size()|| maxVtIndex < m_Vts.Size()) && (!m_VnIndices.Size()|| maxVnIndex < m_Vns.Size()))) {
				m_LinesParsed = 0;
				return false;
			}
			return true;
		}

		template<typename VertexType>
		bool GetMesh(void (*setPos)(Vertex&, const Vec3&), void (*setUV)(Vertex&, const Vec2&), 
				void (*setNormal)(Vertex&, const Vec3&), DynamicArray<VertexType>& outVertices, DynamicArray<uint32_t>& outIndices) const {
			if (m_LinesParsed == 0) {
				PrintError(ErrorOrigin::FileParsing, 
					"attempting to get vertices from Engine::Obj which failed to parse (in function Obj::GetVertices)!");
				return false;
			}
			if (!setPos || !setUV || !setNormal && m_VnIndices.Size()) {
				PrintError(ErrorOrigin::FileParsing, 
					"attempting to get vertices from an obj when a set function is null!");
				return false;
			}
			outVertices.Reserve(m_Vs.Size());
			outIndices.Reserve(m_VIndices.Size());
			for (uint32_t i = 0; i < m_VIndices.Size(); i++) {
				VertexType newVertex{};
				setPos(newVertex, m_Vs[m_VIndices[i]]);
				if (m_VtIndices.Size()) {
					setUV(newVertex, m_Vts[m_VtIndices[i]]);
				}
				if (m_VnIndices.Size()) {
					setNormal(newVertex, m_Vns[m_VnIndices[i]]);
				}
				size_t j = 0;
				for (; j < outVertices.Size(); j++) {
					if (outVertices[j] == newVertex) {
						break;
					}
				}
				if (j == outVertices.Size()) {
					outVertices.PushBack(newVertex);
				}
				outIndices.PushBack(j);
			}
			return true;
		}
	};


	bool LoadImage(const char* fileName, uint32_t components, uint8_t*& outImage, Vec2_T<uint32_t>& outExtent);

	inline void FreeImage(uint8_t* image) {
		free(image);
	}

	template<typename T>
	struct Rect {

		Vec2_T<T> m_Min{};
		Vec2_T<T> m_Max{};

		template<typename U>
		bool IsPointInside(Vec2_T<U> point) const {
			return point.x > m_Min.x && point.y > m_Min.y &&
				point.x < m_Max.x && point.y < m_Max.y;
		}

		template<typename U>
		bool OverLaps(const Rect<U>& other) const {
			return m_Max.x > other.m_Min.x && other.m_Max.x > m_Min.x &&
				m_Max.y > other.m_Min.y && other.m_Max.y > m_Min.y;
		}

		Vec2_T<T> Dimensions() const {
			return m_Max - m_Min;
		}
	};

	template<typename T>
	struct Box {

		Vec3_T<T> m_Min{};
		Vec3_T<T> m_Max{};

		bool IsPointInside(const Vec3& point) const {
			return point.x > m_Min.x && point.y > m_Min.y && point.z > m_Min.z &&
				point.x < m_Max.x && point.y < m_Max.y && point.z < m_Max.z;
		}

		bool OverLaps(const Box& other) const {
			return m_Max.x > other.m_Min.x && other.m_Max.x > m_Min.x &&
				m_Max.y > other.m_Min.y && other.m_Max.y > m_Min.y &&
				m_Max.z > other.m_Min.z && other.m_Max.z > m_Min.z;
		}

		Vec3 Dimensions() const {
			return m_Max - m_Min;
		}
	};

	class Collider {

		friend class Engine;
	
	public:

		enum class Type {
			None = 0,
			Fence = 1,
			Pole = 2,
		};

		struct Fence {

			struct CreateInfo {

				Vec3 m_Dimensions{};
				float m_YRotation{};

				static bool FromFile(FILE* fileStream, CreateInfo& outInfo) {
					return fscanf(fileStream, "{%f%f%f%f}", 
							&outInfo.m_Dimensions.x, &outInfo.m_Dimensions.y, &outInfo.m_Dimensions.z,
							&outInfo.m_YRotation) == 4;
				}
			};

			void Create(const CreateInfo& info) {
				m_HalfDimensions = info.m_Dimensions / 2;
				m_YRotation = info.m_YRotation;
			}

			Vec3 m_HalfDimensions{};
			float m_YRotation{};
		};

		struct Pole {

			struct CreateInfo {

				float m_Radius{};
				float m_Height{};

				static bool FromFile(FILE* fileStream, CreateInfo& outInfo) {
					return fscanf(fileStream, "{%f%f}", 
						&outInfo.m_Radius, &outInfo.m_Height) == 2;
				}
			};

			void Create(const CreateInfo& info) {
				m_Radius = info.m_Radius;
				m_HalfHeight = info.m_Height / 2;
			}

			float m_Radius{};
			float m_HalfHeight{};
		};

		union TypeCreateInfo_U {
			Fence::CreateInfo m_FenceInfo;
			Pole::CreateInfo m_PoleInfo;
		};

		union Collider_U {

			Collider_U(Type type, const TypeCreateInfo_U& typeInfo) {
				switch (type) {
					case Type::Fence:
						m_Fence.Create(typeInfo.m_FenceInfo);
						break;
					case Type::Pole:
						m_Pole.Create(typeInfo.m_PoleInfo);
						break;
					case Type::None:
						break;
				}
			}

			Fence m_Fence;
			Pole m_Pole;
		};

		struct CreateInfo {

			Vec3 m_LocalPosition;
			Type m_Type;
			TypeCreateInfo_U u_TypeInfo;

			static bool FromFile(FILE* fileStream, CreateInfo& outInfo) {
				int res = fscanf(fileStream, "{%f%f%f\n",
					&outInfo.m_LocalPosition.x, &outInfo.m_LocalPosition.y, &outInfo.m_LocalPosition.z);
				if (res != 3) {
					return false;
				}
				char type = fgetc(fileStream);
				switch (type) {
					case 'F':
						if (!Fence::CreateInfo::FromFile(fileStream, outInfo.u_TypeInfo.m_FenceInfo)) {
							PrintError(ErrorOrigin::FileParsing, 
								"failed to parse fence collider (function Collider::Fence::CreateInfo::FromFile)!");
							return false;
						}
						break;
					case 'P':
						if (!Pole::CreateInfo::FromFile(fileStream, outInfo.u_TypeInfo.m_PoleInfo)) {
							PrintError(ErrorOrigin::FileParsing, 
								"failed to parse pole collider (function Collider::Pole::CreateInfo::FromFile)!");
							return false;
						}
						break;
					default:
						return false;
				}
				char c = fgetc(fileStream);
				while (c != ' ' || c != '\n') {
					c = fgetc(fileStream);
				}
				if (c != '}') {
					PrintError(ErrorOrigin::FileParsing, 
						"missing '}' when parsing collider (function Collider::CreateInfo::FromFile)!");
					return false;
				}
				return true;
			}
		};

	private:

		const Type m_Type;
		Collider_U u_Collider;
		Vec3 m_LocalPosition{};
		const Vec3& m_BodyPosition;
		const float& m_BodyYRotation;

	public:

		Collider(Vec3& bodyPosition, float& bodyYRotation, const CreateInfo& info) 
			: m_BodyPosition(bodyPosition), m_BodyYRotation(bodyYRotation), 
				m_LocalPosition(info.m_LocalPosition), m_Type(info.m_Type), u_Collider(info.m_Type, info.u_TypeInfo) {}

		Collider(Vec3& bodyPosition, float& bodyYRotation, Collider&& other) 
			: m_Type(other.m_Type), m_BodyYRotation(bodyYRotation), u_Collider(other.u_Collider), m_LocalPosition(other.m_LocalPosition),
				m_BodyPosition(bodyPosition) {
			other.~Collider();
		}

		Collider(const Collider& other) = delete;
		Collider(Collider&& other) = delete;

	public:

		static bool PoleToStaticPoleCollides(const Collider& a, const Collider& b, Vec3& outAPushBack) {
			const Pole& aPole = a.u_Collider.m_Pole;
			const Pole& bPole = b.u_Collider.m_Pole;
			const Vec3 aPos = a.m_BodyPosition + a.m_LocalPosition;
			const Vec3 bPos = b.m_BodyPosition + b.m_LocalPosition;
			float yMaxs[2] {
				aPos.y + aPole.m_HalfHeight,
				bPos.y + bPole.m_HalfHeight,
			};
			float yMins[2] { 
				aPos.y - aPole.m_HalfHeight,
				bPos.y - bPole.m_HalfHeight,
			};
			if (yMaxs[0] > yMins[1] && yMaxs[1] > yMins[0]) {
				Vec2 centerDiff = Vec2(bPos.x - aPos.x, bPos.z - aPos.z);
				float centerDistanceSqr 
					= centerDiff.SqrMagnitude();
				float radDiff = (aPole.m_Radius - bPole.m_Radius);
				float radSum = (aPole.m_Radius + bPole.m_Radius);
				if (radDiff * radDiff < centerDistanceSqr && centerDistanceSqr < radSum * radSum) {
					outAPushBack = Vec3(centerDiff.x, 0.0f, centerDiff.y).Normalized() * (radSum - sqrt(centerDistanceSqr));
					return true;
				}
			}
			return false;
		}

		static bool PoleToStaticFenceCollides(const Collider& a, const Collider& b, Vec3& outAPushBack) {
			const Pole& aPole = a.u_Collider.m_Pole;
			const Fence& bFence = b.u_Collider.m_Fence;
			const Vec3 aPos = a.m_BodyPosition + a.m_LocalPosition;
			const Vec3 bPos = b.m_BodyPosition + b.m_LocalPosition;
			float yMaxs[2] {
				aPos.y + aPole.m_HalfHeight,
				bPos.y + bFence.m_HalfDimensions.y
			};
			float yMins[2] {
				aPos.y - aPole.m_HalfHeight,
				bPos.y - bFence.m_HalfDimensions.y,
			};
			if (yMaxs[0] > yMins[1] && yMaxs[1] > yMins[0]) {
				Vec2 aPos2D(aPos.x, aPos.z);
				Vec2 bPos2D(bPos.x, bPos.z);
				Vec2 aPosRel = aPos2D - bPos2D;
				Rect<float> boundingRect = {
					.m_Min {
						-bFence.m_HalfDimensions.x,
						-bFence.m_HalfDimensions.z
					},
					.m_Max {
						bFence.m_HalfDimensions.x,
						bFence.m_HalfDimensions.y,
					},
				};
				Mat3 rotationMatrix = Quaternion::AxisRotation(Vec3::Forward(), bFence.m_YRotation + b.m_BodyYRotation).AsMat3();
				Vec3 aPosRelRotated = (Vec3)aPosRel * rotationMatrix;
				if (boundingRect.IsPointInside(aPosRel)) {
					float val = aPosRelRotated.y - boundingRect.m_Min.y;
					float min = val;
					bool xClosest = false;
					val = aPosRelRotated.y - boundingRect.m_Max.y;
					if (abs(val) < abs(min)) {
						min = val;
					}
					val = aPosRelRotated.x - boundingRect.m_Min.x;
					if (abs(val) < abs(min)) {
						min = val;
						xClosest = true;
					}
					val = aPosRelRotated.x - boundingRect.m_Max.x;
					if (abs(val) < abs(min)) {
						min = val;
						xClosest = true;
					}
					Mat3 invRot = Quaternion::AxisRotation(Vec3(0.0f, 0.0f, 1.0f), - (bFence.m_YRotation + b.m_BodyYRotation)).AsMat3();
					if (xClosest) {
						int sign = min < 0.0f ? -1 : 1;
						Vec2 pushBack2D = Vec3(sign * (abs(min) + aPole.m_Radius), 0.0f, 0.0f) * invRot;
						outAPushBack = Vec3(-pushBack2D.x, 0.0f, -pushBack2D.y);
					}
					else {
						int sign = min < 0.0f ? -1 : 1;
						Vec2 pushBack2D = Vec3(0.0f, sign * (abs(min) + aPole.m_Radius), 0.0f) * invRot;
						outAPushBack = Vec3(-pushBack2D.x, 0.0f, -pushBack2D.y);
					}
					return true;
				}
				else {
					Vec2 aClampedPos(
						Clamp(aPosRelRotated.x, boundingRect.m_Min.x, boundingRect.m_Max.x), 
						Clamp(aPosRelRotated.y, boundingRect.m_Min.y, boundingRect.m_Max.y));
					Vec2 diff = aClampedPos - aPosRelRotated;
					float diffSqrMag = diff.SqrMagnitude();
					if (diffSqrMag < aPole.m_Radius * aPole.m_Radius) {
						float diffMag = sqrt(diffSqrMag);
						Mat3 invRot = Quaternion::AxisRotation(Vec3::Forward(), - (bFence.m_YRotation + b.m_BodyYRotation)).AsMat3();
						diff = Vec3(diff) * invRot;
						outAPushBack = Vec3(diff.x / diffMag, 0.0f, diff.y / diffMag) * (aPole.m_Radius - diffMag);
						return true;
					}
				}
			}
			return false;
		}

		static bool FenceToStaticFenceCollides(const Collider& a, const Vec3& aVelocity, const Collider& b, Vec3& outAPushBack) {

			const Fence& aFence = a.u_Collider.m_Fence;
			const Fence& bFence = b.u_Collider.m_Fence;
			const Vec3 aPos = a.m_BodyPosition + a.m_LocalPosition;
			const Vec3 bPos = b.m_BodyPosition + b.m_LocalPosition;

			const float yMaxs[2] {
				aPos.y + aFence.m_HalfDimensions.y,
				bPos.y + bFence.m_HalfDimensions.y,
			};

			const float yMins[2] {
				aPos.y - aFence.m_HalfDimensions.y,
				bPos.y - bFence.m_HalfDimensions.y,
			};

			if (yMaxs[0] > yMins[1] && yMaxs[1] > yMins[0]) {

				const Vec2 aPos2D(aPos.x, aPos.z);
				const Vec2 bPos2D(bPos.x, bPos.z);

				struct Line {
					Vec2 m_Origin;
					Vec2 m_Direction;
				};

				Mat3 rotationMatrix = Quaternion::AxisRotation(Vec3::Forward(), bFence.m_YRotation + b.m_BodyYRotation).AsMat3();

				const Line aLines[2] { 
					{ aPos2D , (Vec3::Right() * rotationMatrix).Normalized() }, 
					{ aPos2D, (Vec3::Up() * rotationMatrix).Normalized() },
				};

				const Vec2 aSides[4] {
					aLines[0].m_Direction * aFence.m_HalfDimensions.x, // RX
					aLines[1].m_Direction * aFence.m_HalfDimensions.z, // RY
					aSides[0] * -1, // RX * -1
					aSides[1] * -1, // RY * - 1
				};

				const Vec2 aCorners[4] {
					aPos2D + aSides[0] + aSides[1],
					aPos2D + aSides[0] + aSides[3],
					aPos2D + aSides[2] + aSides[3],
					aPos2D + aSides[2] + aSides[1],
				};

				const Line bLines[2] { 
					{ bPos2D, (Vec3::Right() * rotationMatrix).Normalized() },
					{ bPos2D, (Vec3::Up() * rotationMatrix).Normalized() },
				};

				const Vec2 bSides[4] {
					bLines[0].m_Direction * bFence.m_HalfDimensions.x, // RX
					bLines[1].m_Direction * bFence.m_HalfDimensions.z, // RY
					bSides[0] * -1, // RX * -1
					bSides[1] * -1, // RY * - 1
				};

				const Vec2 bCorners[4] {
					bPos2D + bSides[0] + bSides[1],
					bPos2D + bSides[0] + bSides[3],
					bPos2D + bSides[2] + bSides[3],
					bPos2D + bSides[2] + bSides[1],
				};

				static constexpr auto project = [](Vec2 vec, const Line& line) -> Vec2 {
					vec -= line.m_Origin;
					float dot = Vec2::Dot(line.m_Direction, vec);
					return line.m_Origin + (line.m_Direction * dot);
				};

				static constexpr auto get_signed_distance = [](Vec2 rectCenter, const Line& line, Vec2 corner, Vec2& outRel) -> float {
					const Vec2 projected = project(corner, line);
					outRel = projected - rectCenter;
					const int sign = (outRel.x * line.m_Direction.x) + (outRel.y * line.m_Direction.y) > 0;
					return outRel.Magnitude() * (sign ? 1 : -1);
				};

				static constexpr auto are_projections_hit = [](const Vec3& rectHalfDimensions, Vec2 minSignedDistances, Vec2 maxSignedDistances) -> bool {
					return (minSignedDistances.x < 0 && maxSignedDistances.x > 0 ||
						abs(minSignedDistances.x) < rectHalfDimensions.x ||
						abs(maxSignedDistances.x) < rectHalfDimensions.x) &&
						(minSignedDistances.y < 0 && maxSignedDistances.y > 0 ||
						abs(minSignedDistances.y) < rectHalfDimensions.z ||
						abs(maxSignedDistances.y) < rectHalfDimensions.z);
				};

				static constexpr float float_max = std::numeric_limits<float>::max();
				static constexpr float float_min = -float_max;

				Vec2 minSignedDistances(float_max, float_max);
				Vec2 maxSignedDistances(float_min, float_min);

				Vec2 relMaxX, relMinX, relMaxY, relMinY;

				for (size_t i = 0; i < 4; i++) {
					Vec2 thisRelX, thisRelY;
					const Vec2 signedDistances(get_signed_distance(aPos2D, aLines[0], bCorners[i], thisRelX), 
						get_signed_distance(aPos2D, aLines[1], bCorners[i], thisRelY));
					minSignedDistances.x = Min(signedDistances.x, minSignedDistances.x);
					minSignedDistances.y = Min(signedDistances.y, minSignedDistances.y);
					maxSignedDistances.x = Max(signedDistances.x, maxSignedDistances.x);
					maxSignedDistances.y = Max(signedDistances.y, maxSignedDistances.y);
					if (signedDistances.x == maxSignedDistances.x) {
						relMaxX = thisRelX;
					}
					if (signedDistances.x == minSignedDistances.x) {
						relMinX = thisRelX;
					}
					if (signedDistances.y == maxSignedDistances.y) {
						relMaxY = thisRelY;
					}
					if (signedDistances.y == minSignedDistances.y) {
						relMinY = thisRelY;
					}
				}

				if (!are_projections_hit(aFence.m_HalfDimensions, minSignedDistances, maxSignedDistances)) {
					return false;
				}

				minSignedDistances = { float_max, float_max };
				maxSignedDistances = { float_min, float_min };

				for (size_t i = 0; i < 4; i++) {
					Vec2 thisRelX, thisRelY;
					const Vec2 signedDistances(get_signed_distance(bPos2D, bLines[0], aCorners[i], thisRelX), 
						get_signed_distance(bPos2D, bLines[1], aCorners[i], thisRelY));
					minSignedDistances.x = Min(signedDistances.x, minSignedDistances.x);
					minSignedDistances.y = Min(signedDistances.y, minSignedDistances.y);
					maxSignedDistances.x = Max(signedDistances.x, maxSignedDistances.x);
					maxSignedDistances.y = Max(signedDistances.y, maxSignedDistances.y);
				}

				if (!are_projections_hit(bFence.m_HalfDimensions, minSignedDistances, maxSignedDistances)) {
					return false;
				}

				static constexpr auto vec_sign = [](Vec2 vec, const Line& line) -> int {
					return (vec.x * line.m_Direction.x) + (vec.y * line.m_Direction.y) > 0 ? 1 : -1;
				};

				Vec2 min = { float_max, float_max };

				Vec2 vec1 = relMaxX.SqrMagnitude() > relMinX.SqrMagnitude() ? relMinX : relMaxX;
				int sign = vec_sign(vec1, aLines[0]);
				Vec2 vec2 = aLines[0].m_Direction * aFence.m_HalfDimensions.x;
				Vec2 vec3 = (vec2 - vec1 * sign) * sign;

				if (vec1.SqrMagnitude() < vec2.SqrMagnitude()) {
					min = vec3;
				}

				vec1 = relMaxY.SqrMagnitude() > relMinY.SqrMagnitude() ? relMinY : relMaxY;
				sign = vec_sign(vec1, aLines[1]);
				vec2 = aLines[1].m_Direction * aFence.m_HalfDimensions.z;
				Vec2 vec4 = (vec2 - vec1 * sign) * sign;

				float threshold = aVelocity.SqrMagnitude() / 2;

				if (vec1.SqrMagnitude() < vec2.SqrMagnitude()) {
					min = Min(min, vec4);
					if (min.SqrMagnitude() < threshold) {
						if (vec4.SqrMagnitude() < threshold) {
							min = vec3;
						}
						else {
							min = vec4;
						}
					}
				}

				outAPushBack = Vec3(min.x, 0.0f, min.y);

				//fmt::print("colliding!");

				return true;
			}
			return false;
		}

		static bool ColliderToStaticColliderCollides(const Collider& a, const Vec3& aVelocity, const Collider& b, Vec3& outAPushBack) {
			if (a.m_Type == Type::Fence) {
				if (b.m_Type == Type::Fence) {
					return FenceToStaticFenceCollides(a, aVelocity, b, outAPushBack);
				}
			}
			if (a.m_Type == Type::Pole) {
				if (b.m_Type == Type::Fence) {
					return PoleToStaticFenceCollides(a, b, outAPushBack);
				}
				if (b.m_Type == Type::Pole) {
					return PoleToStaticPoleCollides(a, b, outAPushBack);
				}
			}
			return false;
		}
	};

	struct Ray {
		Vec3 m_Origin;
		Vec3 m_Direction;
		float m_Length;
	};

	struct RayHitInfo {
		Vec3 m_HitPosition;
		float m_Distance;
	};

	class LogicMesh {
	public:

		typedef Vec3 Face[3];

	private:

		Box<float> m_BoundingBox;
		DynamicArray<Face> m_TransformedFaces;
		DynamicArray<Face> m_Faces;
		DynamicArray<Vec3> m_Vertices;

	public:

		LogicMesh() : m_Faces(), m_TransformedFaces(), m_Vertices() {}

		LogicMesh(const LogicMesh&) = default;
		LogicMesh& operator=(const LogicMesh&) = default;

		LogicMesh(LogicMesh&&) = default;
		LogicMesh& operator=(LogicMesh&&) = default;

		bool Load(const DynamicArray<Vertex>& vertices, const DynamicArray<uint32_t> indices) {
			if (indices.Size() % 3) {
				PrintError(ErrorOrigin::Engine, 
					"indices size must be multiple of 3 when loading mesh (in function LogicMesh::Load)!");
				return false;
			}
			m_Faces.Clear();
			m_TransformedFaces.Clear();
			m_Vertices.Clear();
			for (uint32_t index : indices) {
				if (index >= vertices.Size()) {
					PrintError(ErrorOrigin::Engine, 
						"found invalid index when loading mesh (in function LogicMesh::Load)!");
					return false;
				}
				m_Vertices.PushBack(vertices[index].m_Position);
			}
			uint32_t vertexCount = m_Vertices.Size();
			assert(vertexCount % 3);
			uint32_t faceCount = vertexCount / 3;
			m_Faces.Reserve(faceCount);
			for (uint32_t i = 0; i < vertexCount; i += 3) {
				Face& face = m_Faces.EmplaceBack();
				face[0] = m_Vertices[i];
				face[1] = m_Vertices[i + 1];
				face[2] = m_Vertices[i + 2];
			}
			return true;
		}

		void UpdateTransform(const Mat4& transform) {
			uint32_t faceCount = m_Faces.Size();
			m_TransformedFaces.Resize(faceCount);
			m_BoundingBox = {
				.m_Min {
					float_max,
					float_max,
					float_max,
				},
				.m_Max {
					float_min,
					float_min,
					float_min,
				},
			};
			for (uint32_t i = 0; i < faceCount; i++) {
				Face& face = m_Faces[i];
				Face& transformedFace = m_TransformedFaces[i];
				for (uint32_t j = 0; j < 3; j++) {
					Vec3& pos = transformedFace[j];
					pos = transform * face[j];
					Vec3& max = m_BoundingBox.m_Max;
					max = {
						Max(pos.x, max.x),
						Max(pos.y, max.y),
						Max(pos.z, max.z),
					};
					Vec3& min = m_BoundingBox.m_Min;
					min = {
						Min(pos.x, min.x),
						Min(pos.y, min.y),
						Min(pos.z, min.z),
					};
				}
			}
		}

		const Box<float>& GetBoundingBox() const {
			return m_BoundingBox;
		}

		bool IsRayHit(const Ray& ray, const Face& face, Vec3& outHitPosition, float& outDistance) const {
			Vec3 edge1 = m_Vertices[1] - m_Vertices[0];
			Vec3 edge2 = m_Vertices[2] - m_Vertices[0];
			Vec3 normal = Cross(edge1, edge2);
			float det = -Dot(edge1, edge2);
			if (det == 0.0f) {
				return false;
			}
			float invDet = 1.0f / det;
			Vec3 ao = ray.m_Origin - face[0];
			Vec3 aoXd = Cross(ao, ray.m_Direction);
			float u = Dot(edge2, aoXd) * invDet;
			float v = -Dot(edge1, aoXd) * invDet;
			outDistance = Dot(ao, normal) * invDet;
			outHitPosition = ray.m_Origin + ray.m_Direction * outDistance;
			return det >= 1e-6 && outDistance > 0.0f && outDistance <= ray.m_Length 
				&& u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f;
		}

		bool IsRayHit(const Ray& ray, RayHitInfo& outInfo) const {
			outInfo = {
				{},
				float_max,
			};
			bool wasHit = false;
			for (const Face& face : m_TransformedFaces) {
				Vec3 hitPosition;
				float distance = float_max;
				if (IsRayHit(ray, face, hitPosition, distance)) {
					wasHit = true;
					if (distance < outInfo.m_Distance) {
						outInfo = {
							hitPosition,
							distance,
						};
					}
				}
			}
			return wasHit;
		}
	};

	class StaticMesh {

	private:

		uint32_t m_IndexCount;
		Renderer::Buffer m_VertexBuffer;
		Renderer::Buffer m_IndexBuffer;

	public:

		StaticMesh(Renderer& renderer) noexcept : m_VertexBuffer(renderer), 
			m_IndexBuffer(renderer), m_IndexCount(0) {}

		StaticMesh(StaticMesh&& other) noexcept : m_VertexBuffer(std::move(other.m_VertexBuffer)), 
			m_IndexBuffer(std::move(other.m_IndexBuffer)), m_IndexCount(other.m_IndexCount) {}

		StaticMesh(const StaticMesh&) = delete;

		MeshData GetMeshData() const {
			constexpr static VkDeviceSize offset = 0;
			MeshData data {
				.m_VertexBufferCount = 1,
				.m_IndexCount = m_IndexCount,
				.m_VertexBuffers = &m_VertexBuffer.m_Buffer,
				.m_VertexBufferOffsets = &offset,
				.m_IndexBuffer = m_IndexBuffer.m_Buffer,
			};
			return data;
		}

		void Terminate() {
			m_IndexCount = 0;
			m_VertexBuffer.Terminate();
			m_IndexBuffer.Terminate();
		}

		template<typename VertexType>
		bool CreateBuffers(uint32_t vertexCount, const VertexType* pVertices, uint32_t indexCount, const uint32_t* pIndices) {
			if (m_VertexBuffer.m_BufferSize || m_IndexBuffer.m_BufferSize) {
				PrintError(ErrorOrigin::Renderer, 
				"attempting to create vertex and index buffers when the buffers have already been created (in function StaticMesh::CreateBuffers)!");
				return false;
			}
			if (!m_VertexBuffer.CreateWithData(vertexCount * sizeof(VertexType), pVertices, 
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
				PrintError(ErrorOrigin::Renderer, "failed to create vertex buffer (in function StaticMesh::CreateBuffers)!");
				return false;
			}
			if (!m_IndexBuffer.CreateWithData(indexCount * sizeof(uint32_t), pIndices, 
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
				PrintError(ErrorOrigin::Renderer, "failed to create index buffer (in function StaticMesh::CreateBuffers)!");
				m_VertexBuffer.Terminate();
				return false;
			}
			m_IndexCount = indexCount;
			return true;
		}
	};

	class StaticTexture {

		friend class Engine;

	private:

		Renderer& m_Renderer;
		VkFormat m_Format;
		VkImage m_Image;
		VkDeviceMemory m_VulkanDeviceMemory;

	public:

		StaticTexture(Renderer& renderer) noexcept 
			: m_Renderer(renderer), m_Format(VK_FORMAT_UNDEFINED), m_Image(VK_NULL_HANDLE), m_VulkanDeviceMemory(VK_NULL_HANDLE) {}

		bool IsNull() const {
			return m_Image == VK_NULL_HANDLE;
		}

		bool Create(VkFormat format, Vec2_T<uint32_t> extent, const void* image) {
			uint32_t pixelSize;
			switch (format) {
				case (VK_FORMAT_R8G8B8A8_SRGB):
					pixelSize = 4;
					break;
				case (VK_FORMAT_R8_SRGB):
					pixelSize = 1;
					break;
				case (VK_FORMAT_R8_UINT):
					pixelSize = 1;
					break;
				default:
					PrintError(ErrorOrigin::Renderer, 
						"found unsupported format when creating texture (function StaticTexture in function StaticTexture::Create)!");
					return false;
			}
			VkDeviceSize deviceSize = (VkDeviceSize)extent.x * extent.y * pixelSize;
			Renderer::Buffer stagingBuffer(m_Renderer);
			if (!stagingBuffer.Create(deviceSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				PrintError(ErrorOrigin::Renderer, 
					"failed to create staging buffer for texture (function Renderer::Buffer::Create in function StaticTexture::Create!)");
				return false;
			}
			void* stagingMap;
			VkResult vkRes = vkMapMemory(m_Renderer.m_VulkanDevice, 
				stagingBuffer.m_VulkanDeviceMemory, 0, deviceSize, 0, &stagingMap);
			memcpy(stagingMap, image, deviceSize);
			vkUnmapMemory(m_Renderer.m_VulkanDevice, stagingBuffer.m_VulkanDeviceMemory);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to map staging buffer memory (function vkMapMemory in function StaticTexture::Create)!", vkRes);
				return false;
			}
			uint32_t queueFamilies[2] {
				m_Renderer.m_GraphicsQueueFamilyIndex,
				m_Renderer.m_TransferQueueFamilyIndex,
			};
			VkImageCreateInfo imageInfo {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent { extent.x, extent.y, 1 },
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_CONCURRENT,
				.queueFamilyIndexCount = 2,
				.pQueueFamilyIndices = queueFamilies,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			vkRes = vkCreateImage(m_Renderer.m_VulkanDevice, &imageInfo, m_Renderer.m_VulkanAllocationCallbacks, &m_Image);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to create image (function vkCreateImage in function StaticTexture::Create)!", vkRes);
				return false;
			}
			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(m_Renderer.m_VulkanDevice, m_Image, &memRequirements);
			VkMemoryAllocateInfo imageAllocInfo {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = memRequirements.size,
			};
			if (!m_Renderer.FindMemoryTypeIndex(memRequirements.memoryTypeBits, 
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageAllocInfo.memoryTypeIndex)) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to find memory type index (function Renderer::FindMemoryTypeIndex in function StaticTexture::Create)!");
				Terminate();
				return false;
			}
			vkRes = vkAllocateMemory(m_Renderer.m_VulkanDevice, &imageAllocInfo, 
				m_Renderer.m_VulkanAllocationCallbacks, &m_VulkanDeviceMemory);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to allocate image memory (function vkAllocateMemory in function StaticTexture::Create)!");
				Terminate();
				return false;
			}
			vkRes = vkBindImageMemory(m_Renderer.m_VulkanDevice, m_Image, m_VulkanDeviceMemory, 0);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to bind image memory (function vkBindImageMemory in function StaticTexture::Create)!");
				Terminate();
				return false;
			}
			LockGuard earlyGraphicsCommandBufferQueueGuard(m_Renderer.m_EarlyGraphicsCommandBufferQueueMutex);
			Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
				= m_Renderer.m_EarlyGraphicsCommandBufferQueue.New();
			if (!commandBuffer) {
				PrintError(ErrorOrigin::OutOfMemory, 
					"renderer graphics command buffer queue was out of memory (function in function StaticTexture::Create)!");
				Terminate();
				return false;
			}
			if (!m_Renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(m_Renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
				&commandBuffer->m_CommandBuffer)) {
				PrintError(ErrorOrigin::Renderer, 
					"failed to allocate graphics command buffer (function Renderer::AllocateCommandBuffers in function StaticTexture::Create)!");
				Terminate();
			}
			VkCommandBufferBeginInfo commandBufferBeginInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr,
			};
			vkRes = vkBeginCommandBuffer(commandBuffer->m_CommandBuffer, &commandBufferBeginInfo);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to begin graphics command buffer (function vkAllocateCommandBuffers in function StaticTexture::Create)", 
					vkRes);
				Terminate();
				return false;
			}

			VkImageMemoryBarrier memoryBarrier1 {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = 0,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = m_Image,
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};
			vkCmdPipelineBarrier(commandBuffer->m_CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier1);

			VkBufferImageCopy copyRegion {
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
				.imageOffset = { 0, 0 },
				.imageExtent = { extent.x, extent.y, 1 },
			};
			vkCmdCopyBufferToImage(commandBuffer->m_CommandBuffer, stagingBuffer.m_Buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
				1, &copyRegion);

			VkImageMemoryBarrier memoryBarrier2 {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = 0,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = m_Image,
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			vkCmdPipelineBarrier(commandBuffer->m_CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier2);

			vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to end graphics command buffer (function vkEndCommandBuffer in function StaticTexture::Create)!", 
					vkRes);
				Terminate();
				return false;
			}

			commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit | Renderer::CommandBufferFlag_SubmitCallback;
			commandBuffer->m_SubmitCallback = {
				.m_Callback = [](const Renderer& renderer, const Renderer::CommandBufferSubmitCallback& callback) {
					vkDestroyBuffer(renderer.m_VulkanDevice, callback.m_Data.u_BufferData.m_Buffer, renderer.m_VulkanAllocationCallbacks);
					vkFreeMemory(renderer.m_VulkanDevice, callback.m_Data.u_BufferData.m_VulkanDeviceMemory, 
						renderer.m_VulkanAllocationCallbacks);
				},
				.m_Data {
					.u_BufferData {
						.m_Buffer = stagingBuffer.m_Buffer,
						.m_VulkanDeviceMemory = stagingBuffer.m_VulkanDeviceMemory,
					},
				},
			};
			stagingBuffer.m_Buffer = VK_NULL_HANDLE;
			stagingBuffer.m_VulkanDeviceMemory = VK_NULL_HANDLE;
			m_Format = format;
			return true;
		}

		VkImageView CreateImageView() const {
			if (m_Image == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Vulkan, 
					"attempting to create image view for texture that's null (in function StaticTexture::CreateImageView)!");
				return VK_NULL_HANDLE;
			}
			VkImageViewCreateInfo imageViewInfo {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = m_Image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_Format,
				.components {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};
			VkImageView res;
			VkResult vkRes = vkCreateImageView(m_Renderer.m_VulkanDevice, &imageViewInfo, m_Renderer.m_VulkanAllocationCallbacks, &res);
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to create image view (function vkCreateImageView in function StaticTexture::CreateImageView)");
				return VK_NULL_HANDLE;
			}
			return res;
		}

		void Terminate() {
			vkDestroyImage(m_Renderer.m_VulkanDevice, m_Image, m_Renderer.m_VulkanAllocationCallbacks);
			m_Image = VK_NULL_HANDLE;
			vkFreeMemory(m_Renderer.m_VulkanDevice, m_VulkanDeviceMemory, m_Renderer.m_VulkanAllocationCallbacks);
			m_VulkanDeviceMemory = VK_NULL_HANDLE;
		}
	};

	class FontAtlas {

		friend class Engine;
		friend class UI;

	private:

		static inline VkFormat s_AtlasFormat = VK_FORMAT_UNDEFINED;
		static inline VkSampler s_Sampler = VK_NULL_HANDLE;

		Renderer& m_Renderer;
		TextRenderer& m_TextRenderer;
		GlyphAtlas* m_GlyphAtlas;
		StaticTexture m_AtlasTexture;
		VkImageView m_AtlasImageView = VK_NULL_HANDLE;

	public:

		FontAtlas(Renderer& renderer, TextRenderer& textRenderer) 
			: m_Renderer(renderer), m_TextRenderer(textRenderer), m_AtlasTexture(m_Renderer),
				m_GlyphAtlas((GlyphAtlas*)malloc(sizeof(GlyphAtlas))) {}

		~FontAtlas() {
			Terminate();
			free(m_GlyphAtlas);
			m_GlyphAtlas = nullptr;
		}

		void Terminate() {
			if (m_GlyphAtlas) {
				m_TextRenderer.DestroyGlyphAtlas(*m_GlyphAtlas);
			}
			m_AtlasTexture.Terminate();
			m_Renderer.DestroyImageView(m_AtlasImageView);
			m_AtlasImageView = VK_NULL_HANDLE;
		}

		bool LoadFont(const char* fileName, uint32_t pixelSize) {
			assert(m_GlyphAtlas);
			if (!m_AtlasTexture.IsNull()) {
				PrintError(ErrorOrigin::Engine,
					"attempting to load font atlas that's already loaded (in function FontAtlas::LoadFont)!");
				return false;
			}
			if (!m_TextRenderer.CreateGlyphAtlas(fileName, pixelSize, *m_GlyphAtlas)) {
				PrintError(ErrorOrigin::TextRenderer, 
					"failed to create glyph atlas (function TextRenderer::CreateGlyphAtlas in function FontAtlas::LoadFont)!");
				return false;
			}
			if (!m_AtlasTexture.Create(s_AtlasFormat, m_GlyphAtlas->m_Extent, m_GlyphAtlas->m_Atlas)) {
				PrintError(ErrorOrigin::Engine, 
					"failed to create font atlas texture (function TextRenderer::CreateGlyphAtlas in function FontAtlas::LoadFont)!");
				Terminate();
				return false;
			}
			m_AtlasImageView = m_AtlasTexture.CreateImageView();
			if (m_AtlasImageView == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Engine, 
					"failed to create font atlas image view (function StaticTexture::CreateImageView in function FontAtlas::LoadFont)!");
				Terminate();
				return false;
			}
			return true;
		}

		const GlyphAtlas* GetGlyphAtlas() const {
			return m_GlyphAtlas;
		}

		VkImageView GetImageView() const {
			return m_AtlasImageView;
		}

		static VkSampler GetSampler() {
			return s_Sampler;
		}
	};

	class Input {
	public:

		friend class Engine;

		enum class Key {
			Space = 32,
			Apostrophe = 39,
			Comma = 44,
			Minus = 45,
			Period = 46,
			Slash = 47,
			Zero = 48,
			One = 49,
			Two = 50,
			Three = 51,
			Four = 52,
			Five = 53,
			Six = 54,
			Seven = 55,
			Eight = 56,
			Nine = 57,
			Semicolon = 59,
			Equal = 61,
			A = 65,
			B = 66,
			C = 67,
			D = 68,
			E = 69,
			F = 70,
			G = 71,
			H = 72,
			I = 73,
			J = 74,
			K = 75,
			L = 76,
			M = 77,
			N = 78,
			O = 79,
			P = 80,
			Q = 81,
			R = 82,
			S = 83,
			T = 64,
			U = 85,
			V = 86,
			W = 87,
			X = 88,
			Y = 89,
			Z = 90,
			LeftBracket = 91,
			Backslash = 92,
			RightBracket = 93,
			GraveAccent = 96,
			World1 = 161,
			World2 = 162,
			Escape  = 256,
			Enter = 257,
			Tab = 258,
			Backspace = 259,
			Insert = 260,
			Delete = 261,
			Right = 262,
			Left = 263,
			Down = 264,
			Up = 265,
			PageUp = 266,
			PageDown = 267,
			Home = 268,
			End = 269,
			CapsLock = 280,
			ScrollLock = 281,
			NumLock = 282,
			PrintScreen = 283,
			Pause = 284,
			F1 = 290,
			F2 = 291,
			F3 = 292,
			F4 = 293,
			F5 = 294,
			F6 = 295,
			F7 = 296,
			F8 = 297,
			F9 = 298,
			F10 = 299,
			F11 = 300,
			F12 = 301,
			F13 = 302,
			F14 = 303,
			F15 = 304,
			F16 = 305,
			F17 = 306,
			F18 = 307,
			F19 = 308,
			F20 = 309,
			F21 = 310,
			F22 = 311,
			F23 = 312,
			F24 = 313,
			F25 = 314,
			KP0 = 320,
			KP1 = 321,
			KP2 = 322,
			KP3 = 323,
			KP4 = 324,
			KP5 = 325,
			KP6 = 326,
			KP7 = 327,
			KP8 = 328,
			KP9 = 329,
			KPDecimal = 330,
			KPDivide = 331,
			KPMultiply = 332,
			KPSubtract = 333,
			KPAdd = 334,
			KPEnter = 335,
			KPEqual = 336,
			LeftShift = 340,
			LeftControl = 341,
			LeftAlt = 342,
			LeftSuper = 343,
			RightShift = 344,
			RightControl = 345,
			RightAlt = 346,
			RightSuper = 347,
			Menu = 348,
			MaxEnum = Menu + 1,
		};

		enum class MouseButton {
			One = 0,
			Two = 1,
			Three = 2,
			Four = 3,
			Five = 4,
			Six = 6,
			Eight = 7,
			Left = One,
			Right = Two,
			Middle = Three,
			MaxEnum = Eight + 1,
		};

	private:
		
		static constexpr size_t key_count = static_cast<size_t>(Key::MaxEnum) + 1;
		static constexpr size_t mouse_button_count = static_cast<size_t>(MouseButton::MaxEnum) + 1;

		static inline bool s_PressedKeys[key_count]{};
		static inline bool s_ReleasedKeys[key_count]{};
		static inline bool s_HeldKeys[key_count]{};
		static inline float s_KeyValues[key_count]{};

		static inline DynamicArray<Key> s_ActiveKeys{};

		static inline bool s_PressedMouseButtons[mouse_button_count]{};
		static inline bool s_ReleasedMouseButtons[mouse_button_count]{};
		static inline bool s_HeldMouseButtons[mouse_button_count]{};
		static inline float s_MouseButtonValues[mouse_button_count]{};

		static inline DynamicArray<MouseButton> s_ActiveMouseButtons{};

		static inline DynamicArray<unsigned int> s_TextInput{};

		static inline Vec2_T<double> s_CursorPosition{};
		static inline Vec2_T<double> s_DeltaCursorPosition{};

		static void KeyCallback(GLFWwindow*, int key, int scancode, int action, int mods) {
			if (key < 0) {
				return;
			}
			size_t index = key;
			assert(index < key_count);
			s_PressedKeys[index] = action == GLFW_PRESS;
			s_ReleasedKeys[index] = action == GLFW_RELEASE;
			s_HeldKeys[index] = action != GLFW_RELEASE;
			if (action != GLFW_RELEASE) {
				s_KeyValues[index] = 1.0f;
				s_ActiveKeys.PushBack((Key)key);
			}
		}

		static void MouseButtonCallback(GLFWwindow*, int button, int action, int mods) {
			if (button < 0) {
				return;
			}
			size_t index = button;
			assert(index < mouse_button_count);
			s_PressedMouseButtons[index] = action == GLFW_PRESS;
			s_ReleasedMouseButtons[index] = action == GLFW_RELEASE;
			s_HeldMouseButtons[index] = action != GLFW_RELEASE;
			if (action != GLFW_RELEASE) {
				s_MouseButtonValues[index] = 1.0f;
				s_ActiveMouseButtons.PushBack((MouseButton)button);
			}
		}

		static void CharacterCallback(GLFWwindow*, unsigned int c) {
			s_TextInput.PushBack(c);
		}

		static void ResetInput() {
			for (Key* iter = s_ActiveKeys.begin(); iter != s_ActiveKeys.end();) {
				size_t index = (size_t)*iter;
				if (s_HeldKeys[index]) {
					++iter;
				}
				else {
					s_KeyValues[index] = 0.0f;
					iter = s_ActiveKeys.Erase(iter);
				}
				s_PressedKeys[index] = s_ReleasedKeys[index] = false;
			}

			for (MouseButton* iter = s_ActiveMouseButtons.begin(); iter != s_ActiveMouseButtons.end();) {
				size_t index = (size_t)*iter;
				if (s_HeldMouseButtons[index]) {
					++iter;
				}
				else {
					s_MouseButtonValues[index] = 0.0f;
					iter = s_ActiveMouseButtons.Erase(iter);
				}
				s_PressedMouseButtons[index] = s_ReleasedMouseButtons[index] = false;
			}
			s_TextInput.Resize(0);
		};

		Input(GLFWwindow* pGLFWwindow) {
			s_ActiveKeys.Reserve(key_count);
			s_ActiveMouseButtons.Reserve(mouse_button_count);
			s_TextInput.Reserve(256);
			glfwSetKeyCallback(pGLFWwindow, KeyCallback);
			glfwSetMouseButtonCallback(pGLFWwindow, MouseButtonCallback);
			glfwSetCharCallback(pGLFWwindow, CharacterCallback);
		};

	public:

		static bool WasKeyPressed(Key key) {
			return s_PressedKeys[static_cast<size_t>(key)];
		}

		static bool WasKeyReleased(Key key) {
			return s_ReleasedKeys[static_cast<size_t>(key)];
		}

		static bool WasKeyHeld(Key key) {
			return s_HeldKeys[static_cast<size_t>(key)];
		}

		static float ReadKeyValue(Key key) {
			return s_KeyValues[static_cast<size_t>(key)];
		}

		static bool WasMouseButtonPressed(MouseButton button) {
			return s_PressedMouseButtons[static_cast<size_t>(button)];
		}

		static bool WasMouseButtonReleased(MouseButton button) {
			return s_ReleasedMouseButtons[static_cast<size_t>(button)];
		}

		static bool WasMouseButtonHeld(MouseButton button) {
			return s_HeldMouseButtons[static_cast<size_t>(button)];
		}

		static const DynamicArray<unsigned int>& GetTextInput() {
			return s_TextInput;
		}
	};

	class Time {

			friend class Engine;

		private:
			
			using steady_clock = std::chrono::steady_clock;
			using microseconds = std::chrono::microseconds;

			static inline steady_clock::time_point s_FrameStartTime{};
			static inline float s_DeltaTime = 0.0f;

			static void BeginFrame() {
				s_FrameStartTime = steady_clock::now();
			}

			static void EndFrame() {
				s_DeltaTime = std::chrono::duration_cast<microseconds>((steady_clock::now() - s_FrameStartTime)).count() / 1000000.0f;
			}

		public:

			static float DeltaTime() {
				return s_DeltaTime;
			}
	};

	template<typename T>
	struct PersistentReferenceHolder;

	template<typename T>
	class PersistentReference {
	public:

		static_assert(std::is_base_of<PersistentReferenceHolder<T>, T>());

		T* m_Val;

		PersistentReference() noexcept : m_Val(nullptr) {}

		PersistentReference(T& val) noexcept : m_Val(&val) {
			m_Val->AddPersistentReference(this);
		}

		PersistentReference(const PersistentReference& other) noexcept : m_Val(other.m_Val) {
			if (m_Val) {
				m_Val->AddPersistentReference(this);
			}
		}

		~PersistentReference() {
			RemoveReference();
		}

		bool IsNull() const {
			return !m_Val;
		}

		void SetReference(T& val) {
			RemoveReference();
			m_Val = &val;
			m_Val->AddPersistentReference(this);
		}

		void RemoveReference() {
			if (m_Val) {
				m_Val->RemovePersistentReference(this);
				m_Val = nullptr;
			}
		}
		
		T& operator*() {
			if (!m_Val) {
				CriticalError(ErrorOrigin::NullDereference,
					"attempting to deference null reference (in PersistentReference::operator*)");
			}
			return *m_Val;
		}

		const T& operator*() const {
			if (!m_Val) {
				CriticalError(ErrorOrigin::NullDereference,
					"attempting to deference null reference (in PersistentReference::operator*)");
			}
			return *m_Val;
		}
	};

	template<typename T>
	class PersistentReferenceHolder {

		typedef T Derived;
	
		friend class PersistentReference<T>;
			
	protected:

		DynamicArray<PersistentReference<T>*> m_PersistentReferences;

		PersistentReferenceHolder() : m_PersistentReferences() {
			m_PersistentReferences.Reserve(4);
		}

		PersistentReferenceHolder(PersistentReferenceHolder&& other) 
			: m_PersistentReferences(std::move(other.m_PersistentReferences)) {
			for (PersistentReference<T>* reference : m_PersistentReferences) {
				assert(reference);
				reference->m_Val = (T*)this;
			}
		}

		virtual ~PersistentReferenceHolder() {
			for (PersistentReference<T>* reference : m_PersistentReferences) {
				assert(reference);
				reference->m_Val = nullptr;
			}
		}

	private:

		bool AddPersistentReference(PersistentReference<T>* reference) {
			if (reference) {
				m_PersistentReferences.PushBack(reference);
				return true;
			}
			return false;
		}

		bool RemovePersistentReference(PersistentReference<T>* reference) {
			if (reference) {
				auto end = m_PersistentReferences.end();
				for (auto iter = m_PersistentReferences.begin(); iter != end; iter++) {
					if (*iter == reference) {
						m_PersistentReferences.Erase(iter);
						return true;
					}
				}
			}
			return false;
		}
	};

	class UI {

		friend class Engine;

	public:	

		class Shaders {
		public:

			static constexpr const char* draw_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstant {
layout(offset = 0) 
mat4 c_Transform;
} pc;

void main() {
outUV = inUV;
gl_Position = pc.c_Transform * vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
			)";

			static constexpr const char* draw_fragment_shader = R"(
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstant {
layout(offset = 64) 
uint c_TextureIndex;
} pc;

void main() {
outColor = texture(textures[nonuniformEXT(pc.c_TextureIndex)], inUV);
}
			)";

			static constexpr const char* text_draw_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstant {
layout(offset = 0)
mat4 c_Transform;
} pc;

void main() {
outUV = inUV;
gl_Position = pc.c_Transform * vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
			)";

			static constexpr const char* text_draw_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D glyph_atlas; // unnormalized coordinates
layout(set = 0, binding = 1) uniform sampler2D color_mask; // unnormalized coordinates

layout(push_constant) uniform PushConstant1 {
layout(offset = 64)
uvec2 c_FrameExtent;
uvec2 c_Bearing;
uint c_AtlasOffsetX;
vec4 c_Color;
ivec2 c_ColorMaskOffset;
} pc;

void main() {
vec2 localUV = uvec2(inUV.x * pc.c_FrameExtent.x, inUV.y * pc.c_FrameExtent.y);
float val = textureLod(glyph_atlas, localUV + vec2(pc.c_AtlasOffsetX, 0.0f), 0).r;
outColor = pc.c_ColorMaskOffset.x != -1 ? textureLod(color_mask, localUV + pc.c_ColorMaskOffset, 0) : pc.c_Color;
outColor *= val;
}
			)";

			static constexpr const char* render_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main() {
outUV = inUV;
gl_Position = vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
			)";

			static constexpr const char* render_fragment_shader = R"(
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D image;

void main() {
outColor = texture(image, inUV);
}
			)";
		};

		class Pipelines {

			friend class UI;

			UI& m_UI;

			static constexpr uint32_t max_texture_sets = 500;

			VkPipeline m_DrawPipeline = VK_NULL_HANDLE;
			VkPipelineLayout m_DrawPipelineLayout = VK_NULL_HANDLE;

			VkPipeline m_TextDrawPipeline = VK_NULL_HANDLE;
			VkPipelineLayout m_TextDrawPipelineLayout = VK_NULL_HANDLE;

			VkPipeline m_RenderPipeline = VK_NULL_HANDLE;
			VkPipelineLayout m_RenderPipelineLayout = VK_NULL_HANDLE;

			VkDescriptorSetLayout m_DrawDescriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_TextDrawDescriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_RenderDescriptorSetLayout = VK_NULL_HANDLE;

			Pipelines(UI& UI) : m_UI(UI) {}

			void Initialize() {

				Renderer& renderer = m_UI.m_Renderer;

				VkDescriptorBindingFlags textureArrayDescriptorBindingFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

				const VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
					.pNext = nullptr,
					.bindingCount = 1,
					.pBindingFlags = &textureArrayDescriptorBindingFlags,
				};

				VkDescriptorSetLayoutBinding drawPipelineDescriptorSetLayoutBinding 
					= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				drawPipelineDescriptorSetLayoutBinding.descriptorCount = 64;

				m_DrawDescriptorSetLayout 
					= renderer.CreateDescriptorSetLayout(&descriptorSetLayoutBindingFlagsInfo, 1, &drawPipelineDescriptorSetLayoutBinding);

				if (m_DrawDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create draw descriptor set layout for UI (function Renderer::CreateDescriptorSetLayout in function UI::Pipelines::Initialize)!");
				}

				const VkPushConstantRange drawPipelinePushConstantRanges[2] {
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64),
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16),
				};

				m_DrawPipelineLayout = renderer.CreatePipelineLayout(1, &m_DrawDescriptorSetLayout, 2, drawPipelinePushConstantRanges);

				if (m_DrawPipelineLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create draw pipeline layout for UI (function Renderer::CreatePipelineLayout in function UI::Pipelines::Initialize)!");
				}

				const VkDescriptorSetLayoutBinding textDrawDescriptorSetLayoutBindings[2] {
					Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
					Renderer::GetDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
				};

				m_TextDrawDescriptorSetLayout = renderer.CreateDescriptorSetLayout(nullptr, 2, textDrawDescriptorSetLayoutBindings);

				if (m_TextDrawDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create text draw descriptor set layout for UI (function Renderer::CreateDescriptorSetLayout in function UI::Initialize)!");
				}

				static constexpr uint32_t text_draw_push_constant_count = 2;

				const VkPushConstantRange textDrawPipelinePushConstantRanges[text_draw_push_constant_count] {
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64),
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof(TextFragmentPushConstant)),
				};

				m_TextDrawPipelineLayout = renderer.CreatePipelineLayout(1, &m_TextDrawDescriptorSetLayout, text_draw_push_constant_count, textDrawPipelinePushConstantRanges);

				m_RenderPipelineLayout = renderer.CreatePipelineLayout(1, &m_RenderDescriptorSetLayout, 0, nullptr);

				if (m_RenderPipelineLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create render pipeline layout for UI (function Renderer::CreatePipelineLayout in function UI::Pipelines::Initialize)");
				}

				Renderer::Shader drawShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!drawShaders[0].Compile(Shaders::draw_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile draw vertex shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}
				if (!drawShaders[1].Compile(Shaders::draw_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile draw vertex shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}

				const VkPipelineShaderStageCreateInfo drawPipelineShaderStages[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(drawShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(drawShaders[1]),
				};

				Renderer::Shader textDrawShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!textDrawShaders[0].Compile(Shaders::text_draw_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to compile text draw vertex shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}

				if (!textDrawShaders[1].Compile(Shaders::text_draw_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to compile text draw fragment shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}

				const VkPipelineShaderStageCreateInfo textDrawPipelineShaderStages[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(textDrawShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(textDrawShaders[1]),
				};

				Renderer::Shader renderShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!renderShaders[0].Compile(Shaders::render_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile render vertex shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}
				if (!renderShaders[1].Compile(Shaders::render_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile render vertex shader for UI (function Renderer::Shader::Compile in function UI::Pipelines::Initialize)!");
				}	

				const VkPipelineShaderStageCreateInfo renderPipelineShaderStages[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(renderShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(renderShaders[1]),
				};	

				VkPipelineColorBlendStateCreateInfo colorBlendStateInfo 
					= Renderer::GraphicsPipelineDefaults::color_blend_state;
				colorBlendStateInfo.attachmentCount = 1;
				colorBlendStateInfo.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state;

				VkPipelineRenderingCreateInfo drawPipelineRenderingInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &m_UI.m_ColorFormat, VK_FORMAT_UNDEFINED);

				VkPipelineRenderingCreateInfo renderPipelineRenderingInfo
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, VK_FORMAT_UNDEFINED);

				static constexpr uint32_t pipeline_count = 3;

				VkGraphicsPipelineCreateInfo pipelineInfos[pipeline_count] {
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &drawPipelineRenderingInfo,
						.flags = 0,
						.stageCount = 2,
						.pStages = drawPipelineShaderStages,
						.pVertexInputState = &Vertex2D::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
						.pColorBlendState = &colorBlendStateInfo,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_DrawPipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &drawPipelineRenderingInfo,
						.flags = 0,
						.stageCount = 2,
						.pStages = textDrawPipelineShaderStages,
						.pVertexInputState = &Vertex2D::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
						.pColorBlendState = &colorBlendStateInfo,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_TextDrawPipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &renderPipelineRenderingInfo,
						.flags = 0,
						.stageCount = 2,
						.pStages = renderPipelineShaderStages,
						.pVertexInputState = &Vertex2D::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
						.pColorBlendState = &colorBlendStateInfo,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_RenderPipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
				};
				VkPipeline pipelines[pipeline_count];
				if (!renderer.CreateGraphicsPipelines(pipeline_count, pipelineInfos, pipelines)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create UI pipelines (function Renderer::CreateGraphicsPipelines in function UI::Pipelines::Initialize)!");
				}
				m_DrawPipeline = pipelines[0];
				m_TextDrawPipeline = pipelines[1];
				m_RenderPipeline = pipelines[2];
			}

			void Terminate() {
				Renderer& renderer = m_UI.m_Renderer;
				renderer.DestroyPipeline(m_DrawPipeline);
				renderer.DestroyPipelineLayout(m_DrawPipelineLayout);
				renderer.DestroyDescriptorSetLayout(m_DrawDescriptorSetLayout);
				renderer.DestroyPipeline(m_TextDrawPipeline);
				renderer.DestroyPipelineLayout(m_TextDrawPipelineLayout);
				renderer.DestroyDescriptorSetLayout(m_TextDrawDescriptorSetLayout);
				renderer.DestroyPipeline(m_RenderPipeline);
				renderer.DestroyPipelineLayout(m_RenderPipelineLayout);
				renderer.DestroyDescriptorSetLayout(m_RenderDescriptorSetLayout);
			}
		};

		struct Rect {

			IntVec2 m_Min{};
			IntVec2 m_Max{};

			bool IsPointInside(IntVec2 point) const {
				return point.x >= m_Min.x && point.y >= m_Min.y 
					&& point.x <= m_Max.x && point.y <= m_Max.y;
			}

			IntVec2 Dimensions() const {
				return m_Max - m_Min;
			}

			Vec2 Middle() const {
				return m_Min + Dimensions() / 2;
			}

			void CalcTransform(Vec2_T<uint32_t> framebufferSize, Mat4& out) const {
				out.columns[0] = Vec4((float)(m_Max.x - m_Min.x) / framebufferSize.x, 0.0f, 0.0f, 0.0f);
				out.columns[1] = Vec4(0.0f, (float)(m_Max.y - m_Min.y) / framebufferSize.y, 0.0f, 0.0f);
				out.columns[2] = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
				Vec2 pos = Middle();
				out.columns[3] = Vec4(pos.x / framebufferSize.x * 2.0f - 1.0f, pos.y / framebufferSize.y * 2.0f - 1.0f, 0.0f, 1.0f);
			}
		};

		class DynamicText {

			friend class UI;

			using TRCharacter = TextRenderer::Character;

		public:

			struct FragmentPushConstant {

				Vec2_T<uint32_t> c_FrameExtent;
				Vec2_T<uint32_t> c_Bearing;
				uint32_t c_AtlasOffsetX;
				uint8_t pad0[12];
				Vec4 c_Color;
				IntVec2 c_ColorMaskOffset;
			};

			class Character {

				friend class UI;

			public:

				IntVec2 m_Offset{};
				Mat4 m_AdditionalTransform { 1 };

			private:

				uint32_t m_LocalPositionX;

				FragmentPushConstant m_FragmentPushConstant;

			public:

				Character(const TRCharacter& trCharacter, uint32_t localPositionX, Vec4 color) 
					: m_LocalPositionX(localPositionX) {
					m_FragmentPushConstant = {
						.c_FrameExtent = trCharacter.m_Size,
						.c_Bearing = trCharacter.m_Bearing,
						.c_AtlasOffsetX = trCharacter.m_Offset,
						.c_Color = color,
						.c_ColorMaskOffset = -1,
					};
				}

				uint32_t GetLocalPositionX() {
					return m_LocalPositionX;
				}
			};

			typedef Character* Iterator;
			typedef const Character* ConstIterator;

			UI& m_UI;
			FontAtlas& m_FontAtlas;
			IntVec2 m_Position{};

		private:

			DynamicArray<const TRCharacter*> m_TextRendererCharacters{};
			DynamicArray<Character> m_RenderedCharacters{};

			VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

			VkImageView m_ColorMask = VK_NULL_HANDLE;

			uint32_t m_TextLength = 0;

			uint32_t m_StringLength = 0;
			char m_SmallStringBuffer[16]{};
			uint32_t m_HeapBufferCapacity = 0;
			char* m_HeapStringBuffer = nullptr;

			VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

		public:

			DynamicText(UI& UI, FontAtlas& fontAtlas) noexcept : m_UI(UI), m_FontAtlas(fontAtlas) {}

			~DynamicText() {
				Terminate();
			}

			bool Initialize(VkImageView colorMask) {

				m_RenderedCharacters.Reserve(16);
				m_TextRendererCharacters.Reserve(16);

				Renderer& renderer = m_UI.m_Renderer;

				VkDescriptorPoolSize poolSizes[2] {
					{
						.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = 1,
					},
					{
						.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = 1,
					},
				};
				
				m_DescriptorPool = renderer.CreateDescriptorPool(0, 1, 2, poolSizes);

				if (m_DescriptorPool == VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::Renderer, 
						"failed to create descriptor pool for dynamic text (function Renderer::CreateDescriptorPool in function UI::DynamicText::Initialize)!");
					Terminate();
					return false;
				}

				if (!renderer.AllocateDescriptorSets(nullptr, m_DescriptorPool, 1, 
						&m_UI.m_Pipelines.m_TextDrawDescriptorSetLayout, &m_DescriptorSet)) {
					PrintError(ErrorOrigin::Renderer, 
						"failed to allocate descriptor set for dynamic text (function Renderer::AllocateDescriptorSets in function UI::DynamicText::Initialize)!");
					Terminate();
					return false;
				}

				VkDescriptorImageInfo imageInfos[2] {
					{
						.sampler = FontAtlas::GetSampler(),
						.imageView = m_FontAtlas.GetImageView(),
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					},
					{
						.sampler = FontAtlas::GetSampler(),
						.imageView = colorMask == VK_NULL_HANDLE ? m_FontAtlas.GetImageView() : colorMask,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					},
				};

				VkWriteDescriptorSet writes[2] {
					Renderer::GetDescriptorWrite(nullptr, 0, m_DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[0], nullptr),
					Renderer::GetDescriptorWrite(nullptr, 1, m_DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[1], nullptr),
				};
				renderer.UpdateDescriptorSets(2, writes);
				return true;
			}

			void Terminate() {
				Renderer& renderer = m_UI.m_Renderer;
				renderer.DestroyDescriptorPool(m_DescriptorPool);
				m_DescriptorPool = VK_NULL_HANDLE;
				m_DescriptorSet = VK_NULL_HANDLE;
				free(m_HeapStringBuffer);
				m_HeapStringBuffer = nullptr;
				m_TextRendererCharacters.Clear();
				m_RenderedCharacters.Clear();
				m_Position = 0;
				m_StringLength = 0;
				m_HeapBufferCapacity = 0;
			}

			DynamicText& PutChar(unsigned char c, Vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }) {
				if (m_DescriptorPool == VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::UI, 
						"attempting to push character to dynamic text that hasn't been initialized (in function UI::DynamicText::PushChar)!");
				}
				if (c >= 128) {
					PrintError(ErrorOrigin::UI,
						"attempting to push an invalid character to dynamic text object (in function UI::DynamicText::PushChar)!");
					return *this;
				}
				const TRCharacter& trCharacter = m_FontAtlas.GetGlyphAtlas()->m_Characters[c];
				if (trCharacter.m_Size != 0) {
					Character& character 
						= m_RenderedCharacters.EmplaceBack(m_FontAtlas.GetGlyphAtlas()->m_Characters[c], m_TextLength, color);
				}
				if (m_StringLength < 16) {
					m_SmallStringBuffer[m_StringLength++] = c;
				}
				else if (m_HeapStringBuffer) {
					if (m_StringLength >= m_HeapBufferCapacity) {
						char* temp = m_HeapStringBuffer;
						m_HeapStringBuffer = (char*)malloc(m_HeapBufferCapacity *= 2 * sizeof(char));
						assert(m_HeapStringBuffer);
						for (uint32_t i = 0; i < m_StringLength; i++) {
							m_HeapStringBuffer[i] = temp[i];
						}
						free(temp);
					}
					m_HeapStringBuffer[m_StringLength++] = c;
				}
				else {
					m_HeapBufferCapacity = 32;
					m_HeapStringBuffer = (char*)malloc(m_HeapBufferCapacity);
					assert(m_HeapStringBuffer);
					for (uint32_t i = 0; i < m_StringLength; i++) {
						m_HeapStringBuffer[i] = m_SmallStringBuffer[i];
					}
					m_HeapStringBuffer[m_StringLength++] = c;
				}
				m_TextRendererCharacters.PushBack(&trCharacter);
				m_TextLength += trCharacter.m_Escapement.x;
				return *this;
			}

			Iterator begin() {
				return m_RenderedCharacters.begin();
			}

			ConstIterator end() {
				return m_RenderedCharacters.end();
			}
		};

		typedef DynamicText::FragmentPushConstant TextFragmentPushConstant;

		class StaticText {

			friend class UI;

		public:

			UI& m_UI;
			IntVec2 m_Position;

		private:

			VkImageView m_ImageView = VK_NULL_HANDLE;
			Vec2_T<uint32_t> m_FrameExtent{};
			VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		
			TextImage m_TextImage{};
			StaticTexture m_Texture;
			VkDescriptorPool m_DescriptorPool{};


		public:

			StaticText(UI& UI) : m_UI(UI), m_Texture(m_UI.m_Renderer) {}

			StaticText(const StaticText&) = delete;
			StaticText(StaticText&&) = delete;

			~StaticText() { Terminate(); }

			bool Initialize(const char* text, const GlyphAtlas& atlas, const Vec4& color, Vec2_T<uint32_t> frameExtent, TextAlignment alignment) {
				Terminate();
				TextRenderer::RenderTextInfo renderInfo {
					.m_GlyphAtlas = atlas,
					.m_Spacing = { 0, 0 },
					.m_TextColor = PackColorRBGA(color),
					.m_BackGroundColor = 0,
				};
				switch (alignment) {
					case TextAlignment::Left:
						m_TextImage = m_UI.m_TextRenderer.RenderText<TextAlignment::Left>(text, renderInfo, frameExtent);
						break;
					case TextAlignment::Middle:
						m_TextImage = m_UI.m_TextRenderer.RenderText<TextAlignment::Middle>(text, renderInfo, frameExtent);
						break;
				}
				if (m_TextImage.IsNull()) {
					PrintError(ErrorOrigin::TextRenderer, 
						"failed to render text (function TextRenderer::RenderText in function Text::Initialize)!");
					fmt::print(fmt::emphasis::bold, "text that failed to render: {}\n", text);
					return false;
				}
				m_FrameExtent = m_TextImage.m_Extent;
				if (!m_Texture.Create(m_UI.m_ColorFormat, m_FrameExtent, m_TextImage.m_Image)) {
					PrintError(ErrorOrigin::UI, 
						"failed to create texture for UI text (function StaticTexture::Create in function UI::Text::Initialize)");
					Terminate();
					return false;
				}
				m_ImageView = m_Texture.CreateImageView();
				if (m_ImageView == VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::UI,
						"failed to create image view for UI text (function StaticTexture::CreateImageView in function UI::Text::Initialize)!");
					Terminate();
					return false;
				}
				if (!m_UI.CreateTexture2DArray<1>(&m_ImageView, m_DescriptorSet, m_DescriptorPool)) {
					PrintError(ErrorOrigin::UI, 
						"failed to create descriptor set for UI text (function UI::CreateTexture2DArray in function UI:.Text::Initialize)!");
					Terminate();
					return false;
				}
				return true;
			}

			bool Initialize(char c, const GlyphAtlas& atlas, const Vec4& color) {
				Terminate();
				m_TextImage = m_UI.m_TextRenderer.RenderCharacter(c, atlas, PackColorRBGA(color));
				if (m_TextImage.IsNull()) {
					return false;
				}
				m_FrameExtent = m_TextImage.m_Extent;
				if (!m_Texture.Create(m_UI.m_ColorFormat, m_FrameExtent, m_TextImage.m_Image)) {
					PrintError(ErrorOrigin::UI, 
						"failed to create texture for UI text (function StaticTexture::Create in function UI::Text::Initialize)");
					Terminate();
					return false;
				}
				m_ImageView = m_Texture.CreateImageView();
				if (m_ImageView == VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::UI,
						"failed to create image view for UI text (function StaticTexture::CreateImageView in function UI::Text::Initialize)!");
					Terminate();
					return false;
				}
				if (!m_UI.CreateTexture2DArray<1>(&m_ImageView, m_DescriptorSet, m_DescriptorPool)) {
					PrintError(ErrorOrigin::UI, 
						"failed to create descriptor set for UI text (function UI::CreateTexture2DArray in function UI:.Text::Initialize)!");
					Terminate();
					return false;
				}
				return true;
			}

			void Terminate() {
				TextRenderer::DestroyTextImage(m_TextImage);
				m_Texture.Terminate();
				m_UI.m_Renderer.DestroyImageView(m_ImageView);
				m_ImageView = VK_NULL_HANDLE;
				m_UI.m_Renderer.DestroyDescriptorPool(m_DescriptorPool);
				m_DescriptorPool = VK_NULL_HANDLE;
			}

			bool IsNull() const {
				return m_DescriptorSet == VK_NULL_HANDLE;
			}

			Vec2_T<uint32_t> GetFrameExtent() const {
				return m_FrameExtent;
			}
		};

		enum class WindowState {
			Closed = 0,
			Focused = 1,
			Unfocused = 2,
		};

		/*! @brief UI window class
		*/
		class Window {
		public:

			typedef bool (*Pipeline2DRenderCallback)(const Window& window, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);

			class Button {
			public:

				typedef bool (*Pipeline2DRenderCallback)(const Button& button, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);
		
				const char* const m_StringID;
				IntVec2 m_LocalPosition;
				Rect m_Rect{};
				void (*m_HoverCallback)(Button& button){};
				Pipeline2DRenderCallback m_Pipeline2DRenderCallback{};
				Mat4 m_Transform{};

				Button(const char* stringID, const Rect& rect) : m_StringID(stringID), m_Rect(rect) {}

				IntVec2 GetLocalPosition() const {
					return m_LocalPosition;
				}

				void SetLocalPosition(const Window& window, IntVec2 position, Vec2_T<uint32_t> renderResolution) {
					IntVec2 oldPos = GetLocalPosition();
					IntVec2 deltaPos = position - oldPos;
					IntVec2 size = m_Rect.m_Max - m_Rect.m_Min;
					m_Rect.m_Min = window.m_Rect.m_Min + position;
					m_Rect.m_Max = m_Rect.m_Min + size;
					m_Rect.CalcTransform(renderResolution, m_Transform);
				}

				bool RenderCallback2D(VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex) const {
					return m_Pipeline2DRenderCallback && m_Pipeline2DRenderCallback(*this, outTextureDescriptor, outTextureIndex);
				}

				uint64_t operator()() {
					return String::Hash()(m_StringID);
				}
			};

			static constexpr size_t max_buttons = 250;
			
			UI& m_UI;
			const char* const m_StringID;
			WindowState m_State;
			Rect m_Rect;
			Pipeline2DRenderCallback m_Pipeline2DRenderCallback{};
			Mat4 m_Transform{};
			Dictionary<Button> m_ButtonLookUp;
			DynamicArray<Button*> m_Buttons;

			Window(UI& UI, const char* stringID, WindowState state, const Rect& rect) 
				: m_UI(UI), m_StringID(stringID), m_State(state), m_Rect(rect), m_ButtonLookUp(max_buttons * 2), m_Buttons() {
				m_Buttons.Reserve(max_buttons);
				m_Rect.CalcTransform(m_UI.GetSwapchainResolution(), m_Transform);
			}

			void Render(VkCommandBuffer commandBuffer) const {
				VkDescriptorSet texDescriptor = VK_NULL_HANDLE;
				uint32_t texIndex = 0;
				VkPipelineLayout pipelineLayout = m_UI.m_Pipelines.m_DrawPipelineLayout;
				if (m_Pipeline2DRenderCallback && m_Pipeline2DRenderCallback(*this, texDescriptor, texIndex)) {
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
						0, 1, &texDescriptor, 0, nullptr);
					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &m_Transform);
					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16, &texIndex);
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, m_UI.m_StaticQuadMesh2DData.m_VertexBuffers, 
						m_UI.m_StaticQuadMesh2DData.m_VertexBufferOffsets);
					vkCmdBindIndexBuffer(commandBuffer, m_UI.m_StaticQuadMesh2DData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(commandBuffer, m_UI.m_StaticQuadMesh2DData.m_IndexCount, 1, 0, 0, 0);
				}
				for (Button* button : m_Buttons) {
					if (button->RenderCallback2D(texDescriptor, texIndex)) {
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 
							0, 1, &texDescriptor, 0, nullptr);
						vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &button->m_Transform);
						vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16, &texIndex);
					}
				}
			};

			Button* AddButton(const char* stringID, Vec2_T<uint32_t> size, IntVec2 position) {
				if (m_ButtonLookUp.Contains(stringID)) {
					PrintError(ErrorOrigin::UI, 
						"attempting to add button with an already existing string ID (in function UI::Window::AddButton)!");
					return nullptr;
				}
				IntVec2 truePos = m_Rect.m_Min + size;
				Button* res = m_ButtonLookUp.Emplace(stringID, stringID, 
					Rect { .m_Min { truePos }, .m_Max { truePos + size } } );
				if (!res) {
					PrintError(ErrorOrigin::UI, "failed to add button (function UI::Dictionary::Emplace in function UI::Window::AddButton)!");
					return nullptr;
				}
				res->m_Rect.CalcTransform(m_UI.GetSwapchainResolution(), m_Transform);
				return m_Buttons.PushBack(res);
			}

			bool CheckHover(IntVec2 cursorPos) {
				if (m_Rect.IsPointInside(cursorPos)) {
					for (Button* button : m_Buttons) {
						assert(button);
						if ((button->m_Rect.IsPointInside(cursorPos))) {
							button->m_HoverCallback(*button);
							break;
						}
					}
					return true;
				}
				return false;
			}

			void FramebufferSizeChange(Vec2_T<uint32_t> framebufferSize) {
				m_Rect.CalcTransform(framebufferSize, m_Transform);
				for (Button* button : m_Buttons) {
					assert(button);
					button->m_Rect.CalcTransform(framebufferSize, button->m_Transform);
				}
			}

			IntVec2 GetPosition() const {
				return m_Rect.m_Min;
			}

			/*! @brief Sets the position of window.
			* @param[in] pos: New window position from top left of the window.
			*/
			void SetPosition(IntVec2 pos) {
				IntVec2 oldPos = GetPosition();
				IntVec2 deltaPos = pos - oldPos;
				IntVec2 size = m_Rect.m_Max - m_Rect.m_Min;
				m_Rect.m_Min = pos;
				m_Rect.m_Max = pos + size;
				Vec2_T<uint32_t> renderResolution = m_UI.GetSwapchainResolution();
				m_Rect.CalcTransform(renderResolution, m_Transform);
				for (Button* button : m_Buttons) {
					assert(button);
					IntVec2 buttonPos = button->GetLocalPosition();
					button->SetLocalPosition(*this, buttonPos + deltaPos, renderResolution);
				}
			}

			uint64_t operator()() {
				return String::Hash()(m_StringID);
			}
		};

		typedef Window::Button Button;

		class Entity {

			friend class UI;

		private:

			uint64_t m_ID;

		public:

			virtual void UILoop(UI& UI) = 0;
		};

		struct RenderData {

			RenderData(uint32_t textureIndex, VkDescriptorSet descriptorSet, const Mat4& transform) 
				: m_TextureIndex(textureIndex), m_DescriptorSet(descriptorSet), m_Transform(transform) {}

			RenderData() : m_TextureIndex(0), m_DescriptorSet(VK_NULL_HANDLE), m_Transform(0) {}

			const uint32_t m_TextureIndex;
			const VkDescriptorSet m_DescriptorSet;
			const Mat4 m_Transform;
		};

		struct TextRenderData {

			const VkDescriptorSet m_DescriptorSet;
			const Mat4 m_Transform;
			TextFragmentPushConstant m_FragmentPushConstant;

			TextRenderData(VkDescriptorSet descriptorSet, const Mat4& transform, const TextFragmentPushConstant& fragPushConstant)
				: m_DescriptorSet(descriptorSet), m_Transform(transform), m_FragmentPushConstant(fragPushConstant) {}

			TextRenderData()
				: m_DescriptorSet(VK_NULL_HANDLE), m_Transform(0), m_FragmentPushConstant() {}
		};

	private:

		static constexpr uint32_t sc_UIRenderResolutionHeight1080p = 600;

		static inline DynamicArray<GLFWwindow*> s_GLFWwindows{};
		static inline DynamicArray<UI*> s_UIs{};

		static UI* FindUI(const GLFWwindow* pGlfwWindow) {
			size_t count = s_GLFWwindows.Size();
			for (size_t i = 0; i < count; i++) {
				if (s_GLFWwindows[i] == pGlfwWindow) {
					return s_UIs[i];
				}
			}
			return nullptr;
		}

		Vec2_T<uint32_t> m_UIRenderResolution;

		MeshData m_StaticQuadMesh2DData{};

		uint64_t m_NextEntityID = 0;
		DynamicArray<Entity*> m_Entities{};

		Dictionary<Window> m_WindowLookUp;
		DynamicArray<Window*> m_Windows{};

		DynamicArray<RenderData> m_RenderDatas{};

		DynamicArray<TextRenderData> m_TextRenderDatas{};

		Pipelines m_Pipelines;
		DynamicArray<VkDescriptorSet> m_RenderColorImageDescriptorSets{};

		IntVec2 m_CursorPosition{};

		VkFormat m_ColorFormat = VK_FORMAT_UNDEFINED;
		VkDescriptorPool m_RenderColorImagesDescriptorPool = VK_NULL_HANDLE;
		DynamicArray<VkImageView> m_RenderColorImageViews{};
		DynamicArray<VkDeviceMemory> m_RenderColorImagesMemory{};
		DynamicArray<VkImage> m_RenderColorImages{};
		VkSampler m_Sampler = VK_NULL_HANDLE;

		Renderer& m_Renderer;
		TextRenderer& m_TextRenderer;

		UI(Renderer& renderer, TextRenderer& textRenderer, size_t maxWindows) 
			: m_Renderer(renderer), m_TextRenderer(textRenderer), m_Pipelines(*this), m_WindowLookUp(maxWindows * 2) {
			m_Windows.Reserve(maxWindows);
			s_UIs.Reserve(4);
			s_GLFWwindows.Reserve(4);
			m_RenderDatas.Reserve(250);
			m_TextRenderDatas.Reserve(500);
			m_Entities.Reserve(250);
		}

		UI(const UI&) = delete;
		UI(UI&&) = delete;	

		void Initialize(const StaticMesh& quadMesh2D) {

			s_UIs.PushBack(this);
			GLFWwindow* pGLFWwindow = s_GLFWwindows.PushBack(m_Renderer.m_Window);

			assert(s_GLFWwindows.Size() == s_UIs.Size());

			glfwSetCursorPosCallback(pGLFWwindow, 
				[](GLFWwindow* pGLFWWindow, double xPos, double yPos) {
					UI* pUI = FindUI(pGLFWWindow);
					if (!pUI) {
						PrintError(ErrorOrigin::UI, 
							"failed to find UI (in GLFW cursor pos callback)!");
						return;
					}
					pUI->m_CursorPosition = { (int)xPos, (int)yPos };
					for (Window* window : pUI->m_Windows) {
						assert(window);
						if (window->CheckHover(pUI->m_CursorPosition)) {
							break;
						}
					}

				}
			);

			m_StaticQuadMesh2DData = quadMesh2D.GetMeshData();

			m_Pipelines.Initialize();
		}	

		void Terminate() {
			m_Renderer.DestroyDescriptorPool(m_RenderColorImagesDescriptorPool);
			for (uint32_t i = 0; i < m_RenderColorImages.Size(); i++) {
				m_Renderer.DestroyImageView(m_RenderColorImageViews[i]);
				m_Renderer.DestroyImage(m_RenderColorImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_RenderColorImagesMemory[i]);
			}
			m_Renderer.DestroySampler(m_Sampler);
			m_Pipelines.Terminate();
		}

		void SetViewportToUIRenderResolution(const Renderer::DrawData& drawData) {
			const VkRect2D scissor {
				.offset { 0, 0 },
				.extent { m_UIRenderResolution.x, m_UIRenderResolution.y },
			};
			vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
			const VkViewport viewport {
				.x = 0,
				.y = 0,
				.width = (float)m_UIRenderResolution.x,
				.height = (float)m_UIRenderResolution.y,
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);
		}

	public:

		Vec2_T<uint32_t> GetSwapchainResolution() {
			return Vec2_T<uint32_t>(m_Renderer.m_SwapchainExtent.width, m_Renderer.m_SwapchainExtent.height);
		}

		IntVec2 GetCursorPosition() const {
			return m_CursorPosition;
		}

		bool AddEntity(Entity* entity) {
			if (!entity) {
				PrintError(ErrorOrigin::UI, 
					"attempting to add a null entity to UI (in function UI::AddEntity)");
				return false;
			}
			m_Entities.PushBack(entity);
			entity->m_ID = m_NextEntityID++;
			return true;
		}

		bool RemoveEntity(Entity* entity) {
			if (!entity) {
				PrintError(ErrorOrigin::UI, 
					"attempting to remove a null entity from UI (in function UI::RemoveEntity)");
				return false;
			}
			const auto end = m_Entities.end();
			for (auto iter = m_Entities.begin(); iter != end; iter++) {
				if ((*iter)->m_ID == entity->m_ID) {
					m_Entities.Erase(iter);
					return true;
				}
			}
			PrintError(ErrorOrigin::UI, 
				"couldn't find entity (function UI::RemoveEntity)!");
			return false;
		}

		bool AddRenderData(const DynamicText& text) {
			if (text.m_DescriptorSet == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::UI, 
					"attempting to add render data with uninitialized dynamic text that's null (in function Window::AddRenderData)!");
				return false;
			}
			uint32_t nextRenderedCharIndex = 0;
			IntVec2 textPos = text.m_Position;
			for (const DynamicText::TRCharacter* trCharacter : text.m_TextRendererCharacters) {
				if (trCharacter->m_Size != 0) {
					assert(nextRenderedCharIndex < text.m_RenderedCharacters.Size());
					const DynamicText::Character& character = text.m_RenderedCharacters[nextRenderedCharIndex++];
					IntVec2 charPos = textPos - IntVec2(0, trCharacter->m_Bearing.y) + character.m_Offset;
					Rect rect {
						.m_Min = charPos,
						.m_Max = charPos + trCharacter->m_Size,
					};
					Mat4 transform;
					rect.CalcTransform(GetSwapchainResolution(), transform);
					TextFragmentPushConstant fragPushConst = character.m_FragmentPushConstant;
					m_TextRenderDatas.EmplaceBack(text.m_DescriptorSet, transform, character.m_FragmentPushConstant);
				}
				textPos.x += trCharacter->m_Escapement.x;
			}
			return true;
		}

		bool AddRenderData(const StaticText& text) {
			if (text.m_DescriptorSet == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::UI, 
					"attempting to add render data with uninitialized static text that's null (in function Window::AddRenderData)!");
				return false;
			}
			Rect rect {
				.m_Min = text.m_Position,
				.m_Max = text.m_Position + text.m_FrameExtent,
			};
			Mat4 transform;
			rect.CalcTransform(GetSwapchainResolution(), transform);
			m_RenderDatas.EmplaceBack(0, text.m_DescriptorSet, transform);
			return true;
		}

	private:

		void UILoop() {
			for (Entity* entity : m_Entities) {
				entity->UILoop(*this);
			}
		}

		void RenderUI(const Renderer::DrawData& drawData) {
			{
				SetViewportToUIRenderResolution(drawData);
				VkRenderingAttachmentInfo colorAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = m_RenderColorImageViews[drawData.m_CurrentFrame],
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { 0, 0, 0, 0 },
				};
				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea {
						.offset { 0, 0 },
						.extent { m_UIRenderResolution.x, m_UIRenderResolution.y },
					},
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment,
					.pDepthAttachment = nullptr,
					.pStencilAttachment = nullptr,
				};
				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipeline);
				for (Window* window : m_Windows) {
					window->Render(drawData.m_CommandBuffer);
				}
				for (RenderData& renderData : m_RenderDatas) {
					vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipelineLayout,
						0, 1, &renderData.m_DescriptorSet, 0, nullptr);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DrawPipelineLayout, 
						VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &renderData.m_Transform);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DrawPipelineLayout, 
						VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16, &renderData.m_TextureIndex);
					m_Renderer.DrawIndexed(drawData.m_CommandBuffer, m_StaticQuadMesh2DData);
				}
				m_RenderDatas.Resize(0);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_TextDrawPipeline);
				for (TextRenderData& renderData : m_TextRenderDatas) {
					vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_TextDrawPipelineLayout,
						0, 1, &renderData.m_DescriptorSet, 0, nullptr);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_TextDrawPipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &renderData.m_Transform);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_TextDrawPipelineLayout,
						VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof(TextFragmentPushConstant), &renderData.m_FragmentPushConstant);
					m_Renderer.DrawIndexed(drawData.m_CommandBuffer, m_StaticQuadMesh2DData);
				}
				m_TextRenderDatas.Resize(0);
				vkCmdEndRendering(drawData.m_CommandBuffer);
			}
			{
				VkImage image = m_RenderColorImages[drawData.m_CurrentFrame];
				VkImageMemoryBarrier memoryBarrier1 {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				};
				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier1);
				m_Renderer.SetViewportToSwapchainExtent(drawData);
				VkRenderingAttachmentInfo colorAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = drawData.m_SwapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { 0, 0, 0, 0 },
				};
				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea {
						.offset { 0, 0 },
						.extent { drawData.m_SwapchainExtent },
					},
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment,
					.pDepthAttachment = nullptr,
					.pStencilAttachment = nullptr,
				};
				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_RenderPipeline);
				vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
					m_Pipelines.m_RenderPipelineLayout, 0, 1, &m_RenderColorImageDescriptorSets[drawData.m_CurrentFrame], 0, nullptr);
				vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, m_StaticQuadMesh2DData.m_VertexBuffers, m_StaticQuadMesh2DData.m_VertexBufferOffsets);
				vkCmdBindIndexBuffer(drawData.m_CommandBuffer, m_StaticQuadMesh2DData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawData.m_CommandBuffer, m_StaticQuadMesh2DData.m_IndexCount, 1, 0, 0, 0);
				vkCmdEndRendering(drawData.m_CommandBuffer);

				VkImageMemoryBarrier memoryBarrier2 {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				};
				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier2);
			}
		}

	public:

		template<uint32_t texture_count_T>
		bool CreateTexture2DArray(const VkImageView imageViews[texture_count_T], VkDescriptorSet& outDescriptorSet, 
			VkDescriptorPool& outDescriptorPool)
		{
			VkDescriptorPoolSize poolSize {
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = texture_count_T,
			};
			outDescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, 1, &poolSize);
			if (outDescriptorPool == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Renderer, 
					"failed to create descriptor pool (function Renderer::CreateDescriptorPool in function UI::CreateTexture2DArray)!");
				return false;
			}
			uint32_t count = texture_count_T;
			VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorCountAllocInfo {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorSetCount = 1,
				.pDescriptorCounts = &count,
			};
			if (!m_Renderer.AllocateDescriptorSets(&descriptorCountAllocInfo, outDescriptorPool, 1,
					&m_Pipelines.m_DrawDescriptorSetLayout, &outDescriptorSet)) {
				PrintError(ErrorOrigin::Renderer, 
					"failed to allocate descriptor sets (function Renderer::AllocateDescriptorSets in function UI::CreateTexture2DArray)!");
				m_Renderer.DestroyDescriptorPool(outDescriptorPool);
				outDescriptorPool = VK_NULL_HANDLE;
				return false;
			}
			VkDescriptorImageInfo imageInfos[texture_count_T]{};
			for (uint32_t i = 0; i < texture_count_T; i++) {
				imageInfos[i] = {
					.sampler = m_Sampler,
					.imageView = imageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
			}
			VkWriteDescriptorSet write {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = outDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = texture_count_T,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = imageInfos,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			};
			m_Renderer.UpdateDescriptorSets(1, &write);
			return true;
		}

		Window* AddWindow(const char* stringID, WindowState state, IntVec2 position, Vec2_T<uint32_t> size) {
			if (m_WindowLookUp.Contains(stringID)) {
				PrintError(ErrorOrigin::UI, 
					"attempting to add window with an already existing string ID (in function UI::AddWindow)!");
				return nullptr;
			}
			Window* res = m_WindowLookUp.Emplace(stringID, *this, stringID, state, 
				Rect { .m_Min { position }, .m_Max { position + size } });
			if (!res) {
				PrintError(ErrorOrigin::UI, "failed to add window (function UI::Dictionary::Emplace in function UI::AddWindow)!");
				return nullptr;
			}
			return m_Windows.PushBack(res);
		}

	private:

		void SwapchainCreateCallback(Vec2_T<uint32_t> swapchainExtent, float aspectRatio, uint32_t imageCount) {
			uint32_t uiRenderResHeight = sc_UIRenderResolutionHeight1080p * (float)swapchainExtent.y / 1080;
			m_UIRenderResolution = { (uint32_t)(uiRenderResHeight * aspectRatio), uiRenderResHeight};
			if (m_ColorFormat == VK_FORMAT_UNDEFINED) {
				VkFormat candidates[3] { 
					VK_FORMAT_R8G8B8A8_SRGB,
					VK_FORMAT_R8G8B8A8_UINT,
				};
				m_ColorFormat = m_Renderer.FindSupportedFormat(3, candidates, VK_IMAGE_TILING_OPTIMAL, 
									VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
				if (m_ColorFormat == VK_FORMAT_UNDEFINED) {
					CriticalError(ErrorOrigin::Renderer, 
						"couldn't find suitable color format for UI (function Renderer::FindSupportedFormat in function UI::SwapchainCreateCallback)!");
				}
			}
			if (m_Pipelines.m_RenderDescriptorSetLayout == VK_NULL_HANDLE) {

				const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding 
					= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

				m_Pipelines.m_RenderDescriptorSetLayout
					= m_Renderer.CreateDescriptorSetLayout(nullptr, 1, &descriptorSetLayoutBinding);

				if (m_Pipelines.m_RenderDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create render descriptor set layout for UI (function Renderer::CreateDescriptorSetLayout in function UI::SwapchainCreateCallback)!");
				}
			}
			if (m_Sampler == VK_NULL_HANDLE) {
				m_Sampler = m_Renderer.CreateSampler(Renderer::GetDefaultSamplerInfo());

				if (m_Sampler == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create sampler for UI (function Renderer::CreateSampler in function UI::SwapchainCreateCallback)!");
				}
			}
			for (uint32_t i = 0; i < m_RenderColorImages.Size(); i++) {
				m_Renderer.DestroyImageView(m_RenderColorImageViews[i]);
				m_Renderer.DestroyImage(m_RenderColorImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_RenderColorImagesMemory[i]);
			}
			m_RenderColorImageViews.Resize(imageCount);
			m_RenderColorImages.Resize(imageCount);
			m_RenderColorImagesMemory.Resize(imageCount);
			VkExtent3D colorImageExtent = { m_UIRenderResolution.x, m_UIRenderResolution.y, 1 };
			uint32_t colorImageQueueFamilyIndex = m_Renderer.m_GraphicsQueueFamilyIndex;
			m_Renderer.DestroyDescriptorPool(m_RenderColorImagesDescriptorPool);
			DynamicArray<VkDescriptorPoolSize> poolSizes(imageCount);
			for (VkDescriptorPoolSize& poolSize : poolSizes) {
				poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				poolSize.descriptorCount = 1;
			}
			m_RenderColorImagesDescriptorPool = m_Renderer.CreateDescriptorPool(0, imageCount, imageCount, poolSizes.Data());
			if (m_RenderColorImagesDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create descriptor pool for UI (function Renderer::CreateDescriptorPool in function UI::SwapchainCreateCallback)!");
			}
			m_RenderColorImageDescriptorSets.Resize(imageCount);
			DynamicArray<VkDescriptorSetLayout> descriptorSetLayouts(imageCount);
			for (VkDescriptorSetLayout& layout : descriptorSetLayouts) {
				layout = m_Pipelines.m_RenderDescriptorSetLayout;
			}
			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_RenderColorImagesDescriptorPool, imageCount, 
				descriptorSetLayouts.Data(), m_RenderColorImageDescriptorSets.Data())) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate descriptor sets for UI (function Renderer::AllocateDescriptorSets in function UI::SwapchainCreateCallback)!");
			}

			LockGuard graphicsQueueLockGuard(m_Renderer.m_EarlyGraphicsCommandBufferQueueMutex);
			Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
				= m_Renderer.m_EarlyGraphicsCommandBufferQueue.New();
			if (!commandBuffer) {
				CriticalError(ErrorOrigin::Renderer,
					"renderer graphics command buffer was out of memory (in function UI::SwapchainCreateCallback)!");
			}
			if (!m_Renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(
				m_Renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
				&commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function UI::SwapchainCreateCallback)");
			}
			if (!m_Renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to begin command buffer (function Renderer::BeginCommandBuffer in function UI::SwapchainCreateCallback)");
			}

			for (uint32_t i = 0; i < imageCount; i++) {
				VkImage& image = m_RenderColorImages[i];
				VkDeviceMemory& memory = m_RenderColorImagesMemory[i];
				VkImageView& imageView = m_RenderColorImageViews[i];
				image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorFormat, colorImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &colorImageQueueFamilyIndex);
				if (image == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create render color image for UI (function Renderer::CreateImage in function UI::SwapchainCreateCallback)!");
				}
				memory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				if (memory == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate render color image memory for UI (function Renderer::AllocateImageMemory in function UI::SwapchainCreateCallback)!");
				}
				imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				if (imageView == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create render color image view for UI (function Renderer::CreateImageView in function UI::swapchainCreateCallback)");
				}
				VkDescriptorImageInfo imageInfo {
					.sampler = m_Sampler,
					.imageView = imageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
				VkWriteDescriptorSet write = m_Renderer.GetDescriptorWrite(nullptr, 0, m_RenderColorImageDescriptorSets[i], 
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, nullptr);
				m_Renderer.UpdateDescriptorSets(1, &write);

				VkImageMemoryBarrier memoryBarrier {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					}
				};

				vkCmdPipelineBarrier(commandBuffer->m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
			}

			VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
			if (vkRes != VK_SUCCESS) {
				CriticalError(ErrorOrigin::Vulkan, 
					"failed to end command buffer (function vkEndCommandBuffer in function UI::SwapchainCreateCallback)!", 
					vkRes);
			}

			commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;

			for (Window* window : m_Windows) {
				assert(window);
				window->FramebufferSizeChange(swapchainExtent);
			}
		}
	};

	class World {

		friend class Engine;

	public:

		class Shaders {
		public:

			static constexpr const char* pbr_draw_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outPosition;
layout(location = 2) out vec3 outNormal;

layout(set = 0, binding = 0) uniform CameraMatrices {
mat4 c_Projection;
mat4 c_View;
} camera_matrices;

layout(push_constant) uniform PushConstant {
layout(offset = 0) 
mat4 c_Transform;
mat4 c_NormalMatrix;
} pc;

void main() {

vec3 modelPos = vec3(inPosition.x, -inPosition.y, inPosition.z);

outUV = inUV;

outNormal = normalize(vec3(pc.c_NormalMatrix * vec4(inNormal, 0.0f)));

outPosition = vec3(pc.c_Transform * vec4(modelPos, 1.0f));

gl_Position = camera_matrices.c_Projection * camera_matrices.c_View * pc.c_Transform * vec4(modelPos, 1.0f);
}
			)";

			static constexpr const char* pbr_draw_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outDiffuseColor;
layout(location = 1) out vec4 outPositionAndMetallic;
layout(location = 2) out vec4 outNormalAndRougness;

layout(set = 1, binding = 0) uniform sampler2D diffuse_map;

void main() {

outDiffuseColor = texture(diffuse_map, inUV);
outPositionAndMetallic = vec4(inPosition, 1.0f);
outNormalAndRougness = vec4(inNormal, 1.0f);
}
			)";

			static constexpr const char * ud_draw_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(push_constant) uniform PushConstant {
layout(offset = 0)
mat4 c_LightView;
mat4 c_Transform;
} pc;

void main() {
gl_Position = pc.c_LightView * pc.c_Transform * vec4(inPosition.x, -inPosition.y, inPosition.z, 1.0f);
}
			)";

			static constexpr const char* pbr_render_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main() {
outUV = inUV;
gl_Position = vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
			)";

			static constexpr const char* pbr_render_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D diffuse_colors;
layout(set = 0, binding = 1) uniform sampler2D position_and_metallic;
layout(set = 0, binding = 2) uniform sampler2D normal_and_roughness;

layout(set = 1, binding = 0) uniform sampler2D directional_light_shadow_map;

layout(set = 1, binding = 1) uniform DirectionalLight {
mat4 c_ViewSpaceMatrix;
vec3 c_Direction;
vec3 c_Color;
} directional_light;

bool IsInShadowDirLight(vec4 lightViewPos) {

const vec4 shadowMapCoords = lightViewPos / lightViewPos.w;

if (shadowMapCoords.z > -1.0f && shadowMapCoords.z < 1.0f) {
	float dist = texture(directional_light_shadow_map, shadowMapCoords.st * 0.5f + 0.5f).r;
	float bias = 0.005f;
	return shadowMapCoords.w > 0.0f && dist < shadowMapCoords.z - bias;
}

return false;
}

void main() {

const vec4 modelPosAndMetal = texture(position_and_metallic, inUV);

const vec3 pos = modelPosAndMetal.xyz;
const vec3 normal = vec3(texture(normal_and_roughness, inUV));	

vec4 lightViewPos
	= directional_light.c_ViewSpaceMatrix * vec4(modelPosAndMetal.xyz, 1.0f);

vec3 lightDir = directional_light.c_Direction;

const float diff = IsInShadowDirLight(lightViewPos) ? 0.0f : max(dot(normal, lightDir), 0.0f);

const vec3 diffuse = diff * directional_light.c_Color;

vec3 color = (vec3(0.2f, 0.2f, 0.2f) + diffuse) * vec3(texture(diffuse_colors, inUV));
float gamma = 2.2f;
color = pow(color, vec3(1.0f / gamma));

outColor = vec4(color, 1.0f);
}
			)";

			static constexpr const char* debug_pipeline_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(set = 0, binding = 0) uniform CameraMatrices {
mat4 c_Projection;
mat4 c_View;
} camera_matrices;

layout(push_constant) uniform PushConstant {
layout(offset = 0) mat4 c_Transform;
} pc;

void main() {
gl_Position = camera_matrices.c_Projection * camera_matrices.c_View * pc.c_Transform * vec4(inPosition, 1.0f);
}
			)";

			static constexpr const char* debug_pipeline_fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant {
layout(offset = 64) vec4 c_Color;
} pc;

void main() {
outColor = pc.c_Color;
}
			)";
		};

		class Pipelines {

			friend class World;

			VkPipeline m_DrawPipelinePBR = VK_NULL_HANDLE;
			VkPipelineLayout m_DrawPipelineLayoutPBR = VK_NULL_HANDLE;

			VkPipeline m_DrawPipelineUD{};
			VkPipelineLayout m_DrawPipelineLayoutUD{};

			VkPipeline m_RenderPipelinePBR = VK_NULL_HANDLE;
			VkPipelineLayout m_RenderPipelineLayoutPBR = VK_NULL_HANDLE;

			VkPipeline m_DebugPipeline = VK_NULL_HANDLE;
			VkPipelineLayout m_DebugPipelineLayout = VK_NULL_HANDLE;

			VkDescriptorSetLayout m_DirectionalLightShadowMapDescriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_SingleTextureDescriptorSetLayoutPBR = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;

			void Initialize(Renderer& renderer, VkFormat colorImageResourceFormat) {

				static constexpr VkDescriptorSetLayoutBinding camera_descriptor_set_layout_binding 
					= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

				m_CameraDescriptorSetLayout = renderer.CreateDescriptorSetLayout(nullptr, 1, &camera_descriptor_set_layout_binding);

				if (m_CameraDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create camera descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::Pipelines::Initialize)!");
				}

				static constexpr VkDescriptorSetLayoutBinding texture_descriptor_set_layout_binding
					= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

				m_SingleTextureDescriptorSetLayoutPBR = renderer.CreateDescriptorSetLayout(nullptr, 1, &texture_descriptor_set_layout_binding);

				if (m_SingleTextureDescriptorSetLayoutPBR == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create albedo descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::Pipelines::Initialize)!");
				}

				const VkPushConstantRange pbrDrawPushConstantRange {
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = 128,
				};

				const VkDescriptorSetLayout drawPbrDescriptorSetLayouts[2] {
					m_CameraDescriptorSetLayout,
					m_SingleTextureDescriptorSetLayoutPBR,
				};

				m_DrawPipelineLayoutPBR 
					= renderer.CreatePipelineLayout(2, drawPbrDescriptorSetLayouts, 1, &pbrDrawPushConstantRange);

				if (m_DrawPipelineLayoutPBR == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create pbr draw pipeline layout for world (function Renderer::CreatePipelineLayout in function World::Pipelines::Initialize)!");
				}

				const VkPushConstantRange udDrawPipelinePushConstantRange {
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = 128,
				};

				m_DrawPipelineLayoutUD = renderer.CreatePipelineLayout(0, nullptr, 1, &udDrawPipelinePushConstantRange);

				if (m_DrawPipelineLayoutUD == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create unidirectional light pipeline layout (function Renderer::CreateDescriptorSetLayout in function World::Pipelines::Initialize)!");
				}

				const VkDescriptorSetLayout pbrRenderPipelineDescriptorSetLayouts[2] {
					m_RenderPBRImagesDescriptorSetLayout,
					m_DirectionalLightShadowMapDescriptorSetLayout,
				};

				m_RenderPipelineLayoutPBR 
						= renderer.CreatePipelineLayout(2, pbrRenderPipelineDescriptorSetLayouts, 0, nullptr);

				if (m_RenderPipelineLayoutPBR == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"faileld to create pbr render pipeline layout for world (function Renderer::CreatePipelineLayout in function World::Pipelines::Initialize)!");
				}

				const VkPushConstantRange debugPushConstantRanges[2] {
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64),
					Renderer::GetPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16),
				};

				m_DebugPipelineLayout = renderer.CreatePipelineLayout(1, &m_CameraDescriptorSetLayout, 2, debugPushConstantRanges);

				if (m_DebugPipelineLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create debug pipeline layout for world (function Renderer::CreatePipelineLayout in function World::Pipelines::initialize)");
				}

				Renderer::Shader pbrDrawShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!pbrDrawShaders[0].Compile(Shaders::pbr_draw_pipeline_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr draw vertex shader code (function Renderer::Shader::Compile in function World::Pipelines::Initialize)!");
				}

				if (!pbrDrawShaders[1].Compile(Shaders::pbr_draw_pipeline_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr draw fragment shader code (function Renderer::Shader::Compile in function World::Pipelines::Initialize)!");
				}

				const VkPipelineShaderStageCreateInfo pbrDrawPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[1]),
				};

				Renderer::Shader udDrawVertexShader(renderer, VK_SHADER_STAGE_VERTEX_BIT);

				if (!udDrawVertexShader.Compile(Shaders::ud_draw_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile unidirectional light draw vertex shader (function Renderer::CreateDescriptorSetLayout in function World::Pipelines::Initialize)!");
				}

				const VkPipelineShaderStageCreateInfo udDrawPipelineShaderStageInfo 
					= Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(udDrawVertexShader);

				Renderer::Shader pbrRenderShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!pbrRenderShaders[0].Compile(Shaders::pbr_render_pipeline_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr render vertex shader code (function Renderer::Shader::Compile in function World::Pipelines::Initialize)!");
				}

				if (!pbrRenderShaders[1].Compile(Shaders::pbr_render_pipeline_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr render fragment shader code (function Renderer::Shader::Compile in function World::Pipeliens:.Initialize)");
				}

				const VkPipelineShaderStageCreateInfo pbrRenderPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[1]),
				};

				Renderer::Shader debugShaders[2] {
					{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
					{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
				};

				if (!debugShaders[0].Compile(Shaders::debug_pipeline_vertex_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile vertex shader code (function Renderer::Shader::Compile in function World::Pipelines::Initialize)!");
				}

				if (!debugShaders[1].Compile(Shaders::debug_pipeline_fragment_shader)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile fragment shader code (function Renderer::Shader::Compile in function World::Pipelines::Initialize)!");
				}

				const VkPipelineShaderStageCreateInfo debugPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[0]),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[1]),
				};

				static constexpr uint32_t pbr_draw_color_attachment_count = 3;

				const VkFormat pbrDrawRenderingColorFormats[pbr_draw_color_attachment_count] {
					colorImageResourceFormat,
					colorImageResourceFormat,
					colorImageResourceFormat,
				};

				const VkPipelineRenderingCreateInfo pbrDrawPipelineRenderingInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(pbr_draw_color_attachment_count, pbrDrawRenderingColorFormats, renderer.m_DepthOnlyFormat);

				const VkPipelineRenderingCreateInfo udPipelineRenderingCreateInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(0, nullptr, renderer.m_DepthOnlyFormat);

				const VkPipelineRenderingCreateInfo pbrRenderPipelineRenderingInfo
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, VK_FORMAT_UNDEFINED);

				const VkPipelineRenderingCreateInfo debugPipelineRenderingInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, 
						renderer.m_DepthOnlyFormat);

				VkPipelineColorBlendStateCreateInfo pbrDrawPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
				pbrDrawPipelineColorBlendState.attachmentCount = pbr_draw_color_attachment_count;
				VkPipelineColorBlendAttachmentState pbrDrawPipelineColorAttachmentStates[pbr_draw_color_attachment_count] {
					Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
					Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
					Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
				};
				pbrDrawPipelineColorBlendState.pAttachments = pbrDrawPipelineColorAttachmentStates;

				VkPipelineColorBlendStateCreateInfo pbrRenderPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
				pbrRenderPipelineColorBlendState.attachmentCount = 1;
				pbrRenderPipelineColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend;

				VkPipelineColorBlendStateCreateInfo debugPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
				debugPipelineColorBlendState.attachmentCount = 1;
				debugPipelineColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state;

				VkPipelineRasterizationStateCreateInfo debugPipelineRasterizationState = Renderer::GraphicsPipelineDefaults::rasterization_state;
				debugPipelineRasterizationState.polygonMode = VK_POLYGON_MODE_LINE;

				static constexpr uint32_t pipeline_count = 4;

				VkGraphicsPipelineCreateInfo graphicsPipelineInfos[pipeline_count] = { 
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &pbrDrawPipelineRenderingInfo,
						.stageCount = 2,
						.pStages = pbrDrawPipelineShaderStageInfos,
						.pVertexInputState = &Vertex::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
						.pColorBlendState = &pbrDrawPipelineColorBlendState,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_DrawPipelineLayoutPBR,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &udPipelineRenderingCreateInfo,
						.flags = 0,
						.stageCount = 1,
						.pStages = &udDrawPipelineShaderStageInfo,
						.pVertexInputState = &Vertex::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
						.pColorBlendState = &Renderer::GraphicsPipelineDefaults::color_blend_state,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_DrawPipelineLayoutUD,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &pbrRenderPipelineRenderingInfo,
						.stageCount = 2,
						.pStages = pbrRenderPipelineShaderStageInfos,
						.pVertexInputState = &Vertex2D::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
						.pColorBlendState = &pbrRenderPipelineColorBlendState,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_RenderPipelineLayoutPBR,
						.renderPass = nullptr,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
					{
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &debugPipelineRenderingInfo,
						.stageCount = 2,
						.pStages = debugPipelineShaderStageInfos,
						.pVertexInputState = &Vertex::GetVertexInputState(),
						.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
						.pTessellationState = nullptr,
						.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
						.pRasterizationState = &debugPipelineRasterizationState,
						.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
						.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
						.pColorBlendState = &debugPipelineColorBlendState,
						.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
						.layout = m_DebugPipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
				};

				VkPipeline pipelines[pipeline_count];

				if (!renderer.CreateGraphicsPipelines(pipeline_count, graphicsPipelineInfos, pipelines)) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create world graphics pipeline (function Renderer::CreateGraphicsPipelines in function World::Pipelines::Initialize)!");
				}

				m_DrawPipelinePBR = pipelines[0];
				m_DrawPipelineUD = pipelines[1];
				m_RenderPipelinePBR = pipelines[2];
				m_DebugPipeline = pipelines[3];
			}

			void Terminate(Renderer& renderer) {
				renderer.DestroyDescriptorSetLayout(m_CameraDescriptorSetLayout);
				renderer.DestroyDescriptorSetLayout(m_RenderPBRImagesDescriptorSetLayout);
				renderer.DestroyDescriptorSetLayout(m_DirectionalLightShadowMapDescriptorSetLayout);
				renderer.DestroyDescriptorSetLayout(m_SingleTextureDescriptorSetLayoutPBR);
				renderer.DestroyPipeline(m_DrawPipelinePBR);
				renderer.DestroyPipelineLayout(m_DrawPipelineLayoutPBR);
				renderer.DestroyPipeline(m_DrawPipelineUD);
				renderer.DestroyPipelineLayout(m_DrawPipelineLayoutUD);
				renderer.DestroyPipeline(m_RenderPipelinePBR);
				renderer.DestroyPipelineLayout(m_RenderPipelineLayoutPBR);
				renderer.DestroyPipeline(m_DebugPipeline);
				renderer.DestroyPipelineLayout(m_DebugPipelineLayout);
			}
		};

		class Ground : public PersistentReferenceHolder<Ground> {

			friend class World;
			friend class DynamicArray<Ground>;

		public:
			
			struct CreateInfo {
				const LogicMesh& m_LogicMesh;
				Mat4 m_Transform;
			};

		private:

			const uint64_t m_ObjectID;
			LogicMesh m_LogicMesh;
			Mat4 m_Transform;

			Ground(uint64_t objectID, const CreateInfo& createInfo) 
				: PersistentReferenceHolder<Ground>() , m_ObjectID(objectID), 
					m_LogicMesh(createInfo.m_LogicMesh), m_Transform(createInfo.m_Transform) {
				m_LogicMesh.UpdateTransform(m_Transform);
			}
			
			Ground(const Ground &) = delete;
			Ground(Ground&&) = default;

			bool AABBCheck(const Vec3& position) const {
				return m_LogicMesh.GetBoundingBox().IsPointInside(position);
			}

			bool RayCheck(const Ray& ray, RayHitInfo& outHitInfo) const {
				return m_LogicMesh.IsRayHit(ray, outHitInfo);
			}

		public:

			void UpdateTransform(const Mat4& transform) {
				m_Transform = transform;
				m_LogicMesh.UpdateTransform(m_Transform);
			}
		};

		class Obstacle : public PersistentReferenceHolder<Obstacle> {

			friend class World;
			friend class DynamicArray<Obstacle>;

		public:

			struct CreateInfo {
				Vec3 m_Position{};
				float m_YRotation{};
				Collider::CreateInfo m_ColliderInfo{};
			};

		private:

			uint64_t m_ObjectID;
			Vec3 m_Position;
			float m_YRotation;
			Collider m_Collider;

			Obstacle(uint64_t objectID, const CreateInfo& info) noexcept 
				: PersistentReferenceHolder<Obstacle>(), m_ObjectID(objectID), m_Position(info.m_Position),
					m_YRotation(info.m_YRotation), m_Collider(m_Position, m_YRotation, info.m_ColliderInfo) {}

			Obstacle(const Obstacle&) = delete;

			Obstacle(Obstacle&& other) noexcept : m_ObjectID(other.m_ObjectID),
				m_Position(other.m_Position), m_YRotation(other.m_YRotation), 
					m_Collider(m_Position, m_YRotation, std::move(other.m_Collider)) {}

			bool Collides(const Collider& collider, const Vec3& colliderVelocity, Vec3& outColliderPushBack) const {
				return Collider::ColliderToStaticColliderCollides(collider, colliderVelocity, m_Collider, outColliderPushBack);
			}
		};

		class Chunk {

			friend class World;
			friend class DynamicArray<Chunk>;

			const Rect<float> m_BoundingRect;
			const Vec2_T<uint32_t> m_ChunkMatrixCoords;
			DynamicArray<PersistentReference<Ground>> m_Grounds{};
			DynamicArray<PersistentReference<Obstacle>> m_Obstacles{};
			
			Chunk(Vec2_T<uint32_t> chunkCoords, Vec2 min, Vec2 dimensions) noexcept 
				: m_BoundingRect { .m_Min { min }, .m_Max { min + dimensions } }, m_ChunkMatrixCoords(chunkCoords), m_Grounds() {}

			bool IsPointInside(const Vec3& point) const {
				return m_BoundingRect.IsPointInside(Vec2(point.x, point.z));
			}

			const bool FindHeight(const Vec3& position, float rayLength, float& outHeight) const {
				Ray ray {
					.m_Origin = position,
					.m_Direction = Vec3::Down(),
					.m_Length = rayLength,
				};
				float maxHeight = float_min;
				for (const PersistentReference<Ground>& ref : m_Grounds) {
					assert(!ref.IsNull());
					const Ground& ground = *ref;
					ground.m_PersistentReferences.Data();
					if (ground.AABBCheck(position)) {
						RayHitInfo hitInfo;
						if (ground.RayCheck(ray, hitInfo)) {
							maxHeight = Max(hitInfo.m_HitPosition.y, maxHeight);
						}
					}
				}
				outHeight = maxHeight;
				return maxHeight != float_min;
			}
		};

		class Creature : public PersistentReferenceHolder<Creature> {

			friend class World;
			friend class DynamicArray<Creature>;

		public:

			typedef Vec3 (*MovementVectorUpdateFun)(const Creature& creature);
			typedef void (*MoveCallbackFun)(const Creature& creature, const Vec3& position, const Vec3& deltaPosition);
			typedef void (*CameraFollowCallbackFun)(const Creature& creature, Vec3& outCameraPos, Vec3& outCameraLookAt);

		private:	

			const Chunk* m_Chunk;
			Vec3 m_Position;
			float m_YRotation;
			const uint64_t m_ObjectID;
			Collider m_Collider;

			Creature(uint64_t objectID, const Vec3& position, const Chunk* chunk, const Collider::CreateInfo& colliderInfo)
				: PersistentReferenceHolder<Creature>(), m_Position(position), m_Chunk(chunk), m_ObjectID(objectID),
					m_Collider(m_Position, m_YRotation, colliderInfo) {
				assert(chunk);
				float height;
				if (chunk->FindHeight(position, 2.0f, height)) {
					m_Position.y = height;
				}
			}

			Creature(const Creature&) = delete;

			Creature(Creature&& other) noexcept 
				: m_Chunk(other.m_Chunk), m_Position(other.m_Position), m_YRotation(other.m_YRotation),
					m_ObjectID(other.m_ObjectID), m_Collider(m_Position, m_YRotation, std::move(other.m_Collider)) {}

		public:

			MovementVectorUpdateFun m_MovementVectorUpdate;
			MoveCallbackFun m_MoveCallback;
			CameraFollowCallbackFun m_CameraFollowCallback;

			const Vec3& GetPosition() const {
				return m_Position;
			}

		private:

			Vec3 GetMovementVector() const {
				if (m_MovementVectorUpdate) {
					return m_MovementVectorUpdate(*this);
				}
				return {};
			}

			void Move(const Vec3& position) {
				assert(m_Chunk);
				Vec3 deltaPos = position - m_Position;
				Vec2_T<bool> axisBlocked{};
				m_Position += deltaPos;
				/*
				const Ground* ground = m_Chunk->FindGround(m_Position, deltaPos, axisBlocked);
				if (ground) {
					m_Position.x = !axisBlocked.x ? position.x : m_Position.x;
					m_Position.z = !axisBlocked.y ? position.z : m_Position.z; 
					float height = ground->GetHeightAtPosition(m_Position);
					assert(height != std::numeric_limits<float>::max());
					m_Position.y = height;
				}
				*/
				for (const PersistentReference<Obstacle>& obstacle : m_Chunk->m_Obstacles) {
					assert(obstacle.m_Val);
					Vec3 pushBack;
					if (obstacle.m_Val->Collides(m_Collider, deltaPos, pushBack)) {
						Vec2_T<bool> deltaDirSameAsPushBack = { 
							deltaPos.x > 0 && pushBack.x > 0 || deltaPos.x < 0 && pushBack.x < 0,
							deltaPos.z > 0 && pushBack.z > 0 || deltaPos.z < 0 && pushBack.z < 0,
						};
						m_Position -= Vec3(
							deltaDirSameAsPushBack.x && axisBlocked.x ? 0.0f : pushBack.x, 0.0f, 
							deltaDirSameAsPushBack.y && axisBlocked.y ? 0.0f : pushBack.z
						);
					}
				}
				if (m_MoveCallback) {
					m_MoveCallback(*this, m_Position, deltaPos);
				}
			}
		};

		struct RenderData : PersistentReferenceHolder<RenderData> {

			friend class World;

		private:

			const uint64_t m_ObjectID;

		public:

			VkDescriptorSet m_AlbedoTextureDescriptorSet = VK_NULL_HANDLE;
			Mat4 m_Transform{};
			MeshData m_MeshData{};

			RenderData(uint64_t objectID, const Mat4& transform, const MeshData& meshData) noexcept 
				: PersistentReferenceHolder<RenderData>(), m_ObjectID(objectID), m_Transform(transform), m_MeshData(meshData) {}

			RenderData(const RenderData&) = delete;

			RenderData(RenderData&& other) noexcept = default;

		};	

		struct DebugRenderData : PersistentReferenceHolder<DebugRenderData> {

			friend class Engine;

		private:

			const uint64_t m_ObjectID;

		public:

			Mat4 m_Transform;
			Vec4 m_WireColor;
			MeshData m_MeshData;

			DebugRenderData(uint64_t objectID, const Mat4& transform, const Vec4& wireColor, const MeshData& meshData) noexcept 
				: PersistentReferenceHolder<DebugRenderData>(), 
					m_ObjectID(objectID), m_Transform(transform), m_WireColor(wireColor), m_MeshData(meshData) {}

			DebugRenderData(const DebugRenderData&) = delete;

			DebugRenderData(DebugRenderData&& other) noexcept = default;
		};

		struct TextureMap {

			friend class World;

		private:

			VkImageView m_ImageView{};
			VkDescriptorPool m_DescriptorPool{};

		public:

			VkDescriptorSet m_DescriptorSet{};
		};

		struct CameraMatricesBuffer {
			Mat4 m_Projection;
			Mat4 m_View;
		};

		class UnidirectionalLight {

			friend class World;

			struct Matrices {

				Mat4 m_Projection { 1 };
				Mat4 m_View { 1 };

				Mat4 GetLightViewMatrix() const {
					return m_Projection * m_View;
				}

				Vec3 GetDirection() const {
					return m_View.LookAtFront();
				}
			};

			enum class Type {
				Directional = 0,
				Spot = 1,
			};

			struct FragmentBufferDirectional {
				Mat4 m_ViewMatrix{};
				Vec3 m_Direction{};
				uint8_t pad0[4];
				Vec3 m_Color{};
			};

			struct FragmentBufferSpot {
				Mat4 m_ViewMatrix{};
				Vec3 m_Position{};
				uint8_t pad0[4];
				Vec3 m_Color{};
				uint8_t pad1[4];
				float angle;
			};

			World& m_World;

			const Type m_Type;

			DynamicArray<VkImageView> m_DepthImageViews{};
			Vec2_T<uint32_t> m_ShadowMapResolution;

			Matrices m_ViewMatrices{};

			DynamicArray<VkDescriptorSet> m_ShadowMapDescriptorSets{};

			void* m_FragmentMap{};

			DynamicArray<VkImage> m_DepthImages{};
			DynamicArray<VkDeviceMemory> m_DepthImagesMemory{};
			VkDescriptorPool m_ShadowMapDescriptorPool{};
			VkSampler m_ShadowMapSampler{};
			Renderer::Buffer m_FragmentBuffer;

			UnidirectionalLight(World& world, Type type, Vec2_T<uint32_t> shadowMapResolution) 
				: m_World(world), m_ShadowMapResolution(shadowMapResolution), m_Type(type),
					m_FragmentBuffer(world.m_Renderer) {}

			UnidirectionalLight(const UnidirectionalLight&) = delete;
			UnidirectionalLight(UnidirectionalLight&&) = delete;

			void Initialize(const Mat4& projection, const Mat4& view, const Vec3& color) {

				assert(m_Type == Type::Directional);

				Renderer& renderer = m_World.m_Renderer;

				uint32_t framesInFlight = renderer.m_FramesInFlight;	

				if (m_DepthImages.Size() != framesInFlight) {
					LockGuard graphicsQueueLockGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
					Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
						= renderer.m_EarlyGraphicsCommandBufferQueue.New();
					if (!commandBuffer) {
						CriticalError(ErrorOrigin::Renderer,
							"renderer graphics command buffer was out of memory (in function World::Initialize)!");
					}
					if (!renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(
							renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
							&commandBuffer->m_CommandBuffer)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function World::Initialize)");
					}
					if (!renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to begin command buffer (function Renderer::BeginCommandBuffer in function World::Initialize)");
					}
					SwapchainCreateCallback(framesInFlight, commandBuffer->m_CommandBuffer);
					VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
					if (vkRes != VK_SUCCESS) {
						CriticalError(ErrorOrigin::Vulkan, 
							"failed to end command buffer (function vkEndCommandBuffer in function World::Initialize)!",
						vkRes);
					}
					commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;
				}

				m_ViewMatrices.m_Projection = projection;
				m_ViewMatrices.m_View = view;

				*(FragmentBufferDirectional*)m_FragmentMap = {
					.m_ViewMatrix = m_ViewMatrices.GetLightViewMatrix(),
					.m_Direction = m_ViewMatrices.GetDirection(),
					.m_Color = color,
				};
			}

			void Terminate() {
				Renderer& renderer = m_World.m_Renderer;
				for (uint32_t i = 0; i < m_DepthImages.Size(); i++) {
					renderer.DestroyImageView(m_DepthImageViews[i]);
					renderer.DestroyImage(m_DepthImages[i]);
					renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
				}
				renderer.DestroyDescriptorPool(m_ShadowMapDescriptorPool);
				renderer.DestroySampler(m_ShadowMapSampler);
				m_FragmentBuffer.Terminate();
			}

			void DepthDraw(const Renderer::DrawData& drawData) const {

				VkImageMemoryBarrier memoryBarrier1 {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = m_DepthImages[drawData.m_CurrentFrame],
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
					0, 0, nullptr, 0, nullptr, 1, &memoryBarrier1);

				VkExtent2D extent { m_ShadowMapResolution.x, m_ShadowMapResolution.y };

				VkRect2D scissor {
					.offset = { 0, 0 },
					.extent = extent,
				};

				VkViewport viewport {
					.x = 0,
					.y = 0,
					.width = (float)extent.width,
					.height = (float)extent.height,
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				};

				vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
				vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);

				VkRenderingAttachmentInfo depthAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = m_DepthImageViews[drawData.m_CurrentFrame],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { .depthStencil { .depth = 1.0f, .stencil = 0 } },
				};

				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea = { .offset { 0, 0 }, .extent { extent }, },
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 0,
					.pColorAttachments = nullptr,
					.pDepthAttachment = &depthAttachment,
					.pStencilAttachment = nullptr,
				};

				const Pipelines& pipelines = m_World.m_Pipelines;

				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.m_DrawPipelineUD);
				Mat4 matrices[2] {
					m_ViewMatrices.GetLightViewMatrix(),
					{},
				};
				for (const RenderData& renderData : m_World.m_RenderDatas) {
					matrices[1] = renderData.m_Transform;
					vkCmdPushConstants(drawData.m_CommandBuffer, pipelines.m_DrawPipelineLayoutUD, VK_SHADER_STAGE_VERTEX_BIT, 0, 128, matrices);
					vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, renderData.m_MeshData.m_VertexBuffers, renderData.m_MeshData.m_VertexBufferOffsets);
					vkCmdBindIndexBuffer(drawData.m_CommandBuffer, renderData.m_MeshData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(drawData.m_CommandBuffer, renderData.m_MeshData.m_IndexCount, 1, 0, 0, 0);
				}	
				vkCmdEndRendering(drawData.m_CommandBuffer);

				scissor.extent = drawData.m_SwapchainExtent;
				viewport.width = (float)drawData.m_SwapchainExtent.width;
				viewport.height = (float)drawData.m_SwapchainExtent.height;
				vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
				vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);

				VkImageMemoryBarrier memoryBarrier2 {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = m_DepthImages[drawData.m_CurrentFrame],
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				};

				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
					0, 0, nullptr, 0, nullptr, 1, &memoryBarrier2);
			}

			VkDeviceSize FragmentBufferSize() const {
				return m_Type == Type::Directional ? sizeof(FragmentBufferDirectional) : sizeof(FragmentBufferSpot);
			}

			void SwapchainCreateCallback(uint32_t imageCount, VkCommandBuffer commandBuffer) {

				Renderer& renderer = m_World.m_Renderer;

				if (m_FragmentBuffer.IsNull()) {
					if (!m_FragmentBuffer.Create(FragmentBufferSize(), 
							VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create buffer for directional light (function Renderer::Buffer::Create in function World::DirectionalLight::SwapchainCreateCallback)!");
					}
					if (!m_FragmentBuffer.MapMemory(0, m_FragmentBuffer.m_BufferSize, (void**)&m_FragmentMap)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to map buffer memory for directional light (function Renderer::Buffer::MapMemory in function World::DirectionalLight::SwapchainCreateCallback)!");
					}
				}

				if (m_DepthImageViews.Size() != imageCount) {
					if (m_DepthImages.Size() < imageCount) {

						size_t oldImageCount = m_DepthImages.Size();

						m_DepthImages.Resize(imageCount);
						m_DepthImagesMemory.Resize(imageCount);
						m_DepthImageViews.Resize(imageCount);

						VkExtent3D extent = { m_ShadowMapResolution.x, m_ShadowMapResolution.y, 1 };

						for (size_t i = oldImageCount; i < imageCount; i++) {
							VkImage& image = m_DepthImages[i];
							VkDeviceMemory& memory = m_DepthImagesMemory[i];
							VkImageView& imageView = m_DepthImageViews[i];
							image = renderer.CreateImage(VK_IMAGE_TYPE_2D, renderer.m_DepthOnlyFormat, 
								extent, 1, 1, VK_SAMPLE_COUNT_1_BIT, 
								VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
								VK_SHARING_MODE_EXCLUSIVE, 1, &renderer.m_GraphicsQueueFamilyIndex);
							if (image == VK_NULL_HANDLE) {
								CriticalError(ErrorOrigin::Renderer, 
									"failed to create depth image for directional light (function Renderer::CreateImage in function World::DirectionalLight::SwapchainCreateCallback)!");
							}
							memory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
							if (memory == VK_NULL_HANDLE) {
								CriticalError(ErrorOrigin::Renderer, 
									"failed to allocate depth image memory for directional light (function Renderer::AllocateImageMemory in function World::DirectionalLight::SwapchainCreateCallback)!");
							}
							imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, renderer.m_DepthOnlyFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
							if (imageView == VK_NULL_HANDLE) {
								CriticalError(ErrorOrigin::Renderer, 
									"failed to create depth image view for directional light (function Renderer::AllocateImageMemory in function World::DirectionalLight::SwapchainCreateCallback)!");
							}

							VkImageMemoryBarrier memoryBarrier = {
								.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
								.pNext = nullptr,
								.srcAccessMask = 0,
								.dstAccessMask = 0,
								.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
								.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.image = image,
								.subresourceRange {
									.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
									.baseMipLevel = 0,
									.levelCount = 1,
									.baseArrayLayer = 0,
									.layerCount = 1,
								},
							};

							vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
								VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
						}	
					}
					else {
						for (size_t i = imageCount; i < m_DepthImages.Size(); i++) {
							renderer.DestroyImageView(m_DepthImageViews[i]);
							renderer.DestroyImage(m_DepthImages[i]);
							renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
						}
						m_DepthImages.Resize(imageCount);
						m_DepthImagesMemory.Resize(imageCount);
						m_DepthImageViews.Resize(imageCount);
					}
					if (m_ShadowMapSampler == VK_NULL_HANDLE) {
						m_ShadowMapSampler = renderer.CreateSampler(Renderer::GetDefaultSamplerInfo());
						if (m_ShadowMapSampler == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create shadow map sampler for directional light (function Renderer::CreateSampler in function World::DirectionalLight::SwapchainCreateCallback)!");
						}
					}
					if (m_ShadowMapDescriptorPool != VK_NULL_HANDLE) {
						renderer.DestroyDescriptorPool(m_ShadowMapDescriptorPool);
					}
					DynamicArray<VkDescriptorPoolSize> poolSizes(2 * imageCount);
					for (uint32_t i = 0; i < poolSizes.Size();) {
						poolSizes[i++] = {
							.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							.descriptorCount = 1,
						};
						poolSizes[i++] = {
							.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							.descriptorCount = 1,
						};
					}
					m_ShadowMapDescriptorPool = renderer.CreateDescriptorPool(0, imageCount, poolSizes.Size(), poolSizes.Data());
					if (m_ShadowMapDescriptorPool == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create descriptor pool for directional light (function Renderer::CreateDescriptorPool in function World::DirectionalLight::SwapchainCreateCallback)!");
					}
					m_ShadowMapDescriptorSets.Resize(imageCount);
					DynamicArray<VkDescriptorSetLayout> setLayouts(imageCount);
					for (VkDescriptorSetLayout& layout : setLayouts) {
						layout = m_World.m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout;
					}
					if (!renderer.AllocateDescriptorSets(nullptr, m_ShadowMapDescriptorPool, imageCount, setLayouts.Data(), m_ShadowMapDescriptorSets.Data())) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate descriptor sets for directional light (function Renderer::AllocateDescriptorSets in function World::DirectionalLight::SwapchainCreateCallback)!");
					}
					VkDescriptorBufferInfo descriptorBufferInfo {
						.buffer = m_FragmentBuffer.m_Buffer,
						.offset = 0,
						.range = FragmentBufferSize(),
					};
					for (uint32_t i = 0; i < imageCount; i++) {
						VkDescriptorImageInfo imageInfo {
							.sampler = m_ShadowMapSampler,
							.imageView = m_DepthImageViews[i],
							.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						};
						VkWriteDescriptorSet descriptorWrites[2] {
							Renderer::GetDescriptorWrite(nullptr, 0, m_ShadowMapDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, nullptr),
							Renderer::GetDescriptorWrite(nullptr, 1, m_ShadowMapDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &descriptorBufferInfo),
						};
						renderer.UpdateDescriptorSets(2, descriptorWrites);
					}
				}	
			}
		};

	private:

		Renderer& m_Renderer;

		struct StaticMeshFile {

			StaticMesh m_Mesh;

			DynamicArray<Tuple<PersistentReference<Ground>, Mat4>> m_Grounds{};
			DynamicArray<Tuple<PersistentReference<Obstacle>, Mat4>> m_Obstacles{};

			StaticMeshFile(Renderer& renderer) : m_Mesh(renderer) {}
		};

		struct StaticTextureFile {
	
			StaticTexture m_Texture;
			VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
			VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

			DynamicArray<PersistentReference<Ground>> m_Grounds{};
			DynamicArray<PersistentReference<Obstacle>> m_Obstacles{};

			StaticTextureFile(Renderer& renderer) : m_Texture(renderer) {}
		};

		Dictionary<StaticMeshFile> m_StaticMeshFiles;
		Dictionary<StaticTextureFile> m_StaticDiffuseTextureFiles;

		Vec2_T<uint32_t> m_RenderResolution{};

		uint64_t m_NextObjectID{};
		VkDescriptorSet m_NullTextureDescriptorSet{};
		DynamicArray<Obstacle> m_Obstacles{};
		DynamicArray<Ground> m_Grounds{};
		Vec2_T<uint32_t> m_ChunkMatrixSize{};
		DynamicArray<Chunk> m_ChunkMatrix{};
		Rect<float> m_WorldRect{};
		DynamicArray<Creature> m_Creatures{};
		uint64_t m_CameraFollowObjectID = UINT64_MAX;
		CameraMatricesBuffer* m_CameraMatricesMap = nullptr;

		Vec2_T<float> m_ChunkDimensions{};

		VkFormat m_ColorImageResourcesFormat{};

		DynamicArray<VkImageView> m_DiffuseImageViews{};
		DynamicArray<VkImageView> m_PositionAndMetallicImageViews{};
		DynamicArray<VkImageView> m_NormalAndRougnessImageViews{};
		DynamicArray<VkImageView> m_DepthImageViews{};
		Pipelines m_Pipelines{};
		DynamicArray<RenderData> m_RenderDatas{};
		VkDescriptorSet m_CameraMatricesDescriptorSet = VK_NULL_HANDLE;
		DynamicArray<VkDescriptorSet> m_RenderPBRImagesDescriptorSets{};
		UnidirectionalLight m_DirectionalLight;
		MeshData m_StaticQuadMeshDataPBR{};
	
		DynamicArray<DebugRenderData> m_DebugRenderDatas{};

		VkDescriptorSet m_DefaultAlbedoDescriptorSet{};
		const float m_CameraFov = pi / 4.0f;
		const float m_CameraNear = 0.1f;
		const float m_CameraFar = 100.0f;
		DynamicArray<VkImage> m_DiffuseImages{};
		DynamicArray<VkImage> m_PositionAndMetallicImages{};
		DynamicArray<VkImage> m_NormalAndRougnessImages{};
		DynamicArray<VkImage> m_DepthImages{};
		DynamicArray<VkDeviceMemory> m_DiffuseImagesMemory{};
		DynamicArray<VkDeviceMemory> m_PositionAndMetallicImagesMemory{};
		DynamicArray<VkDeviceMemory> m_NormalAndRougnessImagesMemory{};
		DynamicArray<VkDeviceMemory> m_DepthImagesMemory{};
		VkDescriptorPool m_CameraMatricesDescriptorPool = VK_NULL_HANDLE;
		VkDescriptorPool m_RenderPBRImagesDescriptorPool = VK_NULL_HANDLE;
		VkSampler m_ColorResourceImageSampler{};
		Renderer::Buffer m_CameraMatricesBuffer;
		VkDescriptorPool m_DefaultTextureDescriptorPool{};
		StaticTexture m_DefaultAlbedoTexture;
		VkImageView m_DefaultAlbedoImageView = VK_NULL_HANDLE;

		World(Renderer& renderer) 
			: m_Renderer(renderer), m_DirectionalLight(*this, UnidirectionalLight::Type::Directional, { 1024, 1024 }), 
				m_CameraMatricesBuffer(m_Renderer), m_DefaultAlbedoTexture(m_Renderer) {}

		World(const World&) = delete;
		World(World&&) = delete;

		void Initialize(const StaticMesh& quadMesh2D) {

			m_StaticQuadMeshDataPBR = quadMesh2D.GetMeshData();

			m_Pipelines.Initialize(m_Renderer, m_ColorImageResourcesFormat);

			if (!m_CameraMatricesBuffer.Create(sizeof(CameraMatricesBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create camera matrices buffer (function Renderer::Buffer::Create in function World::Initialize)!");
			}

			VkResult vkRes = vkMapMemory(m_Renderer.m_VulkanDevice, m_CameraMatricesBuffer.m_VulkanDeviceMemory, 0, 
				sizeof(CameraMatricesBuffer), 0, (void**)&m_CameraMatricesMap);
			if (vkRes != VK_SUCCESS) {
				CriticalError(ErrorOrigin::Vulkan, 
					"failed to map camera matrices buffer (function vkMapMemory in function World::Initialize)!");
			}

			m_CameraMatricesMap->m_Projection = Mat4::Projection(m_CameraFov, 
				(float)m_Renderer.m_SwapchainExtent.width / m_Renderer.m_SwapchainExtent.height, m_CameraNear, m_CameraFar);
			m_CameraMatricesMap->m_View = Mat4::LookAt({ 0.0f, 0.0f, 0.0f }, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 3.0f));

			VkDescriptorPoolSize camPoolSize {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
			};

			m_CameraMatricesDescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, 1, &camPoolSize);

			if (m_CameraMatricesDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create camera matrices descriptor pool (function Renderer::CreateDescriptorPool in function World::Initialize)");
			}

			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_CameraMatricesDescriptorPool, 1, 
					&m_Pipelines.m_CameraDescriptorSetLayout, &m_CameraMatricesDescriptorSet)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate camera matrices descriptor set (function Renderer::AllocateDescriptorSets in function World::Initialize)!");
			}

			VkDescriptorBufferInfo cameraDecriptorBufferInfo {
				.buffer = m_CameraMatricesBuffer.m_Buffer,
				.offset = 0,
				.range = sizeof(CameraMatricesBuffer),
			};

			VkWriteDescriptorSet cameraDescriptorSetWrite = Renderer::GetDescriptorWrite(nullptr, 0, m_CameraMatricesDescriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &cameraDecriptorBufferInfo);

			m_Renderer.UpdateDescriptorSets(1, &cameraDescriptorSetWrite);

			m_DirectionalLight.Initialize(
				Mat4::Orthogonal(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f),
				Mat4::LookAt(Vec3(10.0f, 10.0f, 2.0f), Vec3::Up(), Vec3(0.0f, 0.0f, 0.0f)),
				Vec3(201.0f / 255.0f, 226.0f / 255.0f, 255.0f / 255.0f));

			static constexpr uint32_t default_texture_count = 1;

			static constexpr VkDescriptorPoolSize default_textures_pool_sizes[default_texture_count] {
				{
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
				},
			};

			m_DefaultTextureDescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, default_texture_count, default_textures_pool_sizes);

			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_DefaultTextureDescriptorPool,
					1, &m_Pipelines.m_SingleTextureDescriptorSetLayoutPBR, &m_DefaultAlbedoDescriptorSet)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate default texture descriptor sets for world (function Renderer::AllocateDescriptorSets in function World::Initialize)!");
			}

			if (m_DefaultTextureDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create default texture descriptor pool for world (function Renderer::CreateDescriptorPool in function World::Initialize)!");
			}

			static constexpr uint32_t default_albedo_pixel = PackColorRBGA({ 242.0f / 255.0f, 15.0f / 255.0f, 204.0f / 255.0f, 1.0f });
			static constexpr Vec2_T<uint32_t> default_albedo_extent = { 64U, 64U };
			static constexpr size_t default_albedo_pixel_count = default_albedo_extent.x * default_albedo_extent.y;

			uint32_t* const defaultAlbedoImage = (uint32_t*)malloc(sizeof(uint32_t) * default_albedo_pixel_count);

			assert(defaultAlbedoImage);

			for (size_t i = 0; i < default_albedo_pixel_count; i++) {
				defaultAlbedoImage[i] = default_albedo_pixel;
			}

			if (!m_DefaultAlbedoTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, default_albedo_extent, defaultAlbedoImage)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create default albedo texture for world(function Texture::Create in function World::Initialize)!");
			}

			free(defaultAlbedoImage);

			m_DefaultAlbedoImageView = m_DefaultAlbedoTexture.CreateImageView();

			if (m_DefaultAlbedoImageView == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create default albedo image view for world (function Texture::CreateImageView in function World::Initialize)");
			}

			const VkDescriptorImageInfo defaultAlbedoImageInfo {
				.sampler = m_ColorResourceImageSampler,
				.imageView = m_DefaultAlbedoImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};

			const VkWriteDescriptorSet defaultAlbedoDescriptorWrite = Renderer::GetDescriptorWrite(nullptr, 0, m_DefaultAlbedoDescriptorSet, 
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &defaultAlbedoImageInfo, nullptr);

			m_Renderer.UpdateDescriptorSets(1, &defaultAlbedoDescriptorWrite);
		}

		void Terminate() {
			m_CameraMatricesBuffer.Terminate();
			m_Renderer.DestroyDescriptorPool(m_CameraMatricesDescriptorPool);
			m_Renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
			m_Renderer.DestroyDescriptorPool(m_DefaultTextureDescriptorPool);
			m_Renderer.DestroyImageView(m_DefaultAlbedoImageView);
			m_DefaultAlbedoTexture.Terminate();
			m_Renderer.DestroySampler(m_ColorResourceImageSampler);
			m_Pipelines.Terminate(m_Renderer);
			DestroyImageResources();
			m_DirectionalLight.Terminate();
		}

		void DestroyImageResources() {
			for (size_t i = 0; i < m_DepthImages.Size(); i++) {
				m_Renderer.DestroyImageView(m_DepthImageViews[i]);
				m_Renderer.DestroyImageView(m_DiffuseImageViews[i]);
				m_Renderer.DestroyImageView(m_PositionAndMetallicImageViews[i]);
				m_Renderer.DestroyImageView(m_NormalAndRougnessImageViews[i]);
				m_Renderer.DestroyImage(m_DepthImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
				m_Renderer.DestroyImage(m_DiffuseImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_DiffuseImagesMemory[i]);
				m_Renderer.DestroyImage(m_PositionAndMetallicImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_PositionAndMetallicImagesMemory[i]);
				m_Renderer.DestroyImage(m_NormalAndRougnessImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_NormalAndRougnessImagesMemory[i]);
			}
		}

		void Load(Vec2_T<uint32_t> worldDimensions, Vec2_T<uint32_t> chunkMatrixSize, 
				uint32_t groundCount, Ground::CreateInfo groundInfos[], uint32_t obstacleCount, Obstacle::CreateInfo obstacleInfos[]) {
			m_Grounds.Reserve(groundCount);
			for (uint32_t i = 0; i < groundCount; i++) {
				m_Grounds.EmplaceBack(m_NextObjectID++, groundInfos[i]);
			}
			m_Obstacles.Reserve(obstacleCount);
			for (uint32_t i = 0; i < obstacleCount; i++) {
				m_Obstacles.EmplaceBack(m_NextObjectID++, obstacleInfos[i]);
			}
			m_ChunkDimensions = { 
				(float)worldDimensions.x / chunkMatrixSize.x, 
				(float)worldDimensions.y / chunkMatrixSize.y,
			};
			m_ChunkMatrixSize = chunkMatrixSize;
			m_ChunkMatrix.Reserve(m_ChunkMatrixSize.x * m_ChunkMatrixSize.y);
			m_WorldRect.m_Max = Vec2(worldDimensions.x / 2.0f, worldDimensions.y / 2.0f);
			m_WorldRect.m_Min = -m_WorldRect.m_Max;
			for (size_t x = 0; x < m_ChunkMatrixSize.x; x++) {
				for (size_t y = 0; y < m_ChunkMatrixSize.y; y++) {
					Chunk& chunk = m_ChunkMatrix.EmplaceBack(Vec2_T<uint32_t>(x, y), 
							Vec2(m_WorldRect.m_Min.x + x * m_ChunkDimensions.x, 
								m_WorldRect.m_Min.y + y * m_ChunkDimensions.y), 
								m_ChunkDimensions);
					for (Ground& ground : m_Grounds) {
						if (chunk.m_BoundingRect.OverLaps(ground.m_TopViewBoundingRect)) {
							chunk.m_Grounds.EmplaceBack(ground);
						}
					}
					for (Obstacle& obstacle : m_Obstacles) {
						chunk.m_Obstacles.EmplaceBack(obstacle);
					}
				}
			}
		}

		int ParseStaticMeshFile(FILE* fileStream, StaticMeshFile** outFile, Mat4& outTransform) {
			assert(outFile);
			char delimiters[2] { '\n', ' ' };
			if (FileHandler::Skip(fileStream, 2, delimiters) == EOF) {
				return EOF;
			}
			String string{};
			FileHandler::GetLine(fileStream, string);
			*outFile = m_StaticMeshFiles.Find(string.CString());
			if (!outFile) {
				*outFile = m_StaticMeshFiles.Emplace(string.CString(), m_Renderer);
			}
			if (fscanf(fileStream, "%f%f%f%f %f%f%f%f %f%f%f%f %f%f%f%f",
					&outTransform[0][0], &outTransform[0][1], &outTransform[0][2], &outTransform[0][3],
					&outTransform[1][0], &outTransform[1][1], &outTransform[1][2], &outTransform[1][3],
					&outTransform[2][0], &outTransform[2][1], &outTransform[2][2], &outTransform[2][3],
					&outTransform[3][0], &outTransform[3][1], &outTransform[3][2], &outTransform[3][3])
				!= 16) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to parse static mesh transform (function fscanf in function World::ParseStaticMeshFile)!");
				return 1;
			}
			return 0;
		}

		bool LoadObstacle(FILE* fileStream) {
			Obstacle::CreateInfo createInfo{};
			int res = fscanf(fileStream, "{%f%f%f%f\n",
				&createInfo.m_Position.x, &createInfo.m_Position.y, &createInfo.m_Position.z,
				&createInfo.m_YRotation);
			if (res != 4) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to parse obstacle (function fscanf in function World::LoadObstacle)!");
				return false;
			}
			if (!Collider::CreateInfo::FromFile(fileStream, createInfo.m_ColliderInfo)) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to parse collider for obstacle (in function Collider::CreateInfo::FromFile in function World::LoadObstacle)!");
				return false;
			}
			Obstacle& obstacle = m_Obstacles.EmplaceBack(m_NextObjectID++, createInfo);
			char c = fgetc(fileStream);
			for (; c != '}'; c = fgetc(fileStream)) {
				if (c == EOF) {
					PrintError(ErrorOrigin::FileParsing, 
						"missing '}' when parsing obstacle (in function World::LoadObstacle)!");
					return false;
				}
				if (c == ' ' || c == '\n') {
					continue;
				}
				if (c == '{') {
					constexpr size_t delimiter_count = 2;
					constexpr char delimiters[delimiter_count] { '\n', ' ' };
					for (; c != '}'; c = fgetc(fileStream)) {
						if (c == EOF) {
							PrintError(ErrorOrigin::FileParsing, 
								"missing '}' when parsing obstacle (in function World::LoadObstacle)!");
							return false;
						}
						if (FileHandler::Skip(fileStream, delimiter_count, delimiters) == EOF) {
							PrintError(ErrorOrigin::FileParsing, 
								"missing '}' when parsing obstacle (in function World::LoadObstacle)!");
							return false;
						}
						char buf[3];
						buf[0] = fgetc(fileStream);
						buf[1] = fgetc(fileStream);
						buf[2] = '\0';
						if (!strcmp(buf, "SM")) {
							StaticMeshFile* sm;
							Mat4 transform;
							res = ParseStaticMeshFile(fileStream, &sm, transform);
							if (res == EOF) {
								PrintError(ErrorOrigin::FileParsing, 
									"missing '}' when parsing obstacle (in function World::LoadObstacle)!");
								return false;
							}
							else if (res) {
								PrintError(ErrorOrigin::FileParsing, 
									"failed to parse static mesh file (in function World::LoadObstacle)!");
								return false;
							}
							assert(sm);
							sm->m_Obstacles.PushBack({ obstacle, transform });
						}
					}	
				}
			}
			return true;
		}

		bool Load(FILE* fileStream) {
			if (!fileStream) {
				PrintError(ErrorOrigin::FileParsing, 
					"attempting to load world with file stream that's null!");
				return false;
			}
			m_NextObjectID = 0;
			uint32_t staticMeshCount;
			Vec2_T<uint32_t> worldDimensions;
			Vec2_T<uint32_t>& chunkMatrixSize = m_ChunkMatrixSize;
			int res = 
				fscanf(fileStream, "%u%u%u%u%u", 
					&staticMeshCount,
					&worldDimensions.x, &worldDimensions.y,
					&chunkMatrixSize.x, &chunkMatrixSize.y
				);
			if (res != 5) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to load world due to parsing error (function World::Load)!");
				return false;
			}
			while (true) {
				char c = fgetc(fileStream);
				while (c != '\n' && c != EOF) {
					char c = fgetc(fileStream);
				}
				if (c == EOF) {
					break;
				}
				c = fgetc(fileStream);
				switch (c) {
					case 'O':
						LoadObstacle(fileStream);
						break;
					case 'G':
						break;
					default:
						continue;
				}
			}
			for (auto tuple : m_StaticMeshFiles) {
				MeshFileType fileType = GetMeshFileType(tuple.first);
				StaticMeshFile& meshFile = tuple.second;
				switch (fileType) {
					case MeshFileType::Unrecognized:
						PrintError(ErrorOrigin::FileParsing, 
							"found unrecognized mesh file type when parsing world file (function GetMeshFileType in function World::Load)!");
						break;
					case MeshFileType::Obj:
						FILE* fileStream = fopen(tuple.first.CString(), "r");
						if (!fileStream) {
							PrintError(ErrorOrigin::FileParsing, 
								"failed to open mesh file when parsing world file (function GetQuadMesh in function World::Load)!");
							continue;
						}
						Obj obj{};
						if (!obj.Load(fileStream)) {
							fclose(fileStream);
							PrintError(ErrorOrigin::FileParsing, 
								"failed to load obj file (function Obj::Load in function World::Load)!");
							continue;
						}
						fclose(fileStream);
						DynamicArray<Vertex> vertices{};
						DynamicArray<uint32_t> indices{};
						if (!obj.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, vertices, indices)) {
							PrintError(ErrorOrigin::Engine, 
								"failed to construct mesh from obj file (function Obj::GetMesh in function World::Load)!");
						}
						if (!meshFile.m_Mesh.CreateBuffers(vertices.Size(), vertices.Data(), indices.Size(), indices.Data())) {
							PrintError(ErrorOrigin::Engine, 
								"failed to create mesh (function StaticMesh::CreateBuffers in function World::Load)!");
						}
						MeshData meshData = meshFile.m_Mesh.GetMeshData();
						for (const auto& tuple : meshFile.m_Obstacles) {
							AddRenderData(*tuple.first, tuple.second, meshData);
						}
						for (const auto& tuple : meshFile.m_Grounds) {
							AddRenderData(*tuple.first, tuple.second, meshData);
						}
						break;
				}
			}
			return true;
		}

		void Unload() {
			m_Grounds.Clear();
			m_ChunkMatrix.Clear();
			m_Creatures.Clear();
			m_RenderDatas.Clear();
		};

	public:

		Vec2_T<bool> IsOnChunkBorder(const Vec3& position) {
			Vec2 dimensionsHalf = m_WorldRect.Dimensions() / 2;
			Vec2 pos = Vec2(position.x, position.z) + dimensionsHalf;
			Vec2 frac(pos.x / m_ChunkDimensions.x, pos.y / m_ChunkDimensions.y);
			IntVec2 intgr = frac;
			return { frac.x == (float)intgr.x, frac.y == (float)intgr.y };
		}

		Creature& AddCreature(const Vec3& position, const Collider::CreateInfo& colliderInfo) {
			if (!m_ChunkMatrix.Size()) {
				CriticalError(ErrorOrigin::GameLogic,
					"attempting to add a creature to an empty world (in function World::AddCreature)!");
			}
			Vec3 pos(Clamp(position.x, m_WorldRect.m_Min.x + 0.01f, m_WorldRect.m_Max.x - 0.01f), 0.0f,
				Clamp(position.z, m_WorldRect.m_Min.y + 0.01f, m_WorldRect.m_Max.y - 0.01f));
			Vec2_T<bool> isOnBorder = IsOnChunkBorder(position);
			if (isOnBorder.x) {
				pos.x += 0.01f;
			}
			if (isOnBorder.y) {
				pos.z += 0.01f;
			}
			for (const Chunk& chunk : m_ChunkMatrix) {
				if (chunk.IsPointInside(pos)) {
					return m_Creatures.EmplaceBack(m_NextObjectID++, pos, &chunk, colliderInfo);
				}
			}
			assert(false);
		}

		void SetCameraFollowCreature(const Creature& creature) {
			m_CameraFollowObjectID = creature.m_ObjectID;
		}

		bool CreateTextureMap(const StaticTexture& texture, TextureMap& out) const {
			out.m_ImageView = texture.CreateImageView();
			if (out.m_ImageView == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Renderer, 
					"failed to create image view for texture map (function Texture::CreateImageView in function World::CreateTextureMap)!");
				return false;
			}
			static constexpr VkDescriptorPoolSize poolSize {
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
			};
			out.m_DescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, 1, &poolSize);
			if (out.m_DescriptorPool == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Renderer,
					"failed to create descriptor pool for texture map (function Renderer::CreateDescriptorPool in function World::CreateTextureMap)!");
				return false;
			}
			if (!m_Renderer.AllocateDescriptorSets(nullptr, out.m_DescriptorPool, 1, 
					&m_Pipelines.m_SingleTextureDescriptorSetLayoutPBR, &out.m_DescriptorSet)) {
				PrintError(ErrorOrigin::Renderer,
					"failed to allocate descriptor set for texture map (function Renderer::AllocateDescriptorSets in function World::CreateTextureMap)!");
				return false;
			}
			const VkDescriptorImageInfo descriptorInfo {
				.sampler = m_ColorResourceImageSampler,
				.imageView = out.m_ImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			VkWriteDescriptorSet write = Renderer::GetDescriptorWrite(nullptr, 0, out.m_DescriptorSet, 
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorInfo, nullptr);
			m_Renderer.UpdateDescriptorSets(1, &write);
			return true;
		}

		void DestroyTextureMap(const TextureMap& map) {
			m_Renderer.DestroyDescriptorPool(map.m_DescriptorPool);
			m_Renderer.DestroyImageView(map.m_ImageView);
		}

		PersistentReference<RenderData> AddRenderData(const Creature& creature, const Mat4& transform, const MeshData& meshData) {
			return m_RenderDatas.EmplaceBack(creature.m_ObjectID, transform, meshData);
		}

		PersistentReference<RenderData> AddRenderData(const Ground& ground, const Mat4& transform, const MeshData& meshData) {
			return m_RenderDatas.EmplaceBack(ground.m_ObjectID, transform, meshData);
		}
		
		PersistentReference<RenderData> AddRenderData(const Obstacle& obstacle, const Mat4& transform, const MeshData& meshData) {
			return m_RenderDatas.EmplaceBack(obstacle.m_ObjectID, transform, meshData);
		}

		PersistentReference<DebugRenderData> AddDebugRenderData(const Obstacle& obstacle, const Mat4& transform, 
				const Vec4& wireColor, const MeshData& meshData) {
			return m_DebugRenderDatas.EmplaceBack(obstacle.m_ObjectID, transform, wireColor, meshData);
		}

		const DynamicArray<Ground>& GetGrounds() {
			return m_Grounds;
		}
		
		const DynamicArray<Obstacle>& GetObstacles() {
			return m_Obstacles;
		}

		bool RemoveCreature(Creature& creature) {
			RemoveRenderDatas(creature.m_ObjectID);
			if (m_CameraFollowObjectID == creature.m_ObjectID) {
					m_CameraFollowObjectID = UINT64_MAX;
			}
			return m_Creatures.Erase(&creature);
		}

	private:

		void SwapchainCreateCallback(VkExtent2D swapchainExtent, Vec2_T<uint32_t> renderResolution, float aspectRatio, uint32_t imageCount) {

			m_RenderResolution = renderResolution;

			if (m_CameraMatricesMap) {
				m_CameraMatricesMap->m_Projection 
					= Mat4::Projection(m_CameraFov, aspectRatio, m_CameraNear, m_CameraFar);
			}

			if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
				VkFormat colorImageResourcesFormatCandidates[2] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_B8G8R8A8_SRGB };
				m_ColorImageResourcesFormat = m_Renderer.FindSupportedFormat(1, colorImageResourcesFormatCandidates, 
					VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
				if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
					CriticalError(ErrorOrigin::Renderer, 
						"couldn't find suitable format for color image resources (function Renderer::FindSupportedFormat in function World::SwapchainCreateCallback)!");
				}
			}

			static constexpr uint32_t descriptor_count = 3;

			if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {

				VkDescriptorSetLayoutBinding imageSamplerDescriptorSetBinding {
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.pImmutableSamplers = nullptr,
				};

				VkDescriptorSetLayoutBinding pbrRenderPipelineDescriptorSetBindings[descriptor_count] {
					imageSamplerDescriptorSetBinding,
					imageSamplerDescriptorSetBinding,
					imageSamplerDescriptorSetBinding,
				};

				for (uint32_t i = 1; i < descriptor_count; i++) {
					pbrRenderPipelineDescriptorSetBindings[i].binding = i;
				}

				m_Pipelines.m_RenderPBRImagesDescriptorSetLayout 
					= m_Renderer.CreateDescriptorSetLayout(nullptr, descriptor_count, pbrRenderPipelineDescriptorSetBindings);

				if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create pbr render pipeline samplers descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::SwapchainCreateCallback)!");
				}
			}

			if (m_ColorResourceImageSampler == VK_NULL_HANDLE) {
				m_ColorResourceImageSampler = m_Renderer.CreateSampler(m_Renderer.GetDefaultSamplerInfo());
				if (m_ColorResourceImageSampler == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create color resource image sampler for world (function Renderer::CreateSampler in function World::SwapchainCreateCallback)!");
				}
			}

			if (m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout == VK_NULL_HANDLE) {
				
				static constexpr VkDescriptorSetLayoutBinding dir_light_shadow_map_descriptor_set_layout_bindings[2] {
					Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
					Renderer::GetDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
				};

				m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout 
					= m_Renderer.CreateDescriptorSetLayout(nullptr, 2, dir_light_shadow_map_descriptor_set_layout_bindings);

				if (m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create directional light descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::Initialize)!");
				}
			}

			m_Renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
			VkDescriptorPoolSize poolSize {
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
			};
			DynamicArray<VkDescriptorPoolSize> poolSizes(descriptor_count * imageCount);
			for (VkDescriptorPoolSize& size : poolSizes) {
				size = poolSize;
			}
			m_RenderPBRImagesDescriptorPool = m_Renderer.CreateDescriptorPool(0, imageCount, poolSizes.Size(), poolSizes.Data());

			if (m_RenderPBRImagesDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create pbr render pipeline image descriptor pool (function Renderer::CreateDescriptorPool in function World::SwapchainCreateCallback)!");
			}

			m_RenderPBRImagesDescriptorSets.Resize(imageCount);

			DynamicArray<VkDescriptorSetLayout> setLayouts(imageCount);

			for (VkDescriptorSetLayout& set : setLayouts) {
				set = m_Pipelines.m_RenderPBRImagesDescriptorSetLayout;
			}

			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_RenderPBRImagesDescriptorPool, imageCount, 
					setLayouts.Data(), m_RenderPBRImagesDescriptorSets.Data())) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate pbr rendering pipeline image descriptor sets (function Renderer::AllocateDescriptorSets in function World::SwapchainCreateCallback)!");
			}

			DestroyImageResources();
			m_DiffuseImageViews.Resize(imageCount);
			m_PositionAndMetallicImageViews.Resize(imageCount);
			m_NormalAndRougnessImageViews.Resize(imageCount);
			m_DepthImageViews.Resize(imageCount);
			m_DiffuseImages.Resize(imageCount);
			m_PositionAndMetallicImages.Resize(imageCount);
			m_NormalAndRougnessImages.Resize(imageCount);
			m_DepthImages.Resize(imageCount);
			m_DiffuseImagesMemory.Resize(imageCount);
			m_PositionAndMetallicImagesMemory.Resize(imageCount);
			m_NormalAndRougnessImagesMemory.Resize(imageCount);
			m_DepthImagesMemory.Resize(imageCount);

			LockGuard graphicsQueueLockGuard(m_Renderer.m_EarlyGraphicsCommandBufferQueueMutex);
			Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
				= m_Renderer.m_EarlyGraphicsCommandBufferQueue.New();
			if (!commandBuffer) {
				CriticalError(ErrorOrigin::Renderer,
					"renderer graphics command buffer was out of memory (in function World::SwapchainCreateCallback)!");
			}
			if (!m_Renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(
					m_Renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
					&commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function World::SwapchainCreateCallback)");
			}
			if (!m_Renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to begin command buffer (function Renderer::BeginCommandBuffer in function World::SwapchainCreateCallback)");
			}

			VkFormat depthFormat = m_Renderer.m_DepthOnlyFormat;
			VkExtent3D imageExtent {
				.width = m_RenderResolution.x,
				.height = m_RenderResolution.y,
				.depth = 1,
			};
			uint32_t colorImageQueueFamilies[1] { m_Renderer.m_GraphicsQueueFamilyIndex, };
			uint32_t colorImageQueueFamilyCount = 1;
			VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; 
			VkSharingMode colorImageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

			for (uint32_t i = 0; i < imageCount; i++) {
				{
					VkImage& image = m_DiffuseImages[i];
					VkDeviceMemory& imageMemory = m_DiffuseImagesMemory[i];
					VkImageView& imageView = m_DiffuseImageViews[i];
					image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
						VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
						colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
					if (image == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world diffuse image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
					}
					imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					if (imageMemory == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate world diffuse image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
					}
					imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
					if (imageView == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world diffuse image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
					}
				}
				{
					VkImage& image = m_PositionAndMetallicImages[i];
					VkDeviceMemory& imageMemory = m_PositionAndMetallicImagesMemory[i];
					VkImageView& imageView = m_PositionAndMetallicImageViews[i];
					image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
						VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
						colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
					if (image == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create position/metallic image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
					}
					imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					if (imageMemory == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate position/metallic image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)");
					}
					imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
					if (imageView == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world position/metallic image view (function Renderer::CreateImageView in function World::Initialize)");
					}
				}
				{
					VkImage& image = m_NormalAndRougnessImages[i];
					VkDeviceMemory& imageMemory = m_NormalAndRougnessImagesMemory[i];
					VkImageView& imageView = m_NormalAndRougnessImageViews[i];
					image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
						VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
						colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
					if (image == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create normal/roughness image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
					}
					imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					if (imageMemory == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate normal/roughness image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
					}
					imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
					if (imageView == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world normal/roughness image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
					}
				}
				{
					VkImage& image = m_DepthImages[i];
					VkDeviceMemory& imageMemory = m_DepthImagesMemory[i];
					VkImageView& imageView = m_DepthImageViews[i];
					image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1, 
						VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
							VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &m_Renderer.m_GraphicsQueueFamilyIndex);
					if (image == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world depth image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
					}
					imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					if (imageMemory == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate world depth image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)!");
					}
					imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
					if (imageView == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create world depth image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)!");
					}
				}

				VkDescriptorImageInfo descriptorImageInfos[descriptor_count] {
					{
						.sampler = m_ColorResourceImageSampler,
						.imageView = m_DiffuseImageViews[i],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					},
					{
						.sampler = m_ColorResourceImageSampler,
						.imageView = m_PositionAndMetallicImageViews[i],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					},
					{
						.sampler = m_ColorResourceImageSampler,
						.imageView = m_NormalAndRougnessImageViews[i],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					},
				};

				VkWriteDescriptorSet descriptorWrites[descriptor_count];
				for (uint32_t j = 0; j < descriptor_count; j++) {
					descriptorWrites[j] 
						= Renderer::GetDescriptorWrite(nullptr, j, m_RenderPBRImagesDescriptorSets[i], 
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[j], nullptr);
				}

				m_Renderer.UpdateDescriptorSets(descriptor_count, descriptorWrites);

				static constexpr uint32_t image_count = descriptor_count;

				VkImage colorImages[image_count] {
					m_DiffuseImages[i],
					m_PositionAndMetallicImages[i],
					m_NormalAndRougnessImages[i],	
				};	

				VkImageMemoryBarrier memoryBarriers[image_count];

				for (size_t j = 0; j < image_count; j++) {
					memoryBarriers[j] = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = 0,
						.dstAccessMask = 0,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = colorImages[j],
						.subresourceRange {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},
					};
				}
				vkCmdPipelineBarrier(commandBuffer->m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, image_count, memoryBarriers);
			}
			m_DirectionalLight.SwapchainCreateCallback(imageCount, commandBuffer->m_CommandBuffer);
			VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
			if (vkRes != VK_SUCCESS) {
				CriticalError(ErrorOrigin::Vulkan, 
					"failed to end command buffer (function vkEndCommandBuffer in function World::SwapchainCreateCallback)!", 
					vkRes);
			}
			commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;
		}

		void LogicUpdate() {
			for (Creature& creature : m_Creatures) {
				Vec3 movementVector = creature.GetMovementVector();
				if (movementVector == Vec3(0.0f, movementVector.y, 0.0f)) {
					continue;
				}
				Vec3 newPos = creature.m_Position + movementVector;
				const Chunk* curChunk = creature.m_Chunk;
				assert(curChunk);
				if (!curChunk->IsPointInside(newPos)) {
					Vec2_T<uint32_t> newChunkMatrixCoords {
						curChunk->m_ChunkMatrixCoords.x + (newPos.x >= curChunk->m_BoundingRect.m_Max.x ? 1
							: newPos.x <= curChunk->m_BoundingRect.m_Min.x ? -1 : 0),

						curChunk->m_ChunkMatrixCoords.y + (newPos.z >= curChunk->m_BoundingRect.m_Max.y ? 1
							: newPos.z <= curChunk->m_BoundingRect.m_Min.y ? -1 : 0),
					};
					Vec2_T<bool> outsideBounds = BoundsCheck(newChunkMatrixCoords);
					if (outsideBounds.x && outsideBounds.y) {
						continue;
					}
					else if (outsideBounds.x) {
						newPos.x -= movementVector.x;
						newChunkMatrixCoords.x = curChunk->m_ChunkMatrixCoords.x;
					}
					else if (outsideBounds.y) {
						newPos.z -= movementVector.z;
						newChunkMatrixCoords.y = curChunk->m_ChunkMatrixCoords.y;
					}
					const Chunk* newChunk = GetChunk(newChunkMatrixCoords);
					if (newChunk != creature.m_Chunk) {
						creature.m_Chunk = newChunk;
						assert(creature.m_Chunk);
						if (creature.m_Chunk->m_BoundingRect.m_Min.x == newPos.x) {
							newPos.x += 0.01f;
						}
						else if (creature.m_Chunk->m_BoundingRect.m_Max.x == newPos.x) {
							newPos.x -= 0.01f;
						}
						if (creature.m_Chunk->m_BoundingRect.m_Min.y == newPos.z) {
							newPos.z += 0.01f;
						}
						else if (creature.m_Chunk->m_BoundingRect.m_Max.y == newPos.z) {
							newPos.z -= 0.01f;
						}
						fmt::print("chunk change to coords ({}, {})\n",
							creature.m_Chunk->m_ChunkMatrixCoords.x, creature.m_Chunk->m_ChunkMatrixCoords.y);
					}
				}
				creature.Move(newPos);
				if (m_CameraFollowObjectID == creature.m_ObjectID) {
					if (creature.m_CameraFollowCallback) {
						Vec3 eye;
						Vec3 lookAt;
						creature.m_CameraFollowCallback(creature, eye, lookAt);
						m_CameraMatricesMap->m_View = Mat4::LookAt(eye, Vec3::Up(), lookAt);
					}
				}
			}
		}

		void SetViewportToRenderResolution(const Renderer::DrawData& drawData) const {
			const VkRect2D scissor {
				.offset { 0, 0 },
				.extent { m_RenderResolution.x, m_RenderResolution.y },
			};
			vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
			const VkViewport viewport {
				.x = 0,
				.y = 0,
				.width = (float)m_RenderResolution.x,
				.height = (float)m_RenderResolution.y,
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);
		}

		void RenderWorld(const Renderer::DrawData& drawData) const {
			{

				SetViewportToRenderResolution(drawData);

				static constexpr uint32_t color_attachment_count = 3;

				VkRenderingAttachmentInfo colorAttachments[color_attachment_count] {
					{
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
						.pNext = nullptr,
						.imageView = m_DiffuseImageViews[drawData.m_CurrentFrame],
						.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.resolveMode = VK_RESOLVE_MODE_NONE,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.clearValue { .color { .uint32 { 0, 0, 0, 0 } } },
					},
					{
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
						.pNext = nullptr,
						.imageView = m_PositionAndMetallicImageViews[drawData.m_CurrentFrame],
						.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.resolveMode = VK_RESOLVE_MODE_NONE,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.clearValue { .color { .uint32 { 0, 0, 0, 0 } } },
					},
					{
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
						.pNext = nullptr,
						.imageView = m_NormalAndRougnessImageViews[drawData.m_CurrentFrame],
						.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.resolveMode = VK_RESOLVE_MODE_NONE,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.clearValue { .color { .uint32 { 0, 0, 0, 0 } } },
					},
				};

				VkRenderingAttachmentInfo depthAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = m_DepthImageViews[drawData.m_CurrentFrame],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { .depthStencil { .depth = 1.0f, .stencil = 0 } },
				};

				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea { .offset {}, .extent { m_RenderResolution.x, m_RenderResolution.y } },
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = color_attachment_count,
					.pColorAttachments = colorAttachments,
					.pDepthAttachment = &depthAttachment,
				};

				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipelinePBR);
				VkDescriptorSet descriptorSets[2] {
					m_CameraMatricesDescriptorSet,
					VK_NULL_HANDLE,
				};
				for (const RenderData& data : m_RenderDatas) {
					if (data.m_AlbedoTextureDescriptorSet == VK_NULL_HANDLE) {
						descriptorSets[1] = m_DefaultAlbedoDescriptorSet;
					}
					else {
						descriptorSets[1] = data.m_AlbedoTextureDescriptorSet;
					}
					vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipelineLayoutPBR, 
						0, 2, descriptorSets, 0, nullptr);;
					Mat4 matrices[2] {
						data.m_Transform,
						Transpose(Inverse(data.m_Transform)),
					};
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DrawPipelineLayoutPBR,
						VK_SHADER_STAGE_VERTEX_BIT, 0, 128, &matrices);
					vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, data.m_MeshData.m_VertexBuffers, 
						data.m_MeshData.m_VertexBufferOffsets);
					vkCmdBindIndexBuffer(drawData.m_CommandBuffer, data.m_MeshData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(drawData.m_CommandBuffer, data.m_MeshData.m_IndexCount, 1, 0, 0, 0);
				}
				vkCmdEndRendering(drawData.m_CommandBuffer);

				m_Renderer.SetViewportToSwapchainExtent(drawData);
			}

			m_DirectionalLight.DepthDraw(drawData);

			{
				VkRenderingAttachmentInfo colorAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = drawData.m_SwapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = { .color { .uint32 { 0, 0, 0, 0 } } },
				};

				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea = { .offset {}, .extent { m_Renderer.m_SwapchainExtent } },
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment,
					.pDepthAttachment = nullptr,
				};

				static constexpr uint32_t image_count = 3;

				VkImage images[image_count] {
					m_DiffuseImages[drawData.m_CurrentFrame],
					m_PositionAndMetallicImages[drawData.m_CurrentFrame],
					m_NormalAndRougnessImages[drawData.m_CurrentFrame],
				};

				VkImageMemoryBarrier memoryBarriers[image_count]{};

				for (size_t i = 0; i < image_count; i++) {
					memoryBarriers[i] = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = 0,
						.dstAccessMask = 0,
						.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = images[i],
						.subresourceRange {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},
					};
				}

				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
					0, 0, nullptr, 0, nullptr, image_count, memoryBarriers);

				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_RenderPipelinePBR);
				VkDescriptorSet descriptorSets[2] {
					m_RenderPBRImagesDescriptorSets[drawData.m_CurrentFrame],
					m_DirectionalLight.m_ShadowMapDescriptorSets[drawData.m_CurrentFrame],
				};
				vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_RenderPipelineLayoutPBR, 0, 
					2, descriptorSets, 0, nullptr);
				vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, m_StaticQuadMeshDataPBR.m_VertexBufferCount, 
					m_StaticQuadMeshDataPBR.m_VertexBuffers, m_StaticQuadMeshDataPBR.m_VertexBufferOffsets);
				vkCmdBindIndexBuffer(drawData.m_CommandBuffer, m_StaticQuadMeshDataPBR.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawData.m_CommandBuffer, m_StaticQuadMeshDataPBR.m_IndexCount, 1, 0, 0, 0);
				vkCmdEndRendering(drawData.m_CommandBuffer);

				for (size_t i = 0; i < image_count; i++) {
					memoryBarriers[i] = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = 0,
						.dstAccessMask = 0,
						.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = images[i],
						.subresourceRange {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},
					};
				}

				vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
					0, 0, nullptr, 0, nullptr, image_count, memoryBarriers);
			}
		}

		void Render(const Renderer::DrawData& drawData) const {
			RenderWorld(drawData);
			/*
			if (m_DebugRenderDatas.m_Size) {
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DebugPipeline);
				vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DebugPipelineLayout,
					0, 1, &m_CameraMatricesDescriptorSet, 0, nullptr);
				for (const DebugRenderData& data : m_DebugRenderDatas) {
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &data.m_Transform);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16, &data.m_WireColor);
					vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, data.m_MeshData.m_VertexBuffers, 
						data.m_MeshData.m_VertexBufferOffsets);
					vkCmdBindIndexBuffer(drawData.m_CommandBuffer, data.m_MeshData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(drawData.m_CommandBuffer, data.m_MeshData.m_IndexCount, 1, 0, 0, 0);
				}

			}
			*/
		}

		void RemoveRenderDatas(uint64_t objectID) {
			auto end = m_RenderDatas.end();
			for (auto iter = m_RenderDatas.begin(); iter != end;) {
				if (iter->m_ObjectID == objectID) {
					iter = m_RenderDatas.Erase(iter);
					continue;
				}
				++iter;
			}
		}

		Chunk* GetChunk(Vec2_T<uint32_t> chunkMatrixCoords) {
			uint32_t index = chunkMatrixCoords.x * m_ChunkMatrixSize.y + chunkMatrixCoords.y;
			if (index >= m_ChunkMatrix.Size()) {
				return nullptr;
			}
			return &m_ChunkMatrix[index];
		}

		Vec2_T<bool> BoundsCheck(Vec2_T<uint32_t> chunkMatrixCoords) {
			return { chunkMatrixCoords.x >= m_ChunkMatrixSize.x, chunkMatrixCoords.y >= m_ChunkMatrixSize.y };
		}
		};


	enum EngineModeBits {
		EngineMode_Initialized = 1,
		EngineMode_Play = 2,
		EngineMode_Game = 4,
		EngineMode_Editor = 8,
	};

	typedef uint32_t EngineMode;

	class Engine {

		friend void CriticalError(ErrorOrigin origin, const char* err, VkResult vkErr);

	private:
	
		EngineMode m_Mode;

		static constexpr uint32_t sc_RenderResolutionHeight1080p = 400;
		Vec2_T<uint32_t> m_RenderResolution;

		UI m_UI;
		World m_World;
		Renderer m_Renderer;
		TextRenderer m_TextRenderer;

		StaticMesh m_StaticQuadMesh;
		StaticMesh m_StaticQuadMesh2D;

	public:	

		Engine(EngineMode mode, const char* appName, GLFWwindow* window, size_t maxUIWindows) :
				m_Mode(UpdateEngineInstance(this, mode)),
				m_UI(m_Renderer, m_TextRenderer, maxUIWindows),
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, RendererCriticalErrorCallback, SwapchainCreateCallback),
				m_TextRenderer(m_Renderer, TextRendererCriticalErrorCallback),
				m_StaticQuadMesh(m_Renderer),
				m_StaticQuadMesh2D(m_Renderer),
				m_World(m_Renderer)
		{
			Input input(window);

			static constexpr uint32_t quad_vertex_count = 4;

			static constexpr Vertex quad_vertices[quad_vertex_count] {
				{
					.m_Position { -1.0f, 1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, 1.0f },
					.m_UV { 0.0f, 1.0f },
				},
				{
					.m_Position { 1.0f, 1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, 1.0f },
					.m_UV { 1.0f, 1.0f },
				},
				{
					.m_Position { -1.0f, -1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, 1.0f },
					.m_UV { 0.0f, 0.0f },
				},
				{
					.m_Position { 1.0f, -1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, 1.0f },
					.m_UV { 1.0f, 0.0f },
				},	
			};

			static constexpr uint32_t quad_index_count = 6;

			static constexpr uint32_t quad_indices[quad_index_count] {
				3, 2, 0,
				1, 3, 0,
			};

			static constexpr Vertex2D quad_vertices_2D[quad_vertex_count] {
				{
					.m_Position { -1.0f, 1.0f, 0.0f },
					.m_UV { 0.0f, 0.0f },
				},
				{
					.m_Position { 1.0f, 1.0f, 0.0f },
					.m_UV { 1.0f, 0.0f },
				},
				{
					.m_Position { -1.0f, -1.0f, 0.0f },
					.m_UV { 0.0f, 1.0f },
				},
				{
					.m_Position { 1.0f, -1.0f, 0.0f },
					.m_UV { 1.0f, 1.0f },
				},
			};

			if (!m_StaticQuadMesh.CreateBuffers(quad_vertex_count, quad_vertices, quad_index_count, quad_indices)) {
				CriticalError(ErrorOrigin::Engine, 
					"failed to create static 3D quad mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}
			if (!m_StaticQuadMesh2D.CreateBuffers(quad_vertex_count, quad_vertices_2D, quad_index_count, quad_indices)) {
				CriticalError(ErrorOrigin::Engine, 
					"failed to create static 2D quad mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}

			const VkFormat fontAtlasFormatCandidates[1] { VK_FORMAT_R8_SRGB, };
			FontAtlas::s_AtlasFormat = m_Renderer.FindSupportedFormat(1, fontAtlasFormatCandidates,
				VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
			if (FontAtlas::s_AtlasFormat == VK_FORMAT_UNDEFINED) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to find suitable format for font atlas (function Renderer::FindSupportedFormat in Engine constructor)!");
			}

			VkSamplerCreateInfo fontAtlasSamplerInfo = Renderer::GetDefaultSamplerInfo();
			fontAtlasSamplerInfo.unnormalizedCoordinates = VK_TRUE;
			fontAtlasSamplerInfo.minLod = 0.0f;
			fontAtlasSamplerInfo.maxLod = 0.0f;

			FontAtlas::s_Sampler = m_Renderer.CreateSampler(fontAtlasSamplerInfo);

			if (FontAtlas::s_Sampler == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create sampler for font atlas (function Renderer::CreateSampler in Engine constructor)!");
			}

			m_World.Initialize(m_StaticQuadMesh2D);
			m_UI.Initialize(m_StaticQuadMesh2D);
		}

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;

		~Engine() {
			m_World.Terminate();
			m_StaticQuadMesh.Terminate();
			m_StaticQuadMesh2D.Terminate();
			m_UI.Terminate();
			m_Renderer.DestroySampler(FontAtlas::s_Sampler);
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
		}	

		Vec2_T<uint32_t> GetSwapchainResolution() {
			return { m_Renderer.m_SwapchainExtent.width, m_Renderer.m_SwapchainExtent.height };
		}	

	public:

		UI& GetUI() {
			return m_UI;
		}

		Renderer& GetRenderer() {
			return m_Renderer;
		}

		TextRenderer& GetTextRenderer() {
			return m_TextRenderer;
		}

		const StaticMesh& GetQuadMesh() const {
			return m_StaticQuadMesh;
		}

		World& LoadWorld(Vec2_T<uint32_t> worldDimensions, Vec2_T<uint32_t> chunkMatrixSize, 
				uint32_t groundCount, World::Ground::CreateInfo groundInfos[], uint32_t obstacleCount, World::Obstacle::CreateInfo obstacleInfos[]) {
			m_World.Unload();
			m_World.Load(worldDimensions, chunkMatrixSize, groundCount, groundInfos, obstacleCount, obstacleInfos);
			return m_World;
		}

		bool Loop() {

			Time::BeginFrame();

			glfwPollEvents();

			if (m_Mode & EngineMode_Play) {
				m_World.LogicUpdate();
				m_UI.UILoop();
			}

			if (m_Mode & EngineMode_Editor) {
			}

			Renderer::DrawData drawData;
			if (m_Renderer.BeginFrame(drawData)) {
				m_World.RenderWorld(drawData);
				m_UI.RenderUI(drawData);
				m_Renderer.EndFrame(0, nullptr);
			}

			Input::ResetInput();

			bool closing = glfwWindowShouldClose(m_Renderer.m_Window);

			Time::EndFrame();

			return !closing;
		}

	private:

		inline static Engine* s_engine_instance = nullptr;

		static void RendererCriticalErrorCallback(const Renderer* renderer, Renderer::ErrorOrigin origin, const char* err, VkResult vkErr) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"Renderer called a critical error!\nError origin: {}\nError: {}\n", Renderer::ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"Vulkan error code: {}\n", (int)vkErr);
			}
			if (s_engine_instance != nullptr) {
				s_engine_instance->~Engine();
			}
			fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
			glfwTerminate();
#ifdef DEBUG
			assert(false);
#endif
			exit(EXIT_FAILURE);
		}

		static void TextRendererCriticalErrorCallback(const TextRenderer* renderer, 
			TextRenderer::ErrorOrigin origin, const char* err, FT_Error ftErr) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"Text renderer called a critical error!\nError origin: {}\nError: {}\n", TextRenderer::ErrorOriginString(origin), err);
			if (ftErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"FreeType error code: {}\n", ftErr);
			}
			if (s_engine_instance != nullptr) {
				s_engine_instance->~Engine();
			}
			fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
			glfwTerminate();
#ifdef DEBUG
			assert(false);
#endif
			exit(EXIT_FAILURE);
		}

		static void SwapchainCreateCallback(const Renderer& renderer, VkExtent2D swapchainExtent, uint32_t imageCount, VkImageView* imageViews) {
			assert(s_engine_instance);
			float aspectRatio = (float)swapchainExtent.width / swapchainExtent.height;
			uint32_t renderResHeight = sc_RenderResolutionHeight1080p * (float)swapchainExtent.height / 1080;
			s_engine_instance->m_RenderResolution = { (uint32_t)(renderResHeight * aspectRatio), renderResHeight};
			s_engine_instance->m_UI.SwapchainCreateCallback({ swapchainExtent.width, swapchainExtent.height }, aspectRatio, imageCount);
			s_engine_instance->m_World.SwapchainCreateCallback(swapchainExtent, s_engine_instance->m_RenderResolution, aspectRatio, imageCount);
		}

		static inline EngineMode UpdateEngineInstance(Engine* engine, EngineMode mode) {
			if (s_engine_instance) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"attempting to initialize engine twice (only one engine allowed)!");
				exit(EXIT_FAILURE);
			}
			s_engine_instance = engine;
			return mode;
		}

	};

	typedef World::Creature Creature;
}
