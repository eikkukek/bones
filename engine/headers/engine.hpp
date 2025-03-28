#pragma once

#include "renderer.hpp"
#include "text_renderer.hpp"
#include "math.hpp"
#include "builtin_pipelines.hpp"
#include "fmt/color.h"
#include "vulkan/vulkan_core.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#define JPH_DEBUG_RENDERER
#include "Jolt/Jolt.h"
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Renderer/DebugRenderer.h"
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <chrono>

#ifndef JOLT_DEBUG
JPH_SUPPRESS_WARNINGS
#endif

namespace engine {

	class Engine;
	class UI;

	using UID = uint64_t;
	using LockGuard = std::lock_guard<std::mutex>;
	using MeshData = Renderer::MeshData;

	enum EngineStateBits {
		EngineState_Initialized = 1,
		EngineState_Play = 2,
		EngineState_Editor = 4,
		EngineState_EditorView = 8,
		EngineState_Release = 16,
		EngineState_LogicAwake = 32,
		EngineState_QueueLogicShutdown = 64,
		EngineState_Pause = 128,
	};

	typedef uint32_t EngineState;

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
		Engine,
		Renderer,
		TextRenderer,
		UI,
		AssetManager,
		Editor,
		OutOfMemory,
		NullDereference,
		IndexOutOfBounds,
		Vulkan,
		Stb,
		Jolt,
		FileParsing,
		FileWriting,
		GameLogic,
		MaxEnum,
	};

	static const char* ErrorOriginString(ErrorOrigin origin) {
		static const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
			"Uncategorized",
			"Engine",
			"Renderer",
			"TextRenderer",
			"UI",
			"AssetManager",
			"Editor",
			"OutOfMemory",
			"NullDereference",
			"IndexOutOfBounds",
			"Vulkan",
			"STB",
			"Jolt",
			"FileParsing",
			"FileWriting",
			"GameLogic",
		};
		if (origin == ErrorOrigin::MaxEnum) {
			return strings[0];
		}
		return strings[(size_t)origin];
	}

	void CriticalError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS, const char* libErr = nullptr);

	inline void PrintError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS, const char* libErr = nullptr) {
		fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
			"Engine called an error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
		if (vkErr != VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "Vulkan error code: {}\n", (int)vkErr);
		}
		if (libErr) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "Library error: {}\n", libErr);
		}
	}

	inline void PrintWarning(const char* warn) {
		fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, 
			"Engine called a warning:\n {}\n", warn);
	}

	inline void LogError(const char* err) {
		fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
			"error: {}", err);
	}

	template<typename T>
	class Optional {
	private:

		T m_Value;
		bool m_HasValue;

	public:

		Optional() : m_HasValue(false) {}
		Optional(const T& value) : m_Value(value), m_HasValue(true) {}

		bool HasValue() const {
			return m_HasValue;
		}

		T& GetValue() {
			if (!m_HasValue) {
				PrintError(ErrorOrigin::Engine,
					"attempting to get value from optional that doesn't have a value (in function Optional::GetValue)!");
			}
			return m_Value;
		}

		const T& GetValue() const {
			if (!m_HasValue) {
				PrintError(ErrorOrigin::Engine,
					"attempting to get value from optional that doesn't have a value (in function Optional::GetValue)!");
			}
			return m_Value;
		}
	};

	template<typename T, size_t size_T>
	class Array {
	public:

		using Iterator = T*;
		using ConstIterator = const T*;

		T m_Data[size_T]{};

		constexpr size_t Size() const {
			return size_T;
		}

		constexpr T* Data() {
			return m_Data;
		}

		constexpr const T* Data() const {
			return m_Data;
		}

		constexpr T& operator[](size_t index) {
			assert(index < size_T);
			return m_Data[index];
		}

		constexpr const T& operator[](size_t index) const {
			assert(index < size_T);
			return m_Data[index];
		}

		Iterator begin() {
			return m_Data;
		}

		ConstIterator begin() const {
			return m_Data;
		}

		ConstIterator end() const {
			return &m_Data[size_T];
		}
	};

	template<typename T>
	class DynamicArray {
	public:

		using Iterator = T*;
		using ConstIterator = const T*;

	private:

		T* m_Data;
		uint32_t m_Size;
		uint32_t m_Capacity;

	public:

		DynamicArray() : m_Data(nullptr), m_Size(0), m_Capacity(0) {}

		DynamicArray(uint32_t size) : m_Data(nullptr), m_Size(0), m_Capacity(0) {
			Resize(size);
		}

		DynamicArray(const DynamicArray& other) noexcept 
			: m_Data(nullptr), m_Size(0), m_Capacity(0) {
			Reserve(other.m_Capacity);
			for (const T& val : other) {
				PushBack(val);
			}
		}

		DynamicArray& operator=(const DynamicArray& other) noexcept {
			Clear();
			Reserve(other.m_Capacity);
			for (const T& val : other) {
				PushBack(val);
			}
			return *this;
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
			return *this;
		}

		~DynamicArray() {
			for (uint32_t i = 0; i < m_Size; i++) {
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

		DynamicArray& Reserve(uint32_t capacity) {
			if (capacity < m_Capacity) {
				return *this;
			}
			T* temp = (T*)malloc(sizeof(T) * capacity);
			for (uint32_t i = 0; i < m_Size; i++) {
				new(&temp[i]) T(std::move(m_Data[i]));
			}
			free(m_Data);
			m_Capacity = capacity;
			m_Data = temp;
			return *this;
		}

		DynamicArray& Resize(uint32_t size) {
			if (size == m_Size) {
				return *this;
			}
			if (size < m_Size) {
				for (uint32_t i = size; i < m_Size; i++) {
					(&m_Data[i])->~T();
				}
				m_Size = size;
				return *this;
			}
			if (size >= m_Capacity) {
				Reserve(size * 2);
			}
			for (uint32_t i = m_Size; i < size; i++) {
				new(&m_Data[i]) T();
			}
			m_Size = size;
			return *this;
		}

		template<typename... Args>
		DynamicArray& Resize(uint32_t size, Args&&... args) {
			if (size == m_Size) {
				return *this;
			}
			if (size < m_Size) {
				for (uint32_t i = size; i < m_Size; i++) {
					(&m_Data[i])->~T();
				}
				m_Size = size;
				return *this;
			}
			if (size >= m_Capacity) {
				Reserve(size * 2);
			}
			for (uint32_t i = m_Size; i < size; i++) {
				new(&m_Data[i]) T(args...);
			}
			m_Size = size;
		}

		DynamicArray& Clear() {
			for (uint32_t i = 0; i < m_Size; i++) {
				(m_Data + i)->~T();
			}
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
			return m_Data + m_Size;
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

		using Bucket = T[BucketCapacity];

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

	template<typename KeyType, typename ValueType>
	class OrderedDictionary {
	private:

		uint32_t m_Size;
		uint32_t m_Capacity;
		uint8_t* m_Data;

	public:

		using ValueIterator = ValueType*;
		using ConstValueIterator = const ValueType*;

		using KeyIterator = KeyType*;
		using ConstKeyIterator = const KeyType*;

		OrderedDictionary() noexcept 
			: m_Size(0), m_Capacity(0), m_Data(nullptr) {}

		OrderedDictionary(uint32_t capacity) 
			: m_Size(0), m_Capacity(0), m_Data(nullptr) {
			Reserve(capacity);
		}

		OrderedDictionary(const OrderedDictionary& other) 
			: m_Size(0), m_Capacity(0), m_Data(nullptr) {
			if (!other.m_Data) {
				return;
			}
			Reserve(other.m_Capacity);
			const KeyType* otherKeys = other._Keys();
			const ValueType* otherValues = other._Values();
			KeyType* thisKeys = _Keys();
			ValueType* thisValues = _Values();
			m_Size = other.m_Size;
			for (uint32_t i = 0; i < m_Size; i++) {
				thisKeys[i] = otherKeys[i];
				thisValues[i] = otherValues[i];
			}
		}

		OrderedDictionary(OrderedDictionary&& other) 
			: m_Size(other.m_Size), m_Capacity(other.m_Capacity), m_Data(other.m_Data) {
			other.m_Size = 0;
			other.m_Capacity = 0;
			other.m_Data = nullptr;
		}

		OrderedDictionary& operator=(const OrderedDictionary& other) {
			Clear();
			if (!other.m_Data) {
				return *this;
			}
			Reserve(other.m_Capacity);
			const KeyType* otherKeys = other._Keys();
			const ValueType* otherValues = other._Values();
			KeyType* thisKeys = _Keys();
			ValueType* thisValues = _Values();
			m_Size = other.m_Size;
			for (uint32_t i = 0; i < m_Size; i++) {
				thisKeys[i] = otherKeys[i];
				thisValues[i] = otherValues[i];
			}
			return *this;
		}

		OrderedDictionary& operator=(OrderedDictionary&& other) {
			Clear();
			m_Size = other.m_Size;
			m_Capacity = other.m_Capacity;
			m_Data = other.m_Data;
			other.m_Size = 0;
			other.m_Capacity = 0;
			return *this;
		}

		~OrderedDictionary() {
			Clear();
		}

		uint32_t Size() const {
			return m_Size;
		}

		uint32_t Capacity() const {
			return m_Capacity;
		}

		OrderedDictionary& Reserve(uint32_t capacity) {
			if (capacity <= m_Capacity) {
				return *this;
			}
			uint8_t* temp = (uint8_t*)malloc(capacity * sizeof(KeyType) + capacity * sizeof(ValueType));
			assert(temp && "malloc failed!");
			if (m_Data) {
				KeyType* keys = _Keys();
				ValueType* values = _Values();
				KeyType* newKeys = (KeyType*)temp;
				ValueType* newValues = (ValueType*)(temp + capacity * sizeof(KeyType));
				for (uint32_t i = 0; i < m_Size; i++) {
					new(&newKeys[i]) KeyType(std::move(keys[i]));
					new(&newValues[i]) ValueType(std::move(values[i]));
				}
			}	
			free(m_Data);
			m_Data = temp;
			m_Capacity = capacity;
			return *this;
		}

		OrderedDictionary& Clear() {
			if (m_Data) {
				KeyType* keys = _Keys();
				ValueType* values = _Values();
				for (uint32_t i = 0; i < m_Size; i++) {
					(&keys[i])->~KeyType();
					(&values[i])->~ValueType();
				}
				free(m_Data);
			}
			m_Size = 0;
			m_Capacity = 0;
			m_Data = nullptr;
			return* this;
		}

		ValueType* Insert(const KeyType& key, const ValueType& value) {
			if (!m_Data) {
				Reserve(4);
				*_Keys() = key;
				ValueType* values = _Values();
				*values = value;
				++m_Size;
				return values;
			}
			if (m_Capacity == m_Size) {
				Reserve(m_Capacity * 2);
			}
			int64_t index = _FindNewIndex(key);
			if (index == -1) {
				return nullptr;
			}
			return _InsertToIndex(index, key, value);
		}

		template<typename... Args>
		ValueType* Emplace(const KeyType& key, Args&&... args) {
			if (!m_Data) {
				Reserve(4);
				*_Keys() = key;
				ValueType* values = _Values();
				new(values) ValueType(std::forward<Args>(args)...);
				++m_Size;
				return values;
			}
			if (m_Capacity == m_Size) {
				Reserve(m_Capacity * 2);
			}
			int64_t index = _FindNewIndex(key);
			if (index == -1) {
				return nullptr;
			}
			return _EmplaceToIndex(index, key, std::forward<Args>(args)...);
		}

		bool Erase(const KeyType& key) {
			if (!m_Data) {
				return false;
			}
			int64_t index = _FindIndex(key);
			if (index == -1) {
				return false;
			}
			KeyType* keys = _Keys();
			ValueType* values = _Values();
			(&keys[index])->~KeyType();
			(&values[index])->~ValueType();
			--m_Size;
			for (uint32_t i = index; i < m_Size; i++) {
				new(&keys[i]) KeyType(std::move(keys[i + 1]));
				new(&values[i]) ValueType(std::move(values[i + 1]));
			}
			return true;
		}

		bool Contains(const KeyType& key) {
			return _FindIndex(key) != -1;
		}

		ValueType* Find(const KeyType& key) {
			if (!m_Data) {
				return nullptr;
			}
			uint32_t index = _FindIndex(key);
			if (index == -1) {
				return nullptr;
			}
			return _Values() + index;
		}

		const KeyType* GetKeys() const {
			return _Keys();
		}

		const ValueType* GetValues() const {
			return _Values();
		}

		ValueType* GetValues() {
			return _Values();
		}

		KeyIterator KeysBegin() {
			return _Keys();
		}

		ConstKeyIterator KeysBegin() const {
			return _Keys();
		}

		ConstKeyIterator KeysEnd() const {
			return _Keys() + m_Size;
		}

		ValueIterator ValuesBegin() {
			return _Values();
		}

		ConstValueIterator ValuesBegin() const {
			return _Values();
		}

		ConstValueIterator ValuesEnd() const {
			return _Values() + m_Size;
		}

		ValueIterator begin() {
			return ValuesBegin();
		}

		ConstValueIterator begin() const {
			return ValuesBegin();
		}

		ConstValueIterator end() const {
			return ValuesEnd();
		}

	private:

		KeyType* _Keys() const {
			return (KeyType*)m_Data;
		}

		ValueType* _Values() const {
			return (ValueType*)(m_Data + m_Capacity * sizeof(KeyType));
		}

		int64_t _FindIndex(const KeyType& key) {
			assert(m_Data);
			KeyType* ptr = _Keys();
			uint32_t left = 0;
			uint32_t right = m_Size - 1;
			uint32_t index = 0;
			while (left <= right) {
				index = (left + right) / 2;
				if (ptr[index] < key) {
					left = index + 1;
					continue;
				}
				if (key < ptr[index]) {
					right = index - 1;
					continue;
				}
				return index;
			}
			return -1;
		}

		int64_t _FindNewIndex(const KeyType& key) {
			assert(m_Data);
			KeyType* ptr = _Keys();
			int64_t left = 0;
			int64_t right = (int64_t)m_Size - 1;
			uint32_t index = 0;
			while (left <= right) {
				index = (left + right) / 2;
				if (ptr[index] < key) {
					left = index + 1;
					continue;
				}
				if (key < ptr[index]) {
					right = (int64_t)index - 1;
					continue;
				}
				return -1;
			}
			return left;
		}

		ValueType* _InsertToIndex(uint32_t index, const KeyType& key, const ValueType& value) {
			assert(m_Data);
			KeyType* keys = _Keys();
			ValueType* values = _Values();
			for (uint32_t i = m_Size; i > index; i--) {
				new(&keys[i]) KeyType(std::move(keys[i - 1]));
				new(&values[i]) ValueType(std::move(values[i - 1]));
			}
			ValueType& newValueSpot = values[index];
			new(&keys[index]) KeyType(key);
			new(&newValueSpot) ValueType(value);
			++m_Size;
			return &newValueSpot;
		}

		template<typename... Args>
		ValueType* _EmplaceToIndex(uint32_t index, const KeyType& key, Args&&... args) {
			assert(m_Data);
			KeyType* keys = _Keys();
			ValueType* values = _Values();
			for (uint32_t i = m_Size; i > index; i--) {
				new(&keys[i]) KeyType(std::move(keys[i - 1]));
				new(&values[i]) ValueType(std::move(values[i - 1]));
			}
			ValueType& newValueSpot = values[index];
			new(&keys[index]) KeyType(key);
			new(&newValueSpot) ValueType(std::forward<Args>(args)...);
			++m_Size;
			return &newValueSpot;
		}
	};

	template<typename T>
	class OrderedArray {
	private:
		
		uint32_t m_Size;
		uint32_t m_Capacity;
		T* m_Data;

	public:

		using Iterator = T*;
		using ConstIterator = const T*;

		OrderedArray() : m_Size(0), m_Capacity(0), m_Data(nullptr) {}

		OrderedArray(const OrderedArray& other) : m_Size(0), m_Capacity(0), m_Data(0) {
			Reserve(other.m_Capacity);
			for (uint32_t i = 0; i < other.m_Size; i++) {
				m_Data[i] = other.m_Data[i];
			}
		}

		OrderedArray(OrderedArray&& other) 
			: m_Size(other.m_Size), m_Capacity(other.m_Capacity), m_Data(other.m_Data) {
			other.m_Size = 0;
			other.m_Capacity = 0;
			other.m_Data = 0;
		}

		OrderedArray& operator==(const OrderedArray& other) {
			Clear();
			Reserve(other.m_Capacity);
			for (uint32_t i = 0; i < other.m_Size; i++) {
				m_Data[i] = other.m_Data[i];
			}
		}

		OrderedArray& operator==(OrderedArray&& other) {
			Clear();
			m_Size = other.m_Size;
			m_Capacity = other.m_Capacity;
			m_Data = other.m_Data;
			other.m_Size = 0;
			other.m_Capacity = 0;
			other.m_Data = nullptr;
		}

		~OrderedArray() {
			Clear();
		}

		uint32_t Size() const {
			return m_Size;
		}

		OrderedArray& Reserve(uint32_t capacity) {
			if (m_Capacity >= capacity) {
				return *this;
			}
			T* temp = (T*)malloc(capacity * sizeof(T));
			assert(temp && "malloc failed!");
			for (uint32_t i = 0; i < m_Size; i++) {
				temp[i] = m_Data[i];
			}
			free(m_Data);
			m_Data = temp;
			m_Capacity = capacity;
			return *this;
		}

		OrderedArray& Clear() {
			for (uint32_t i = 0; i < m_Size; i++) {
				(&m_Data[i])->~T();
			}
			m_Size = 0;
			m_Capacity = 0;
			free(m_Data);
			m_Data = nullptr;
			return *this;
		}

		T* Insert(const T& value) {
			if (!m_Data) {
				Reserve(4);
				*m_Data = value;
				++m_Size;
				return m_Data;
			}
			if (!m_Size) {
				*m_Data = value;
				++m_Size;
				return m_Data;
			}
			if (m_Capacity == m_Size) {
				Reserve(m_Capacity * 2);
			}
			int64_t index = _FindNewIndex(value);
			if (index == -1) {
				return nullptr;
			}
			return _InsertToIndex(index, std::move(value));
		}

		template<typename... Args>
		T* Emplace(Args&&... args) {
			if (!m_Data) {
				Reserve(4);
				new(m_Data) T(std::forward<Args>(args)...);
				++m_Size;
				return m_Data;
			}
			if (!m_Size) {
				new(m_Data) T(std::forward<Args>(args)...);
				++m_Size;
				return m_Data;
			}
			if (m_Capacity == m_Size) {
				Reserve(m_Capacity * 2);
			}
			T value(std::forward(args)...);
			int64_t index = _FindNewIndex();
			if (index == -1) {
				return nullptr;
			}
			return _EmplaceToIndex(index, std::move(value));
		}

		bool Erase(const T& value) {
			if (!m_Data || !m_Size) {
				return false;
			}
			int64_t index = _FindIndex(value);
			if (index == -1) {
				return false;
			}
			(&m_Data[index])->~T();
			--m_Size;
			for (uint32_t i = index; i < m_Size; i++) {
				new(&m_Data[i]) T(std::move(m_Data[i + 1]));
			}
			return true;
		}

		void EraseAll() {
			for (uint32_t i = 0; i < m_Size; i++) {
				(m_Data + i)->~T();
			}
			m_Size = 0;
		}

		bool Contains(const T& value) const {
			return m_Data && m_Size ? _FindIndex(value) != -1 : false;
		}

		T* Find(const T& value) {
			if (!m_Data || !m_Size) {
				return nullptr;
			}
			uint32_t index = _FindIndex(value);
			if (index == -1) {
				return nullptr;
			}
			return m_Data + index;
		}

		Iterator begin() {
			return m_Data;
		}

		ConstIterator begin() const {
			return m_Data;
		}

		ConstIterator end() const {
			return m_Data + m_Size;
		}

	private:

		int64_t _FindIndex(const T& value) const {
			assert(m_Data && m_Size);
			if (m_Size == 1) {
				const T& comp = m_Data[0];
				return !(comp < value) && !(value < comp) ? 0 : -1;
			}
			uint32_t left = 0;
			uint32_t right = m_Size - 1;
			uint32_t index = 0;
			while (left <= right) {
				index = (left + right) / 2;
				const T& comp = m_Data[index];
				if (comp < value) {
					left = index + 1;
					continue;
				}
				if (value < comp) {
					right = index - 1;
					continue;
				}
				return index;
			}
			return -1;
		}


		int64_t _FindNewIndex(const T& value) {
			assert(m_Data && m_Size);
			int64_t left = 0;
			int64_t right = (int64_t)m_Size - 1;
			uint32_t index = 0;
			while (left <= right) {
				index = (left + right) / 2;
				const T& comp = m_Data[index];
				if (comp < value) {
					left = index + 1;
					continue;
				}
				if (value < comp) {
					right = (int64_t)index - 1;
					continue;
				}
				return -1;
			}
			return left;
		}

		T* _InsertToIndex(uint32_t index, const T& value) {
			assert(m_Data);
			for (uint32_t i = m_Size; i > index; i--) {
				new(&m_Data[i]) T(std::move(m_Data[i - 1]));
			}
			T& newSpot = m_Data[index];
			new(&newSpot) T(value);
			++m_Size;
			return &newSpot;
		}

		template<typename... Args>
		T* _EmplaceToIndex(uint32_t index, T&& value) {
			assert(m_Data);
			for (uint32_t i = m_Size; i > index; i--) {
				new(&m_Data[i]) T(std::move(m_Data[i - 1]));
			}
			T& newSpot = m_Data[index];
			new(&newSpot) T(std::move(value));
			++m_Size;
			return &newSpot;
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

		String(const String& other) : m_Data(nullptr), m_Length(0), m_Capacity(0) {
			Reserve(other.m_Capacity);
			memcpy(m_Data, other.m_Data, other.m_Length * sizeof(char));
			m_Length = other.m_Length;
		}

		String& operator=(const String& other) {
			Clear();
			Reserve(other.m_Capacity);
			memcpy(m_Data, other.m_Data, other.m_Length * sizeof(char));
			m_Length = other.m_Length;
			return *this;
		}

		constexpr String(String&& other) noexcept
			: m_Data(other.m_Data), m_Length(other.m_Length), m_Capacity(other.m_Capacity) {
			other.m_Data = nullptr;
			other.m_Length = 0;
			other.m_Capacity = 0;
		}

		String(const char* str) noexcept : m_Data(nullptr), m_Length(0), m_Capacity(0) {
			uint32_t len = strlen(str);
			if (len < small_string_buffer_size) {
				m_Data = m_SmallStringBuffer;
			}
			else {
				Reserve(len + 1);
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
				Reserve(m_Length + 1);
			}
			uint32_t index = 0;
			for (; begin != end; begin++) {
				m_Data[index++] = buf[begin];
			}
			m_Data[m_Length] = '\0';
		}

		String(FILE* fileStream) {
			if (!fileStream || feof(fileStream) || ferror(fileStream)) {
				PrintError(ErrorOrigin::FileParsing,
					"invalid file stream passed to String (in String constructor)!");
			}
			uint32_t length;
			if (fread(&length, sizeof(uint32_t), 1, fileStream) != 1) {
				PrintError(ErrorOrigin::FileParsing,
					"failed to read string length from file stream (in String constructor)!");
				return;
			}
			Reserve(length + 1);
			uint32_t readLength = fread(&m_Data, sizeof(char), length, fileStream);
			if (readLength != length) {
				PrintError(ErrorOrigin::FileParsing,
					"failed to read entire string from file stream (in String constructor)!");
			}
			assert(length >= readLength);
			m_Data[readLength] = '\0';
			m_Length = readLength;
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
			char* temp = (char*)malloc(capacity * sizeof(char));
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
			m_Data[m_Length] = '\0';
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

		String& operator+=(char c) {
			return Push(c);
		}

		friend String operator+(const String& a, const String& b) {
			String result{};
			uint32_t rLength = a.m_Length + b.m_Length;
			result.Reserve(rLength + 1);
			memcpy(result.m_Data, a.m_Data, a.m_Length * sizeof(char));
			memcpy(result.m_Data + a.m_Length, b.m_Data, b.m_Length * sizeof(char));
			result.m_Length = rLength;
			result.m_Data[result.m_Length] = '\0';
			return result;
		}

		friend String operator+(const String& a, const char* b) {
			String result{};
			uint32_t bLength = strlen(b);
			uint32_t rLength = a.m_Length + bLength;
			result.Reserve(rLength + 1);
			memcpy(result.m_Data, a.m_Data, a.m_Length * sizeof(char));
			memcpy(result.m_Data + a.m_Length, b, bLength * sizeof(char));
			result.m_Length = rLength;
			result.m_Data[result.m_Length] = '\0';
			return result;
		}

		uint64_t operator()() const {
			if (!m_Length) {
				return 0;
			}
			uint64_t res = 37;
			for (uint32_t i = 0; i < m_Length; i++) {
				res = (res * 54059) ^ ((uint64_t)m_Data[i] * 76963);
			}
			return res;
		}

		bool operator==(const String& other) const {
			return m_Data && other.m_Data && !strcmp(m_Data, other.m_Data);
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

	template<typename T>
	String IntToString(T value) {
		static_assert(std::is_integral<T>());
		if constexpr (std::is_unsigned<T>()) {
			char buffer[21];
			uint64_t uint = value;
			buffer[20] = '0' + uint % 10;
			uint32_t i = 20;
			while (uint /= 10) {
				buffer[--i] = '0' + uint % 10;
			}
			return String(buffer, i, 21);
		}
		else {
			if (value < 0) {
				int64_t absInt = abs(value);
				char buffer[21];
				buffer[20] = '0' + absInt % 10;
				uint32_t i = 20;
				while (absInt /= 10) {
					buffer[--i] = '0' + absInt % 10;
				}
				buffer[--i] = '-';
				return String(buffer, i, 21);
			}
			char buffer[20];
			int64_t absInt = value;
			buffer[19] = '0' + absInt % 10;
			uint32_t i = 19;
			while (absInt /= 10) {
				buffer[--i] = '0' + absInt % 10;
			}
			return String(buffer, i, 20);
		}
		return {};
	}

	template<typename T, typename U>
	struct Tuple {
		T first;
		U second;
	};

	template<typename T>
	class Dictionary {
	public:

		static constexpr uint32_t max_bucket_size = 4;

		using KeyType = String;
		using ValueType = T;
		using KeyBucket = KeyType[max_bucket_size];
		using ValueBucket = ValueType[max_bucket_size];

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
			if (m_KeyBuckets) {
				for (const auto& pair : *this) {
					(&pair.first)->~KeyType();
					(&pair.second)->~ValueType();
				}
			}
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

		ValueType* Find(const String& key) const {
			uint64_t hash = key();
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
		ValueType* Emplace(const String& key, Args&&... args) {
			if ((float)m_Size / m_Capacity > 0.9f) {
				Reserve(m_Capacity * 2);
			}
			uint64_t hash = key();
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

		ValueType* Insert(const String& key, const ValueType& value) {
			if ((float)m_Size / m_Capacity > 0.9f) {
				Reserve(m_Capacity * 2);
			}
			uint64_t hash = key();
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

		bool Erase(const String& key) {
			uint64_t hash = key();
			uint32_t index = hash % m_Capacity;
			uint8_t& bucketSize = m_BucketSizes[index];
			KeyBucket& keyBucket = m_KeyBuckets[index];
			ValueBucket& valueBucket = m_ValueBuckets[index];
			if (bucketSize) {
				for (uint32_t i = 0; i < bucketSize; i++) {
					if (key == keyBucket[i]) {
						for (uint32_t j = i; j < bucketSize - 1; j++) {
							keyBucket[i].~String();
							valueBucket[i].~T();
							new(&keyBucket[i]) String(std::move(keyBucket[i + 1]));
							new(&valueBucket[i]) T(std::move(valueBucket[i + 1]));
						}
						--bucketSize;
						return true;
					}
				}
			}
			return false;
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

		template<size_t delimiter_count_T>
		static int Skip(FILE* fs, const Array<char, delimiter_count_T>& delimiters) {
			char c = fgetc(fs);
			while (true) {
				if (c == EOF) {
					return EOF;
				}
				for (char delim : delimiters) {
					if (c == delim) {
						return c;
					}
				}
				c = fgetc(fs);
			}
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

		static int SkipLine(FILE* fs) {
			char c = fgetc(fs);
			while (true) {
				if (c == EOF) {
					return EOF;
				}
				if (c == '\n') {
					return c;
				}
			}
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

	class Obj {
	private:

		uint32_t m_LinesParsed = 0;

		DynamicArray<Vec3> m_Vs;
		DynamicArray<Vec2> m_Vts;
		DynamicArray<Vec3> m_Vns;

		DynamicArray<uint32_t> m_VIndices{};
		DynamicArray<uint32_t> m_VtIndices{};
		DynamicArray<uint32_t> m_VnIndices{};

		uint32_t m_MaxVIndex;
		uint32_t m_MaxVtIndex;
		uint32_t m_MaxVnIndex;

	public:

		bool Load(FILE* fileStream) {
			if (!fileStream) {
				return false;
			}
			char buf[3];
			buf[2] = '\0';
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
						m_MaxVIndex = Max(vI, m_MaxVIndex);
						if (fgetc(fileStream) == '/') {
							uint32_t vtI;
							if (fscanf(fileStream, "%i", &vtI) == 1) {
								m_VtIndices.PushBack(--vtI);
								m_MaxVtIndex = Max(vtI, m_MaxVtIndex);
							}
						}
						if (fgetc(fileStream) == '/') {
							uint32_t& vnI = m_VnIndices.EmplaceBack();
							if (fscanf(fileStream, "%i", &vnI) != 1) {
								m_LinesParsed = 0;
								return false;
							}
							--vnI;
							m_MaxVnIndex = Max(vnI, m_MaxVnIndex);
						}
					}
					for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
					continue;
				}
				for (char c = fgetc(fileStream); c != '\n' && c != EOF; c = fgetc(fileStream)) {}
			}
			if (!IsValid()) {
				m_LinesParsed = 0;
				return false;
			}
			return true;
		}

		bool IsValid() const {
			return (!m_VtIndices.Size() || m_VIndices.Size() == m_VtIndices.Size()) &&
				(!m_VnIndices.Size() || m_VIndices.Size() == m_VnIndices.Size()) && !(m_VtIndices.Size() % 3) &&
				m_MaxVIndex < m_Vs.Size() && (!m_VtIndices.Size() || m_MaxVtIndex < m_Vts.Size()) && (!m_VnIndices.Size()|| m_MaxVnIndex < m_Vns.Size());
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

		Vec3_T<T> Dimensions() const {
			return m_Max - m_Min;
		}

		Vec3_T<T> Middle() const {
			return m_Min + Dimensions() / 2;
		}

		bool IsPointInside(const Vec3_T<T>& point) const {
			return point.x > m_Min.x && point.y > m_Min.y && point.z > m_Min.z &&
				point.x < m_Max.x && point.y < m_Max.y && point.z < m_Max.z;
		}

		bool IsLineCrossing(const Vec3_T<T>& a, const Vec3_T<T>& b) const {

			static constexpr auto get_intersection = [](T distance1, T distance2, 
					const Vec3_T<T>& a, const Vec3_T<T>& b, Vec3_T<T>& out) -> bool {
				if ((distance1 * distance2) >= 0.0f) {
					return false;
				}
				if (distance1 == distance2) {
					return false;
				}
				out = a + (b - a) * (-distance1 / (distance2 - distance1));
				return true;
			};

			static constexpr auto in_box = [](const Box<T>& box, const Vec3_T<T>& intersection, uint32_t axis) -> bool {
				if (axis == 1) {
					return intersection.y > box.m_Min.y && intersection.y < box.m_Max.y && 
						intersection.z > box.m_Min.z && intersection.y < box.m_Max.z;
				}
				if (axis == 2) {
					return intersection.x > box.m_Min.x && intersection.x < box.m_Max.x && 
						intersection.z > box.m_Min.z && intersection.z < box.m_Max.z;
				}
				if (axis == 3) {
					return intersection.x > box.m_Min.x && intersection.x < box.m_Max.x && 
						intersection.x > box.m_Min.x && intersection.x < box.m_Max.x;
				}
				return false;
			};

			if (a.x < m_Min.x && b.x < m_Min.x) {
				return false;
			}
			if (a.x > m_Max.x && b.x > m_Max.x) {
				return false;
			}
			if (a.y < m_Min.y && b.y < m_Min.y) {
				return false;
			}
			if (a.y > m_Max.y && b.y > m_Max.y) {
				return false;
			}
			if (a.z < m_Min.z && b.z < m_Min.z) {
				return false;
			}
			if (a.z > m_Max.z && b.z > m_Max.z) {
				return false;
			}
			if (a.x > m_Min.x && a.x < m_Max.x &&
				a.y > m_Min.y && a.y < m_Max.y &&
				a.z > m_Min.z && a.z < m_Max.z) {
				return true;
			}
			Vec3 intersection;
			return get_intersection(a.x - m_Min.x, b.x - m_Min.x, a, b, intersection) && in_box(*this, intersection, 1) ||
				get_intersection(a.y - m_Min.y, b.y - m_Min.y, a, b, intersection) && in_box(*this, intersection, 2) ||
				get_intersection(a.z - m_Min.z, b.z - m_Min.z, a, b, intersection) && in_box(*this, intersection, 3) ||
				get_intersection(a.x - m_Max.x, b.x - m_Max.x, a, b, intersection) && in_box(*this, intersection, 1) ||
				get_intersection(a.y - m_Max.y, b.y - m_Max.y, a, b, intersection) && in_box(*this, intersection, 2) ||
				get_intersection(a.z - m_Max.z, b.z - m_Max.z, a, b, intersection) && in_box(*this, intersection, 3);
		}

		bool OverLaps(const Box& other) const {
			return m_Max.x > other.m_Min.x && other.m_Max.x > m_Min.x &&
				m_Max.y > other.m_Min.y && other.m_Max.y > m_Min.y &&
				m_Max.z > other.m_Min.z && other.m_Max.z > m_Min.z;
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

		using Face = Array<Vec3, 3>;

	private:

		Box<float> m_BoundingBox;
		DynamicArray<Face> m_TransformedFaces;
		DynamicArray<Face> m_Faces;
		DynamicArray<Vec3> m_Vertices;

	public:

		LogicMesh() : m_Faces(), m_TransformedFaces(), m_Vertices() {}

		LogicMesh(const DynamicArray<Vertex>& vertices, const DynamicArray<uint32_t> indices) 
			: m_Faces(), m_TransformedFaces(), m_Vertices() {
			Load(vertices, indices);
		}

		template<size_t vertex_count_T, size_t index_count_T>
		LogicMesh(const Array<Vertex, vertex_count_T>& vertices, const Array<uint32_t, index_count_T>& indices) 
			: m_Faces(), m_TransformedFaces(), m_Vertices() {
			Load(vertices, indices);
		}

		LogicMesh(const LogicMesh&) = default;
		LogicMesh& operator=(const LogicMesh&) = default;

		LogicMesh(LogicMesh&&) = default;
		LogicMesh& operator=(LogicMesh&&) = default;

		const Box<float>& GetBoundingBox() const {
			return m_BoundingBox;
		}

		const bool AABBCheck(const Vec3& position) const {
			return m_BoundingBox.IsPointInside(position);
		}

		const bool AABBCheck(const Ray& ray) const {
			return m_BoundingBox.IsLineCrossing(ray.m_Origin, ray.m_Origin + ray.m_Direction * ray.m_Length);
		}

		bool Load(const DynamicArray<Vertex>& vertices, const DynamicArray<uint32_t> indices) {
			if (indices.Size() % 3) {
				PrintError(ErrorOrigin::Engine, 
					"indices size must be multiple of 3 when loading mesh (in function LogicMesh::Load)!");
				return false;
			}
			m_Faces.Clear();
			m_TransformedFaces.Clear();
			m_Vertices.Clear();
			m_Vertices.Reserve(vertices.Size());
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
			m_Faces.Reserve(vertexCount / 3);
			for (uint32_t i = 0; i < vertexCount; i += 3) {
				Face& face = m_Faces.EmplaceBack();
				face[0] = m_Vertices[i];
				face[1] = m_Vertices[i + 1];
				face[2] = m_Vertices[i + 2];
			}
			return true;
		}

		template<size_t vertex_count_T, size_t index_count_T>
		bool Load(const Array<Vertex, vertex_count_T>& vertices, const Array<uint32_t, index_count_T>& indices) {
			static_assert(!(index_count_T % 3));
			m_Faces.Clear();
			m_TransformedFaces.Clear();
			m_Vertices.Clear();
			m_Vertices.Reserve(vertex_count_T);
			for (uint32_t i = 0; i < index_count_T; i++) {
				uint32_t index = indices[i];
				if (index >= vertex_count_T) {
					PrintError(ErrorOrigin::Engine, 
						"found invalid index when loading mesh (in function LogicMesh::Load)!");
					return false;
				}
				m_Vertices.PushBack(vertices[index].m_Position);
			}
			uint32_t vertexCount = m_Vertices.Size();
			assert(!(vertexCount % 3));
			m_Faces.Reserve(vertexCount / 3);
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
					pos.y *= -1;
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

		bool IsRayHit(const Ray& ray, const Face& face, Vec3& outHitPosition, float& outDistance) const {
			Vec3 edge1 = face[2] - face[0];
			Vec3 edge2 = face[1] - face[0];
			Vec3 normal = Cross(edge1, edge2);
			float det = -Dot(ray.m_Direction, normal);
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
			return det >= 1e-6 && outDistance > 0.0f && outDistance <= ray.m_Length && 
				u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f;
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

		~StaticMesh() {
			Terminate();
		}

		bool IsNull() const {
			return !m_VertexBuffer.m_BufferSize || !m_IndexBuffer.m_BufferSize;
		}

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
			if (!IsNull()) {
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

		static inline DynamicArray<GLFWcursorposfun> s_CursorPositionCallbacks;
		static inline DynamicArray<GLFWscrollfun> s_ScrollCallbacks;
		
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

		static inline Vec2_T<int> s_ContentArea { 1, 1 };

		static inline Vec2_T<double> s_CursorPosition{};
		static inline Vec2_T<double> s_DeltaCursorPosition{};

		static inline Vec2_T<double> s_DeltaScrollOffset{};

	public:

		static bool AddCursorPositionCallback(GLFWcursorposfun function) {
			if (!function) {
				return false;
			}
			s_CursorPositionCallbacks.PushBack(function);
			return true;
		}

		static bool RemoveCursorPositionCallback(GLFWcursorposfun function) {
			auto iter = s_CursorPositionCallbacks.begin();
			auto end = s_CursorPositionCallbacks.end();
			for (; iter != end; iter++) {
				if (*iter == function) {
					s_CursorPositionCallbacks.Erase(iter);
					return true;
				}
			}
			return false;
		}

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

		static Vec2_T<double> GetMousePosition() {
			return s_CursorPosition;
		}

		static Vec2_T<double> GetDeltaMousePosition() {
			return s_DeltaCursorPosition;
		}

		static Vec2_T<double> GetDeltaScrollOffset() {
			return s_DeltaScrollOffset;
		}

	private:

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

		static void CursorPositionCallback(GLFWwindow* window, double xPos, double yPos) {
			Vec2_T<double> pos = { xPos, yPos };
			s_DeltaCursorPosition = Vec2_T<double>(pos.x - s_CursorPosition.x, (pos.y - s_CursorPosition.y) * -1);
			s_CursorPosition = pos;
			for (auto function : s_CursorPositionCallbacks) {
				function(window, xPos, yPos);
			}
		}

		static void ScrollCallback(GLFWwindow*, double xOffset, double yOffset) {
			s_DeltaScrollOffset = { xOffset, yOffset };
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
			s_DeltaCursorPosition = 0.0;
			s_DeltaScrollOffset = 0.0;
		};

		Input(GLFWwindow* pGLFWwindow) {
			s_ActiveKeys.Reserve(key_count);
			s_ActiveMouseButtons.Reserve(mouse_button_count);
			s_TextInput.Reserve(256);
			glfwSetKeyCallback(pGLFWwindow, KeyCallback);
			glfwSetMouseButtonCallback(pGLFWwindow, MouseButtonCallback);
			glfwSetCharCallback(pGLFWwindow, CharacterCallback);
			glfwSetCursorPosCallback(pGLFWwindow, CursorPositionCallback);
			glfwSetScrollCallback(pGLFWwindow, ScrollCallback);
			GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();
			glfwGetWindowSize(pGLFWwindow, &s_ContentArea.x, &s_ContentArea.y);
		};
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

			IntVec2 Dimensions() const {
				return m_Max - m_Min;
			}

			Vec2 Middle() const {
				return m_Min + Dimensions() / 2;
			}

			bool IsPointInside(IntVec2 point) const {
				return point.x >= m_Min.x && point.y >= m_Min.y 
					&& point.x <= m_Max.x && point.y <= m_Max.y;
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

			using Iterator = Character*;
			using ConstIterator = const Character*;

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

		using TextFragmentPushConstant = DynamicText::FragmentPushConstant;

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

			using Pipeline2DRenderCallback = bool(*)(const Window& window, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);

			class Button {
			public:

				using Pipeline2DRenderCallback = bool(*)(const Button& button, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);
		
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

		using Button = Window::Button;

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

			Input::AddCursorPositionCallback(
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

	using ObjectID = uint64_t;
	using RenderID = uint64_t;

	class PhysicsManager {

		friend class Engine;

	public:

		using ObjectLayer = JPH::ObjectLayer;
		using BroadPhaseLayer = JPH::BroadPhaseLayer;
		using uint = JPH::uint;
		using Body = JPH::Body;

		struct Layer {

			static constexpr ObjectLayer NonMoving = 0;
			static constexpr ObjectLayer Moving = 1;
			static constexpr ObjectLayer Kinematic = 2;

			static constexpr ObjectLayer Default = Moving;
			static constexpr uint LayerCount = 3;

			ObjectLayer m_Value;

			constexpr Layer(ObjectLayer value = Layer::NonMoving) {
				m_Value = value;
			}

			constexpr Layer& operator=(ObjectLayer value) {
				m_Value = value;
				return *this;
			}

			constexpr operator ObjectLayer() {
				return m_Value;
			}
		};

		struct BroadPhaseLayers {

			static constexpr BroadPhaseLayer NonMoving { 0 };
			static constexpr BroadPhaseLayer Moving { 1 };
			static constexpr BroadPhaseLayer Kinematic { 2 };

			static constexpr const BroadPhaseLayer& Default = Moving;
			static constexpr uint LayerCount = 3;
		};

	private:

		class Stack {
		private:

			const uint32_t c_Size;
			uint32_t m_End;
			uint8_t* m_Data;

		public:

			Stack(uint32_t size) : c_Size(size), m_End(0), m_Data(nullptr) {
				assert(c_Size);
				m_Data = (uint8_t*)malloc(c_Size);
			}

			~Stack() {
				free(m_Data);
				m_Data = nullptr;
			}

			template<typename T, typename... Args>
			T* Allocate(Args&&... args) {
				if (m_End + sizeof(T) > c_Size) {
					PrintError(ErrorOrigin::OutOfMemory,
						"stack was out of memory (in function PhysicsManager::Stack::Allocate)!");
					return nullptr;
				}
				T* p = (T*)(m_Data + m_End);
				new(p) T(std::forward<Args>(args)...);
				m_End += sizeof(T);
				return p;
			}

			void Clear() {
				m_End = 0;
			}
		};

		class JoltObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
		public:
			virtual bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
				switch (a) {
					case Layer::NonMoving:
						return b == Layer::Moving;
					case Layer::Moving:
						return true;
					case Layer::Kinematic:
						return true;
					default:
						JPH_ASSERT(false);
						return false;
				}
			}
		};

		class JoltBroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
		private:

			BroadPhaseLayer m_ObjectToBroadPhase[Layer::LayerCount];

		public:
			
			JoltBroadPhaseLayerInterfaceImpl() {
				m_ObjectToBroadPhase[Layer::NonMoving] = BroadPhaseLayers::NonMoving;
				m_ObjectToBroadPhase[Layer::Moving] = BroadPhaseLayers::Moving;
				m_ObjectToBroadPhase[Layer::Kinematic] = BroadPhaseLayers::Kinematic;
			}

			virtual uint GetNumBroadPhaseLayers() const override {
				return BroadPhaseLayers::LayerCount;
			}

			virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
				JPH_ASSERT(layer < Layer::LayerCount);
				return m_ObjectToBroadPhase[layer];
			}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
				switch ((BroadPhaseLayer::Type)layer) {
					case (BroadPhaseLayer::Type)BroadPhaseLayers::NonMoving:
						return "NonMoving";
					case (BroadPhaseLayer::Type)BroadPhaseLayers::Moving:
						return "Moving";
					case (BroadPhaseLayer::Type)BroadPhaseLayers::Kinematic:
						return "Kinematic";
					default:
						JPH_ASSERT(false);
						return "Invalid";
				}
			}
#endif
		};

		class JoltObjectVsBroadPhaseFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
			virtual bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
				switch (a) {
					case Layer::NonMoving:
						return b == BroadPhaseLayers::Moving;
					case Layer::Moving:
						return true;
					case Layer::Kinematic:
						return true;
					default:
						JPH_ASSERT(false);
						return false;
				}
			}
		};

		class GlobalJoltContactListener : public JPH::ContactListener {

			using ContactManifold = JPH::ContactManifold;
			using ContactSettings = JPH::ContactSettings;
			using SubShapeIDPair = JPH::SubShapeIDPair;

			virtual void OnContactAdded(const Body& a, const Body& b, const ContactManifold& manifold, ContactSettings& ioSettings) override {
			}

			virtual void OnContactPersisted(const Body& a, const Body& b, const ContactManifold& manifold, ContactSettings& ioSettings) override {
			}

			virtual void OnContactRemoved(const SubShapeIDPair& subShapeIDPair) override {
			}
		};

		class GlobalJoltBodyActivationListener : public JPH::BodyActivationListener {
		public:

			using BodyID = JPH::BodyID;

			virtual void OnBodyActivated(const BodyID& odyID, JPH::uint64 inBodyUserData) override {
			}

			virtual void OnBodyDeactivated(const BodyID& bodyID, JPH::uint64 inBodyUserData) override {
			}
		};

		class JoltDebugRendererImpl : public JPH::DebugRenderer {
		private:

			class BatchImpl : public JPH::RefTargetVirtual {
			private:
	
				JPH::atomic<uint32_t> m_RefCount = 0;

			public:

				JoltDebugRendererImpl& m_DebugRenderer;
				ObjectID m_ObjectID;

				BatchImpl(JoltDebugRendererImpl& renderer, ObjectID objectID) 
					: m_DebugRenderer(renderer), m_ObjectID(objectID) {}

				JPH_OVERRIDE_NEW_DELETE

				virtual void AddRef() override { 
					++m_RefCount; 
				}

				virtual void Release() override { 
					if (--m_RefCount == 0) {
						m_DebugRenderer.TerminateMesh(m_ObjectID);
						delete(this);
					}
				}
			};

			ObjectID m_NextObjectID = 0;
			OrderedDictionary<ObjectID, StaticMesh> m_Meshes{};

		public:

			World& m_World;
			Renderer& m_Renderer;

			JoltDebugRendererImpl(World& world, Renderer& renderer) : m_World(world), m_Renderer(renderer) {}
			
			void BaseInitialize() {
				Initialize();
			}

			void Terminate() {
				for (StaticMesh& mesh : m_Meshes) {
					mesh.Terminate();
				}
			}

			JPH_OVERRIDE_NEW_DELETE

			void TerminateMesh(ObjectID m_ObjectID) {
				m_Meshes.Erase(m_ObjectID);
			}

			virtual Batch CreateTriangleBatch(const Triangle* triangles, int triangleCount) override {
				assert(triangleCount % 3 == 0);
				DynamicArray<engine::Vertex> vertices(triangleCount * 3);
				DynamicArray<uint32_t> indices(triangleCount * 3);
				for (uint32_t i = 0; i < triangleCount; i++) {
					const Triangle& triangle = triangles[i];
					for (uint32_t j = 0; j < 3; j++) {
						const Vertex& inVertex = triangle.mV[j];
						engine::Vertex& convVertex = vertices[i * 3 + j];
						convVertex.m_Position = { inVertex.mPosition.x, inVertex.mPosition.y, inVertex.mPosition.z };
						convVertex.m_Normal = { inVertex.mNormal.x, inVertex.mNormal.y, inVertex.mNormal.z };
						convVertex.m_UV = { inVertex.mUV.x, inVertex.mUV.y };
						indices[i * 3 + j] = i * 3 + (2 - j);
					}
				}
				StaticMesh* mesh = m_Meshes.Emplace(m_NextObjectID, m_Renderer);
				assert(mesh);
				if (!mesh->CreateBuffers(vertices.Size(), vertices.Data(), indices.Size(), indices.Data())) {
					PrintError(ErrorOrigin::Engine,
						"failed to create buffers for mesh (function StaticMesh::CreateBuffers in function PhysicsManager::JoltDebugRenderer::CreateTriangleBatch)!");
					m_Meshes.Erase(m_NextObjectID);
					return nullptr;
				}
				return new BatchImpl(*this, m_NextObjectID++);
			}

			virtual Batch CreateTriangleBatch(const Vertex* vertices, int vertexCount, const uint32_t* indices, int indexCount) override {
				DynamicArray<engine::Vertex> convVertices(vertexCount);
				for (uint32_t i = 0; i < vertexCount; i++) {
					const Vertex& inVert = vertices[i];
					engine::Vertex& convVert = convVertices[i];
					convVert.m_Position = { inVert.mPosition.x, inVert.mPosition.y, inVert.mPosition.z };
					convVert.m_Normal = { inVert.mNormal.x, inVert.mNormal.y, inVert.mNormal.z };
					convVert.m_UV = { inVert.mUV.x, inVert.mUV.y };
				}
				StaticMesh* mesh = m_Meshes.Emplace(m_NextObjectID, m_Renderer);
				assert(mesh);
				if (!mesh->CreateBuffers(vertexCount, convVertices.Data(), indexCount, indices)) {
					PrintError(ErrorOrigin::Engine,
						"failed to create buffers for mesh (function StaticMesh::CreateBuffers in function PhysicsManager::JoltDebugRenderer::CreateTriangleBatch)!");
					m_Meshes.Erase(m_NextObjectID);
					return nullptr;
				}
				return new BatchImpl(*this, m_NextObjectID++);
			}

			virtual void DrawGeometry(JPH::RMat44Arg modelMat, const JPH::AABox& worldSpaceBounds, float LODScale,
				JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode) override;

			virtual void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override {}

			virtual void DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow) override {}

			virtual void DrawText3D(JPH::RVec3Arg position, const JPH::string_view &string, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override {}
		};

		class JoltBodyDrawFilterImpl : public JPH::BodyDrawFilter {
		private:

			OrderedArray<JPH::BodyID> m_ShouldRender{};

		public:

			uint32_t BodyIDCount() const {
				return m_ShouldRender.Size();
			}

			bool ContainsBodyID(JPH::BodyID ID) const {
				return m_ShouldRender.Contains(ID);
			}
	
			bool AddBodyID(JPH::BodyID ID) {
				return m_ShouldRender.Insert(ID);
			}

			bool RemoveBodyID(JPH::BodyID ID) {
				return m_ShouldRender.Erase(ID);
			}

			void RemoveAllBodyIDs() {
				m_ShouldRender.EraseAll();
			}

			virtual bool ShouldDraw(const JPH::Body& body) const override {
				return m_ShouldRender.Contains(body.GetID());
			}
		};

		static void JoltTraceImpl(const char* fmt, ...) {
			va_list list;
			va_start(list, fmt);
			char buf[1024];
			vsnprintf(buf, sizeof(buf), fmt, list);
			va_end(list);
			fmt::print(fmt::emphasis::bold, "Jolt trace: {}\n", buf);
		}

		static bool JoltAssertFailedImpl(const char* expr, const char* msg, const char* file, JPH::uint line) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"{}:{}:\nAssertion failed ({})\n{}\n", file, line, expr, msg ? msg : "");
			CriticalError(ErrorOrigin::Jolt, "Jolt assertion failed!");
			return true;
		}

		JPH::JobSystemThreadPool* m_JobSystem;
		JPH::PhysicsSystem* m_PhysicsSystem;
		JPH::TempAllocatorImpl* m_TempAllocator;
		Stack m_Stack;
		JoltBroadPhaseLayerInterfaceImpl m_BroadPhaseLayerInterface{};
		JoltObjectVsBroadPhaseFilterImpl m_ObjectVsBroadPhaseFilter{};
		JoltObjectLayerPairFilterImpl m_ObjectLayerPairFilter{};
		GlobalJoltContactListener m_GlobalContactListener{};
		GlobalJoltBodyActivationListener m_GlobalBodyActivationListener{};
		JoltDebugRendererImpl m_DebugRenderer;
		JoltBodyDrawFilterImpl m_BodyDrawFilter{};
		const uint c_MaxBodies;
		const uint c_MaxBodyPairs;
		const uint c_MaxContactConstraints;
		float m_FrameTimer = 0.0f;
		float m_FixedDeltaTime = 1.0f / 60.0f;
		int m_CollisionSteps = 1;

		PhysicsManager(World& world, Renderer& renderer, uint maxBodies, uint maxBodyPairs, uint maxContactConstraints)
			: m_JobSystem(nullptr), m_TempAllocator(nullptr), m_PhysicsSystem(nullptr), m_Stack(65536), m_DebugRenderer(world, renderer),
				c_MaxBodies(maxBodies), c_MaxBodyPairs(maxBodyPairs), c_MaxContactConstraints(maxContactConstraints) {
		}

		void Initialize() {

			using namespace JPH;

			RegisterDefaultAllocator();

			Trace = JoltTraceImpl;
			JPH_IF_ENABLE_ASSERTS(AssertFailed = JoltAssertFailedImpl);

			Factory::sInstance = m_Stack.Allocate<Factory>();

			RegisterTypes();

			m_TempAllocator = m_Stack.Allocate<TempAllocatorImpl>(10 * 1024 * 1024);

			m_JobSystem = m_Stack.Allocate<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);

			m_PhysicsSystem = m_Stack.Allocate<PhysicsSystem>();
			m_PhysicsSystem->Init(c_MaxBodies, 0, c_MaxBodyPairs, c_MaxContactConstraints, m_BroadPhaseLayerInterface, m_ObjectVsBroadPhaseFilter, m_ObjectLayerPairFilter);

			m_PhysicsSystem->SetContactListener(&m_GlobalContactListener);
			m_PhysicsSystem->SetBodyActivationListener(&m_GlobalBodyActivationListener);
			
			m_DebugRenderer.BaseInitialize();
		}

		void Terminate() {
			using namespace JPH;
			m_DebugRenderer.Terminate();
			UnregisterTypes();
			Factory::sInstance->~Factory();
			Factory::sInstance = nullptr;
			m_Stack.Clear();
			m_PhysicsSystem = nullptr;
		}

		bool PhysicsUpdate() {
			using namespace JPH;
			if (m_FrameTimer >= m_FixedDeltaTime) {
				m_PhysicsSystem->Update(m_FixedDeltaTime, m_CollisionSteps, m_TempAllocator, m_JobSystem);
				m_FrameTimer = 0.0f;
				return true;
			}
			m_FrameTimer += Time::DeltaTime();
			return false;
		}

		void DebugUpdate() {
			using namespace JPH;
			if (m_BodyDrawFilter.BodyIDCount()) {
				BodyManager::DrawSettings drawSettings{};
				drawSettings.mDrawShapeWireframe = true;
				m_PhysicsSystem->DrawBodies(drawSettings, &m_DebugRenderer, &m_BodyDrawFilter);
			}
		}

	public:
		
		JPH::BodyInterface& GetBodyInterface() {
			return m_PhysicsSystem->GetBodyInterface();
		}

		JoltBodyDrawFilterImpl& GetBodyDrawFilter() {
			return m_BodyDrawFilter;
		}
	};

	typedef PhysicsManager::Layer PhysicsLayer;

	class World;

	constexpr inline uint64_t Invalid_ID = UINT64_MAX;

	enum WorldRenderDataFlagBits {
		WorldRenderDataFlag_NoSave = 1,
	};

	typedef uint32_t WorldRenderDataFlags;

	struct WorldRenderData {

		WorldRenderDataFlags m_Flags;

		VkDescriptorSet m_AlbedoTextureDescriptorSet = VK_NULL_HANDLE;
		Mat4 m_Transform{};
		MeshData m_MeshData{};

		WorldRenderData(WorldRenderDataFlags flags, const Mat4& transform, const MeshData& meshData) noexcept
			: m_Flags(flags), m_Transform(transform), m_MeshData(meshData) {}

		WorldRenderData(const WorldRenderData&) = delete;

		WorldRenderData(WorldRenderData&& other) noexcept = default;
	};

	class Body {

		friend class Area;
		friend class World;
		friend class OrderedDictionary<ObjectID, Body>;

	public:

		enum class ColliderShape {
			None = 0,
			Sphere,
			Box,
			Capsule,
			ConvexHull,
		};

		struct SphereCreateInfo {
			float m_Radius;
		};

		struct BoxCreateInfo {

			Vec3 m_HalfExtent;
			float m_ConvexRadius;
		};

		struct CapsuleCreateInfo {
			float m_HalfHeight;
			float m_Radius;
		};

		struct ConvexHullCreateInfo {
			JPH::Vec3* m_Points;
			uint32_t m_PointCount;
			float m_MaxConvexRadius;
		};

		union ShapeCreateInfo_U {
			SphereCreateInfo m_Sphere;
			BoxCreateInfo m_Box;
			CapsuleCreateInfo m_Capsule;
			ConvexHullCreateInfo m_ConvexHull;
		};

		struct ColliderCreateInfo {
			ColliderShape m_ColliderShape{};
			ShapeCreateInfo_U u_ShapeCreateInfo{};
		};

	private:	
	
		union ShapeSettings_U {

			int u_None{};
			JPH::SphereShapeSettings m_Sphere;
			JPH::BoxShapeSettings m_Box;
			JPH::CapsuleShapeSettings m_Capsule;
			JPH::ConvexHullShapeSettings m_ConvexHull;

			ShapeSettings_U() : u_None(-1) {}

			ShapeSettings_U(ShapeSettings_U&& other, ColliderShape shape) {
				using namespace JPH;
				switch (shape) {
					using S = ColliderShape;
					case S::None:
						u_None = -1;
						break;
					case S::Sphere:
						new(&m_Sphere) SphereShapeSettings(other.m_Sphere.mRadius, other.m_Sphere.mMaterial);
						m_Sphere.SetDensity(other.m_Sphere.mDensity);
						m_Sphere.mUserData = other.m_Sphere.mUserData;
						m_Sphere.SetEmbedded();
						break;
					case S::Box:
						new(&m_Box) BoxShapeSettings(other.m_Box.mHalfExtent, other.m_Box.mConvexRadius, other.m_Box.mMaterial);
						m_Box.SetDensity(other.m_Box.mDensity);
						m_Box.mUserData = other.m_Box.mUserData;
						m_Box.SetEmbedded();
						break;
					case S::Capsule:
						new(&m_Capsule) CapsuleShapeSettings(other.m_Capsule.mHalfHeightOfCylinder, other.m_Capsule.mRadius, other.m_Capsule.mMaterial);
						m_Capsule.SetDensity(other.m_Capsule.mDensity);
						m_Capsule.mUserData = other.m_Capsule.mUserData;
						m_Capsule.SetEmbedded();
						break;
					case S::ConvexHull:
						new(&m_ConvexHull) ConvexHullShapeSettings(other.m_ConvexHull.mPoints, other.m_ConvexHull.mMaxConvexRadius, other.m_ConvexHull.mMaterial);
						m_ConvexHull.SetDensity(other.m_ConvexHull.mDensity);
						m_ConvexHull.mUserData = other.m_ConvexHull.mUserData;
						m_ConvexHull.SetEmbedded();
						break;
					default:
						u_None = -1;
						break;
				}
				other.Terminate(shape);
			}

			~ShapeSettings_U() {}

			ColliderShape Init(const SphereCreateInfo& createInfo, const JPH::PhysicsMaterial* material) {
				using namespace JPH;
				new(&m_Sphere) SphereShapeSettings(createInfo.m_Radius, material);
				m_Sphere.SetEmbedded();
				return ColliderShape::Sphere;
			}

			ColliderShape Init(const BoxCreateInfo& createInfo, const JPH::PhysicsMaterial* material) {
				using namespace JPH;
				new(&m_Box) BoxShapeSettings(createInfo.m_HalfExtent, createInfo.m_ConvexRadius, material);
				m_Box.SetEmbedded();
				return ColliderShape::Box;
			}

			ColliderShape Init(const CapsuleCreateInfo& createInfo, const JPH::PhysicsMaterial* material) {
				using namespace JPH;
				new(&m_Capsule) CapsuleShapeSettings(createInfo.m_HalfHeight, createInfo.m_Radius, material);
				m_Capsule.SetEmbedded();
				return ColliderShape::Capsule;
			}

			ColliderShape Init(const ConvexHullCreateInfo& createInfo, const JPH::PhysicsMaterial* material) {
				using namespace JPH;
				assert(createInfo.m_Points);
				new(&m_ConvexHull) ConvexHullShapeSettings(createInfo.m_Points, createInfo.m_PointCount, createInfo.m_MaxConvexRadius, material);
				m_ConvexHull.SetEmbedded();
				return ColliderShape::ConvexHull;
			}

			void Terminate(ColliderShape shape) {
				switch (shape) {
					using S = ColliderShape;
					case S::None:
						return;
					case S::Sphere:
						(&m_Sphere)->~SphereShapeSettings();
						return;
					case S::Box:
						(&m_Box)->~BoxShapeSettings();
						return;
					case S::Capsule:
						(&m_Capsule)->~CapsuleShapeSettings();
						return;
					case S::ConvexHull:
						(&m_ConvexHull)->~ConvexHullShapeSettings();
						return;
				}
			}
		};

	public:

		struct SaveState {
			Vec3 m_Position;
			Quaternion m_Rotation;
		};

	private:

		using CreationSettings = JPH::BodyCreationSettings;

		Area& m_Area;
		PhysicsManager& m_PhysicsManager;

		String m_Name;
	
		Vec3 m_Position{};
		Quaternion m_Rotation{};
		Mat4 m_Transform;
		OrderedDictionary<RenderID, Mat4> m_RenderDataTransforms{};

		JPH::BodyID m_BodyID{};
		PhysicsLayer m_Layer = PhysicsLayer::Default;
		ColliderShape m_ColliderShape;
		ShapeSettings_U u_ShapeSettings;

		Body(Area& area, PhysicsManager& physicsManager, const char* name, const Vec3& position, const Quaternion& rotation)
			: m_Area(area), m_PhysicsManager(physicsManager),
				m_Name(name), m_Position(-position.x, position.y, -position.z), m_Rotation(rotation),
				m_ColliderShape(ColliderShape::None) {
			UpdateTransforms();
		}

		Body(const Body&) = delete;

		Body(Body&& other) noexcept
			: m_Area(other.m_Area), m_PhysicsManager(other.m_PhysicsManager), 
				m_Position(other.m_Position), m_Rotation(other.m_Rotation), m_Transform(other.m_Transform), 
				m_RenderDataTransforms(std::move(other.m_RenderDataTransforms)),
				m_BodyID(other.m_BodyID), m_ColliderShape(other.m_ColliderShape),
				u_ShapeSettings(std::move(other.u_ShapeSettings), m_ColliderShape) {}

		void Terminate() {
			using namespace JPH;
			u_ShapeSettings.Terminate(m_ColliderShape);
			if (!m_BodyID.IsInvalid()) {
				BodyInterface& interface = m_PhysicsManager.GetBodyInterface();
				interface.RemoveBody(m_BodyID);
				interface.DestroyBody(m_BodyID);
			}
			new(&m_BodyID) BodyID();
		}

		bool PhysInitialize(PhysicsLayer layer, const ColliderCreateInfo& colliderInfo, const JPH::PhysicsMaterial* material) {
			m_Layer = layer;
			switch (colliderInfo.m_ColliderShape) {
				using S = ColliderShape;
				case S::None:
					return false;
				case S::Sphere:
					return _PhysInitialize(colliderInfo.u_ShapeCreateInfo.m_Sphere, material);
				case S::Box:
					return _PhysInitialize(colliderInfo.u_ShapeCreateInfo.m_Box, material);
				case S::Capsule:
					return _PhysInitialize(colliderInfo.u_ShapeCreateInfo.m_Capsule, material);
				case S::ConvexHull:
					return _PhysInitialize(colliderInfo.u_ShapeCreateInfo.m_ConvexHull, material);
			}
			return false;
		}

		template<typename... Args>
		bool _PhysInitialize(Args&&... shapeArgs) {

			using namespace JPH;

			EMotionType motionType = EMotionType::Static;
			switch (m_Layer) {
				case PhysicsLayer::NonMoving:
					break;
				case PhysicsLayer::Moving:
					motionType = EMotionType::Dynamic;
					break;
				case PhysicsLayer::Kinematic:
					motionType = EMotionType::Kinematic;
					break;
				default:
					assert(false);
					return false;
			}

			m_ColliderShape = u_ShapeSettings.Init(std::forward<Args>(shapeArgs)...);
			JPH::ShapeSettings::ShapeResult shapeRes{};
			switch (m_ColliderShape) {
				using S = ColliderShape;
				case S::None:
					assert(false);
					return false;
				case S::Sphere:
					shapeRes = u_ShapeSettings.m_Sphere.Create();
					break;
				case S::Box:
					shapeRes = u_ShapeSettings.m_Box.Create();
					break;
				case S::Capsule:
					shapeRes = u_ShapeSettings.m_Capsule.Create();
					break;
				case S::ConvexHull:
					shapeRes = u_ShapeSettings.m_ConvexHull.Create();
					break;
			}

			if (shapeRes.HasError()) {
				PrintError(ErrorOrigin::Jolt,
					"failed to create shape (function JPH::ShapeSettings::Create in function Body::Initialize)!", VK_SUCCESS, shapeRes.GetError().c_str());
				return false;
			}
			JPH::ShapeRefC shape = shapeRes.Get();
			BodyCreationSettings settings(shape, m_Position, m_Rotation, motionType, m_Layer);
			m_BodyID = m_PhysicsManager.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
			return !m_BodyID.IsInvalid();
		}

	public:

		const String& GetName() const {
			return m_Name;
		}

		JPH::BodyID GetPhysicsID() const {
			return m_BodyID;
		}

		bool IsActive() {
			return m_PhysicsManager.GetBodyInterface().IsActive(m_BodyID);
		}

		Vec3 GetPosition() const {
			return { -m_Position.x, m_Position.y, -m_Position.z };
		}

		void SetPosition(const Vec3& position) {
			m_Position = { -position.x, position.y, -position.z };
			if (!m_BodyID.IsInvalid()) {
				m_PhysicsManager.GetBodyInterface().SetPosition(m_BodyID, m_Position, JPH::EActivation::Activate);
			}
			UpdateTransforms();
		}

		Quaternion GetRotation() const {
			return m_Rotation;
		}

		void SetRotation(const Quaternion& rotation) {
			m_Rotation = rotation;
			if (!m_BodyID.IsInvalid()) {
				m_PhysicsManager.GetBodyInterface().SetRotation(m_BodyID, m_Rotation, JPH::EActivation::Activate);
			}
			UpdateTransforms();
		}

		void SetPositionAndRotation(const Vec3& position, const Quaternion& rotation) {
			m_Position = { -position.x, position.y, -position.z };
			m_Rotation = rotation;
			if (!m_BodyID.IsInvalid()) {
				m_PhysicsManager.GetBodyInterface().SetPositionAndRotation(m_BodyID, m_Position, m_Rotation, JPH::EActivation::Activate);
			}
			UpdateTransforms();
		}

		Box<float> GetBoundingBox() const {
			return {};
		}

		//! @brief Adds force to body
		void AddForce(const Vec3& force) {
			assert(!m_BodyID.IsInvalid());
			m_PhysicsManager.GetBodyInterface().AddForce(m_BodyID, { -force.x, force.y, -force.z });
		}

		void AddTorque(const Vec3& torque) {
			assert(!m_BodyID.IsInvalid());
			m_PhysicsManager.GetBodyInterface().AddTorque(m_BodyID, torque);
		}

		//! @brief Moves body to target position/rotation in delta time seconds
		//! @param position: Target position
		//!	@param rotation: Target rotation
		//! @param seconds: Delta time in seconds
		void Move(const Vec3& position, const Quaternion& rotation, float deltaTime) {
			m_PhysicsManager.GetBodyInterface().MoveKinematic(m_BodyID, position, rotation, deltaTime);
		}

		SaveState GetSaveState() const {
			return {
				.m_Position = m_Position,
				.m_Rotation = m_Rotation,
			};
		}

		void LoadSaveState(const SaveState& state) {
			m_Position = state.m_Position;
			m_Rotation = state.m_Rotation;
			m_PhysicsManager.GetBodyInterface().SetPositionAndRotation(m_BodyID, m_Position, m_Rotation, JPH::EActivation::DontActivate);
			UpdateTransforms();
		}

	private:

		bool AddRenderDataTransform(RenderID ID, const Mat4& transform) {
			bool res = m_RenderDataTransforms.Insert(ID, transform);
			if (res) {
				bool a = UpdateRenderTransform(ID);
				assert(a);
			}
			return res;
		}

		bool RemoveRenderData(RenderID ID) {
			return m_RenderDataTransforms.Erase(ID);
		}

		void FixedUpdate() {
			using namespace JPH;
			assert(!m_BodyID.IsInvalid());
			BodyInterface& bodyInterface = m_PhysicsManager.GetBodyInterface();
			m_Position = bodyInterface.GetPosition(m_BodyID);
			m_Rotation = bodyInterface.GetRotation(m_BodyID);
			UpdateTransforms();
		}

		bool UpdateRenderTransform(RenderID ID);
		void UpdateTransforms();
	};

	enum AreaFlagBits {
		AreaFlag_NoSave = 1,
	};

	typedef uint32_t AreaFlags;

	class Area {

		friend class World;
		friend class Editor;

		friend class OrderedDictionary<ObjectID, Area>;

	public:

		class SaveState {

			friend class Area;
			friend class World;

		private:

			static constexpr uint32_t body_alloc_size = sizeof(ObjectID) + sizeof(Body::SaveState);
			
			uint32_t m_BodyCount = 0;
			ObjectID* m_BodyIDs = nullptr;
			Body::SaveState* m_BodySaveStates = nullptr;

			SaveState() {}

			~SaveState() {
				m_BodyCount = 0;
				m_BodyIDs = nullptr;
				m_BodySaveStates = nullptr;
			}

			void Initialize(const Area& area, ObjectID* bodyIDsPtr, Body::SaveState* bodySaveStatesPtr) {
				assert(bodyIDsPtr && bodySaveStatesPtr);
				m_BodyCount = area.m_Bodies.Size();
				m_BodyIDs = bodyIDsPtr; m_BodySaveStates = bodySaveStatesPtr;
				memcpy(m_BodyIDs, area.m_Bodies.GetKeys(), m_BodyCount * sizeof(ObjectID));
				const Body* bodies = area.m_Bodies.GetValues();
				for (uint32_t i = 0; i < m_BodyCount; i++) {
					m_BodySaveStates[i] = bodies[i].GetSaveState();
				}
			}
		};

		World& m_World;

		ObjectID AddBody(const char* name, const Vec3& body, const Quaternion& rotation, PhysicsLayer physicsLayer,
			const Body::ColliderCreateInfo& colliderInfo, const JPH::PhysicsMaterial* physicsMaterial);
		bool RemoveBody(ObjectID ID);

		Body* GetBody(ObjectID ID) {
			return m_Bodies.Find(ID);
		}

	private:

		OrderedDictionary<ObjectID, Body> m_Bodies{};
		
		Area(World& world, AreaFlags flags) noexcept
			: m_World(world) {}

		void LoadSaveState(const SaveState& saveState) {
			for (uint32_t i = 0; i < saveState.m_BodyCount; i++) {
				Body* body = m_Bodies.Find(saveState.m_BodyIDs[i]);
				if (!body) {
					PrintError(ErrorOrigin::Engine,
						"couldn't find body from area (in function Area::LoadSaveState)!");
					continue;
				}
				body->LoadSaveState(saveState.m_BodySaveStates[i]);
			}
		}

		void Terminate() {
			for (Body& body : m_Bodies) {
				body.Terminate();
			}
			m_Bodies.Clear();
		}
	};	

	class UnidirectionalLight {

		friend class World;

	public:

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

		const ObjectID m_ObjectID;

	private:

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

		UnidirectionalLight(World& world, ObjectID objectID, Type type, Vec2_T<uint32_t> shadowMapResolution);

		UnidirectionalLight(const UnidirectionalLight&) = delete;
		UnidirectionalLight(UnidirectionalLight&&) = delete;

		void Initialize(const Mat4& projection, const Mat4& view, const Vec3& color);

		void Terminate();

		FragmentBufferDirectional& GetDirectionalBuffer() {
			assert(m_FragmentMap);
			return *(FragmentBufferDirectional*)(m_FragmentMap);
		}

		VkDeviceSize GetFragmentBufferSize() const {
			return m_Type == Type::Directional ? sizeof(FragmentBufferDirectional) : sizeof(FragmentBufferSpot);
		}

	public:

		void SetViewMatrix(const Mat4& matrix) {
			if (m_Type == Type::Directional) {
				m_ViewMatrices.m_View = matrix;
				auto& buf = GetDirectionalBuffer();
				buf.m_ViewMatrix = m_ViewMatrices.GetLightViewMatrix();
				buf.m_Direction = m_ViewMatrices.GetDirection();
			}
			else {
			}
		}

		void DepthDraw(const Renderer::DrawData& drawData) const;

		void SwapchainCreateCallback(uint32_t imageCount, VkCommandBuffer commandBuffer);
	};

	inline bool LoadMesh(const String& fileName, StaticMesh& outMesh) {
		FILE* fileStream = fopen(fileName.CString(), "r");
		if (!fileStream) {
			PrintError(ErrorOrigin::FileParsing, 
				"failed to open mesh file (function fopen in function LoadMesh)!");
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"file that failed to open {}", fileName.CString());
			return false;
		}
		MeshFileType type = GetMeshFileType(fileName);
		if (type == MeshFileType::Obj) {
			Obj obj{};
			DynamicArray<Vertex> vertices{};
			DynamicArray<uint32_t> indices{};
			if (!obj.Load(fileStream)) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to load obj mesh file (function Obj::Load in function LoadMesh)");
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"mesh file that failed to load {}", fileName.CString());
				fclose(fileStream);
				return false;
			}
			fclose(fileStream);
			if (!obj.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, vertices, indices)) {
				PrintError(ErrorOrigin::FileParsing, 
					"failed to load obj mesh file (function Obj::Load in function LoadMesh)");
				return false;
			}
			if (!outMesh.CreateBuffers(vertices.Size(), vertices.Data(), indices.Size(), indices.Data())) {
				PrintError(ErrorOrigin::Engine, 
					"failed to create buffers for static mesh (function Obj::Load in function LoadMesh)");
				return false;
			}
		}
		return true;
	}

	class Metadata {
	public:

		enum class Type {
			Mesh = 1,
			Texture = 2,
			MaxEnum = 3,
		};

		union Data_U {

			int m_None;
			StaticMesh m_Mesh;
			StaticTexture m_Texture;

			Data_U(Renderer& renderer, Type type) {
				switch (type) {
					case Type::Mesh:
						new(&m_Mesh) StaticMesh(renderer);
						break;
					case Type::Texture:
						new(&m_Texture) StaticTexture(renderer);
						break;
					default:
						m_None = -1;
						PrintError(ErrorOrigin::AssetManager,
							"invalid metadata type (in Metadata constructor)!");
						break;
				}
			}

			Data_U(Data_U&& other, Type type) {
				switch (type) {
					case Type::Mesh:
						new(&m_Mesh) StaticMesh(std::move(other.m_Mesh));
						break;
					case Type::Texture:
						new(&m_Texture) StaticTexture(std::move(other.m_Texture));
						break;
					default:
						m_None = -1;
						PrintError(ErrorOrigin::AssetManager,
							"invalid metadata type (in Metadata constructor)!");
						break;
				}
			}

			~Data_U() {}
		};

		const uint64_t m_ID;

	private:

		Type m_Type;
		Data_U u_Data;

	public:

		Metadata(uint64_t ID, Type type, Renderer& renderer) 
			: m_ID(ID), m_Type(type), u_Data(renderer, type) {}

		Metadata(const Metadata&) = delete;

		Metadata(Metadata&& other) : m_ID(other.m_ID), m_Type(other.m_Type), u_Data(std::move(other.u_Data), m_Type) {}

		bool IsNull() const {
			switch (m_Type) {
				case Type::Mesh:
					return !u_Data.m_Mesh.IsNull();
				case Type::Texture:
					return !u_Data.m_Texture.IsNull();
				default:
					return false;
			}
		}

		Type GetType() {
			return m_Type;
		}

		bool Load(const Obj& obj) {
			assert(m_Type == Type::Mesh);
			DynamicArray<Vertex> vertices;
			DynamicArray<uint32_t> indices;
			if (!obj.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, 
				vertices, indices)) {
				PrintError(ErrorOrigin::AssetManager,
					"failed to get construct mesh from obj (function Obj::GetMesh in function Metadata::Load)!");
				return false;
			}
			if (!u_Data.m_Mesh.CreateBuffers(vertices.Size(), vertices.Data(), indices.Size(), indices.Data())) {
				PrintError(ErrorOrigin::Renderer,
					"failed to create static mesh (function Obj::GetMesh in function Metadata::Load)!");
				return false;
			}
			return true;
		}

		void Deload() {
			switch (m_Type) {
				case Type::Mesh:
					u_Data.m_Mesh.Terminate();
					break;
				case Type::Texture:
					u_Data.m_Texture.Terminate();
					break;
				default:
					break;
			}
		}

		bool GetMeshData(MeshData& out) {
			if (m_Type != Type::Mesh) {
				PrintError(ErrorOrigin::AssetManager,
					"attempting to get mesh data from metadata of non-mesh type (in function Metadata::GetMeshData)!");
				return false;
			}
			if (u_Data.m_Mesh.IsNull()) {
				PrintError(ErrorOrigin::AssetManager,
					"attempting to get mesh data from metadata that's null (int function Metadata::GetMeshData)!");
				return false;
			}
			out = u_Data.m_Mesh.GetMeshData();
			return true;
		}
	};

	class AssetManager {

		friend class Engine;

	private:

		Renderer& m_Renderer;
		String m_ProjectName;
		uint64_t m_NextMetaID;
		Dictionary<Metadata> m_Metadatas{};

		AssetManager(const String& projectName, Renderer& renderer) 
			: m_Renderer(renderer), m_ProjectName(projectName) {
			String projectFileName = projectName + ".meta";
			FILE* metafile = fopen(projectFileName.CString(), "r");
			if (!metafile) {
				metafile = fopen(projectFileName.CString(), "w");
				if (fwrite(projectName.CString(), sizeof(char), projectName.Length(), metafile) != projectName.Length() ||
					!fwrite("\n", sizeof(char), 1, metafile)) {
					PrintError(ErrorOrigin::FileWriting, 
						"failed to write to project metafile (function fwrite in AssetManager constructor)!");
				}
			}
			else {
				String line{};
				FileHandler::GetLine(metafile, line);
				if (line != projectName) {
					PrintWarning("loading meta data from a different project (in AssetManager constructor)!");
				}
				if (FileHandler::Skip(metafile, Array<char, 1> { '[' }) != EOF) {
					char c = FileHandler::SkipLine(metafile);
					if (c == EOF) {
						PrintError(ErrorOrigin::FileParsing, 
							"missing ']' when loading project metadata (in AssetManager constructor)");
					}
					while (true) {
						line.Clear();
						if (FileHandler::GetLine(metafile, line) == EOF) {
							PrintError(ErrorOrigin::FileParsing, 
								"missing ']' when loading project metadata (in AssetManager constructor)");
						}
						if (line[0] == ']') {
							break;
						}
						FILE* assetMetafile = fopen(line.CString(), "r");
						if (!assetMetafile) {
							PrintError(ErrorOrigin::FileParsing, 
								"failed to open metafile (function fopen in AssetManager construcor)!");
							continue;
						}
						uint64_t ID;
						if (!LoadMetadata(assetMetafile, ID)) {
							PrintError(ErrorOrigin::FileParsing,
								"failed to load metadata (function AssetManager::LoadMetadata in AssetManager constructor)!");
							fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
								"metafile that failed to load: {}", line.CString());
							continue;
						}
						m_NextMetaID = Max(m_NextMetaID, ID + 1);
					}
				}
			}
			fclose(metafile);
		}

		bool LoadMetadata(FILE* metafile, uint64_t& outID) {
			if (!metafile) {
				return false;
			}
			uint64_t ID;
			uint32_t type;
			if (fscanf(metafile, "ID:%llu\nType:%u\n", &ID, &type) != 2) {
				PrintError(ErrorOrigin::FileParsing,
					"invalid metadata file (in function LoadMetadata)!");
				return false;
			}
			String assetFileName{};
			FileHandler::GetLine(metafile, assetFileName);
			m_Metadatas.Emplace(assetFileName, ID, (Metadata::Type)type, m_Renderer);
			return true;
		}

	public:

		bool LoadAsset(const String& fileName, MeshData& outMeshData) {
			auto data = m_Metadatas.Find(fileName);
			if (data) {
				if (data->GetType() != Metadata::Type::Mesh) {
					PrintError(ErrorOrigin::AssetManager,
						"attempting to load mesh from a file that isn't a mesh (in function AssetManager::LoadAsset)!");
					return false;
				}
				if (!data->IsNull()) {
					if (!data->GetMeshData(outMeshData)) {
						PrintError(ErrorOrigin::AssetManager,
							"failed to get mesh data from metadata (function Metadata::GetMeshData in function AssetManager::LoadAsset)!");
						return false;
					}
					return true;
				}
			}
			if (!data) {
				data = m_Metadatas.Emplace(fileName, m_NextMetaID++, Metadata::Type::Mesh, m_Renderer);	
			}
			MeshFileType type = GetMeshFileType(fileName);
			if (type == MeshFileType::Unrecognized) {
				PrintError(ErrorOrigin::AssetManager,
					"attempting to load mesh asset of unknown type (function GetMeshFileType in function AssetManager::LoadAsset)!");
				return false;
			}
			if (type == MeshFileType::Obj) {
				FILE* fileStream = fopen(fileName.CString(), "r");
				if (!fileStream) {
					PrintError(ErrorOrigin::AssetManager,
						"failed to open mesh asset file (function fopen in function AssetManager::LoadAsset)!");
					m_Metadatas.Erase(fileName);
					return false;
				}
				Obj obj{};
				bool res = obj.Load(fileStream);
				fclose(fileStream);
				if (!res) {
					PrintError(ErrorOrigin::Engine,
						"failed to load obj file (function Obj::Load in function AssetManager::LoadAsset)!");
					m_Metadatas.Erase(fileName);
					return false;
				}
				if (!data->Load(obj)) {
					PrintError(ErrorOrigin::AssetManager,
						"failed to load mesh asset from metadata (function Metadata::Load in function AssetManager::LoadAsset)!");
					m_Metadatas.Erase(fileName);
					return false;
				}
				assert(data->GetMeshData(outMeshData));
				return true;
			}
			return false;
		}	
	};

	class Editor;

	class World {

		friend class Engine;
		friend class Area;
		friend class Editor;

		friend class UnidirectionalLight;

	public:

		class Entity {
		public:
			virtual void Update(World& world) = 0;
			virtual void Terminate() = 0;
		};

		struct DebugRenderData {
			MeshData m_MeshData{};
			Mat4 m_Transform{};
			Vec4 m_Color{};
			Optional<uint32_t> m_ID{};
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

		class SaveState {

			friend class World;

		private:

			static constexpr uint32_t area_alloc_size = sizeof(ObjectID) + sizeof(Area::SaveState);

			uint8_t* m_Data = nullptr;

			uint32_t m_AreaCount = 0;
			ObjectID* m_AreaIDs = nullptr;
			Area::SaveState* m_AreaSaveStates = nullptr;

		public:

			SaveState() {}

			SaveState(const SaveState&) = delete;

			SaveState(SaveState&& other)
				: m_Data(other.m_Data), m_AreaCount(other.m_AreaCount),
					m_AreaIDs(other.m_AreaIDs), m_AreaSaveStates(other.m_AreaSaveStates) {
				other.m_Data = nullptr;
				other.m_AreaCount = 0;
				other.m_AreaIDs = nullptr;
				other.m_AreaSaveStates = nullptr;
			}

			~SaveState() {
				Clear();
			}

			void Clear() {
				free(m_Data);
				m_Data = nullptr;
				m_AreaCount = 0;
				m_AreaIDs = nullptr;
				m_AreaSaveStates = nullptr;
			}

			void Initialize(const World& world) {
				if (m_Data) {
					Clear();
				}
				m_AreaCount = world.m_Areas.Size();
				uint32_t bodyRegionStart = area_alloc_size * m_AreaCount;
				const Area* areas = world.m_Areas.GetValues();
				uint32_t allocSize = 0;
				for (uint32_t i = 0; i < m_AreaCount; i++) {
					allocSize += area_alloc_size + Area::SaveState::body_alloc_size * areas[i].m_Bodies.Size();
				}
				m_Data = (uint8_t*)malloc(allocSize);
				assert(m_Data);
				m_AreaIDs = (ObjectID*)m_Data;
				uint32_t areaIDRegSize = sizeof(ObjectID) * m_AreaCount;
				m_AreaSaveStates = (Area::SaveState*)(m_Data + areaIDRegSize);
				memcpy(m_AreaIDs, world.m_Areas.GetKeys(), areaIDRegSize);
				for (uint32_t i = 0; i < m_AreaCount; i++) {
					const Area& area = areas[i];
					Area::SaveState& saveState = m_AreaSaveStates[i];
					uint32_t bodyCount = area.m_Bodies.Size();
					uint8_t* p = m_Data + bodyRegionStart;
					saveState.Initialize(area, (ObjectID*)p, (Body::SaveState*)(p + (sizeof(ObjectID) * bodyCount)));
					bodyRegionStart += Area::SaveState::body_alloc_size * bodyCount;
				}
			}
		};

	private:

		struct MouseHitBuffer {
			float m_Depth;
			uint32_t m_ID;
		};

	public:

		static constexpr float default_camera_fov = Math::pi / 4.0f;
		static constexpr float default_camera_near = 0.1f;
		static constexpr float default_camera_far = 100.0f;

		AssetManager& m_AssetManager;
		PhysicsManager& m_PhysicsManager;
		Renderer& m_Renderer;

	private:

		Vec2_T<uint32_t> m_RenderResolution{};

		ObjectID m_NextObjectID = 0;
		RenderID m_NextRenderID = 0;
		VkDescriptorSet m_NullTextureDescriptorSet = VK_NULL_HANDLE;
		DynamicArray<Entity*> m_Entities{};
		OrderedDictionary<ObjectID, Area> m_Areas{};
		Vec3 m_GameCameraPosition{};
		Quaternion m_GameCameraRotation{};
		float m_EditorCameraSensitivity = Math::pi / 2;
		float m_EditorCameraSpeed = 5.0f;
		static constexpr float editor_min_camera_speed = 1.0f;
		static constexpr float editor_max_camera_speed = 20.0f;
		Vec3 m_EditorCameraPosition{};
		Vec2 m_EditorCameraRotations{};
		uint32_t m_InvalidMouseHitID = UINT32_MAX;
		uint32_t m_DebugMouseHitID = m_InvalidMouseHitID;

		MouseHitBuffer** m_DebugMouseHitBufferMaps = nullptr;

		bool m_WasInEditorView{};
		CameraMatricesBuffer* m_CameraMatricesMap = nullptr;
		VkImage* m_DiffuseImages = nullptr;
		VkImage* m_PositionAndMetallicImages = nullptr;
		VkImage* m_NormalAndRougnessImages = nullptr;
		VkImageView* m_DiffuseImageViews = nullptr;
		VkImageView* m_PositionAndMetallicImageViews = nullptr;
		VkImageView* m_NormalAndRougnessImageViews = nullptr;
		VkImageView* m_DepthImageViews = nullptr;
		pipelines::World m_Pipelines{};
		CameraMatricesBuffer m_EditorCamera{};
		CameraMatricesBuffer m_GameCamera{};
		OrderedDictionary<RenderID, WorldRenderData> m_RenderDatas{};
		VkDescriptorSet m_CameraMatricesDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet* m_RenderPBRImagesDescriptorSets = nullptr;
		VkImageView* m_DebugDepthImageViews = nullptr;
		VkDescriptorSet* m_DebugMouseHitDescriptorSets = nullptr;
		DynamicArray<DebugRenderData> m_DebugWireRenderDatas{};
		DynamicArray<DebugRenderData> m_DebugSolidRenderDatas{};
		UnidirectionalLight m_DirectionalLight;
		MeshData m_StaticQuadMeshDataPBR{};

		VkDescriptorSet m_DefaultAlbedoDescriptorSet{};
		VkFormat m_ColorImageResourcesFormat{};
		uint32_t m_FramesInFlight = 0;
		uint8_t* m_FramesInFlightData = nullptr;
		VkImage* m_DepthImages = nullptr;
		VkImage* m_DebugDepthImages = nullptr;
		VkDeviceMemory* m_DiffuseImagesMemory = nullptr;
		VkDeviceMemory* m_PositionAndMetallicImagesMemory = nullptr;
		VkDeviceMemory* m_NormalAndRougnessImagesMemory = nullptr;
		VkDeviceMemory* m_DepthImagesMemory = nullptr;
		VkDeviceMemory* m_DebugDepthImagesMemory = nullptr;
		VkDescriptorPool m_CameraMatricesDescriptorPool = VK_NULL_HANDLE;
		VkDescriptorPool m_RenderPBRImagesDescriptorPool = VK_NULL_HANDLE;
		VkDescriptorPool m_DebugMouseHitDescriptorPool = VK_NULL_HANDLE;
		VkSampler m_ColorResourceImageSampler{};
		Renderer::Buffer m_CameraMatricesBuffer;
		Renderer::Buffer* m_DebugMouseHitBuffers = nullptr;
		VkDescriptorPool m_DefaultTextureDescriptorPool{};
		StaticTexture m_DefaultAlbedoTexture;
		VkImageView m_DefaultAlbedoImageView = VK_NULL_HANDLE;

		World(AssetManager& assetManager, PhysicsManager& physicsManager, Renderer& renderer) 
			: m_AssetManager(assetManager), m_Renderer(renderer), m_PhysicsManager(physicsManager),
				m_DirectionalLight(*this, m_NextObjectID++, UnidirectionalLight::Type::Directional, { 1024, 1024 }),
				m_CameraMatricesBuffer(m_Renderer), m_DefaultAlbedoTexture(m_Renderer) {}

		World(const World&) = delete;
		World(World&&) = delete;

		void Initialize(const StaticMesh& quadMesh2D);

		void Terminate() {
			for (Area& area : m_Areas) {
				area.Terminate();
			}
			m_CameraMatricesBuffer.Terminate();
			m_Renderer.DestroyDescriptorPool(m_CameraMatricesDescriptorPool);
			m_Renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
			m_Renderer.DestroyDescriptorPool(m_DefaultTextureDescriptorPool);
			m_Renderer.DestroyImageView(m_DefaultAlbedoImageView);
			m_DefaultAlbedoTexture.Terminate();
			m_Renderer.DestroySampler(m_ColorResourceImageSampler);
			m_Pipelines.Terminate(m_Renderer);
			DestroyImageResources();
			DestroyDebugResources();
			DeallocateFramesInFlightData();
			m_DirectionalLight.Terminate();
		}

		void AllocateFramesInFlightData(uint32_t framesInFlight) {

			static constexpr auto get_p = [](uint8_t* p, uint32_t& ioPos, uint32_t size, uint32_t count) -> void* {
				void* r = p + ioPos;
				const uint32_t advance = size * count;
				memset(r, 0, advance);
				ioPos += advance;
				return r;
			};

			static constexpr uint32_t frame_image_count = 5;

			if (framesInFlight != m_FramesInFlight) {

				const uint32_t alloc_size =
					sizeof(VkImage) * frame_image_count * framesInFlight +
					sizeof(VkDeviceMemory) * frame_image_count * framesInFlight +
					sizeof(VkImageView) * frame_image_count * framesInFlight +
					sizeof(VkDescriptorSet) * framesInFlight * 2 +
					sizeof(MouseHitBuffer*) * framesInFlight +
					sizeof(Renderer::Buffer) * framesInFlight;

				free(m_FramesInFlightData);
				m_FramesInFlightData = (uint8_t*)malloc(alloc_size);

				uint8_t* p = m_FramesInFlightData;
				uint32_t pos = 0;
				m_DiffuseImages = (VkImage*)							get_p(p, pos, sizeof(VkImage), framesInFlight);
				m_PositionAndMetallicImages = (VkImage*)				get_p(p, pos, sizeof(VkImage), framesInFlight);
				m_NormalAndRougnessImages = (VkImage*)					get_p(p, pos, sizeof(VkImage), framesInFlight);
				m_DiffuseImageViews = (VkImageView*)					get_p(p, pos, sizeof(VkImageView), framesInFlight);
				m_PositionAndMetallicImageViews = (VkImageView*)		get_p(p, pos, sizeof(VkImageView), framesInFlight);
				m_NormalAndRougnessImageViews = (VkImageView*)			get_p(p, pos, sizeof(VkImageView), framesInFlight);
				m_DepthImageViews = (VkImageView*)						get_p(p, pos, sizeof(VkImageView), framesInFlight);
				m_RenderPBRImagesDescriptorSets = (VkDescriptorSet*)	get_p(p, pos, sizeof(VkDescriptorSet), framesInFlight);
				m_DebugDepthImageViews = (VkImageView*)					get_p(p, pos, sizeof(VkImageView), framesInFlight);
				m_DebugMouseHitDescriptorSets = (VkDescriptorSet*)		get_p(p, pos, sizeof(VkDescriptorSet), framesInFlight);
				m_DepthImages = (VkImage*)								get_p(p, pos, sizeof(VkImage), framesInFlight);
				m_DebugDepthImages = (VkImage*)							get_p(p, pos, sizeof(VkImage), framesInFlight);
				m_DiffuseImagesMemory = (VkDeviceMemory*)				get_p(p, pos, sizeof(VkDeviceMemory), framesInFlight);
				m_PositionAndMetallicImagesMemory = (VkDeviceMemory*)	get_p(p, pos, sizeof(VkDeviceMemory), framesInFlight);
				m_NormalAndRougnessImagesMemory = (VkDeviceMemory*)		get_p(p, pos, sizeof(VkDeviceMemory), framesInFlight);
				m_DepthImagesMemory = (VkDeviceMemory*)					get_p(p, pos, sizeof(VkDeviceMemory), framesInFlight);
				m_DebugDepthImagesMemory = (VkDeviceMemory*)			get_p(p, pos, sizeof(VkDeviceMemory), framesInFlight);
				m_DebugMouseHitBufferMaps = (MouseHitBuffer**)			get_p(p, pos, sizeof(MouseHitBuffer*), framesInFlight);
				m_DebugMouseHitBuffers = (Renderer::Buffer*)			get_p(p, pos, sizeof(Renderer::Buffer), framesInFlight);
				m_FramesInFlight = framesInFlight;

				for (uint32_t i = 0; i < m_FramesInFlight; i++) {
					new(m_DebugMouseHitBuffers + i) Renderer::Buffer(m_Renderer);
				}
			}
		}

		void DeallocateFramesInFlightData() {
			free(m_FramesInFlightData);
			m_FramesInFlightData = nullptr;
			m_FramesInFlight = 0;
		}

		void DestroyImageResources() {
			for (uint32_t i = 0; i < m_FramesInFlight; i++) {
				m_Renderer.DestroyImageView(m_DepthImageViews[i]);
				m_Renderer.DestroyImageView(m_DiffuseImageViews[i]);
				m_Renderer.DestroyImageView(m_PositionAndMetallicImageViews[i]);
				m_Renderer.DestroyImageView(m_NormalAndRougnessImageViews[i]);
				m_Renderer.DestroyImageView(m_DebugDepthImageViews[i]);
				m_Renderer.DestroyImage(m_DepthImages[i]);
				m_Renderer.DestroyImage(m_DiffuseImages[i]);
				m_Renderer.DestroyImage(m_PositionAndMetallicImages[i]);
				m_Renderer.DestroyImage(m_NormalAndRougnessImages[i]);
				m_Renderer.DestroyImage(m_DebugDepthImages[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_DiffuseImagesMemory[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_PositionAndMetallicImagesMemory[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_NormalAndRougnessImagesMemory[i]);
				m_Renderer.FreeVulkanDeviceMemory(m_DebugDepthImagesMemory[i]);
			}
		}

		void DestroyDebugResources() {
			m_Renderer.DestroyDescriptorPool(m_DebugMouseHitDescriptorPool);
			for (uint32_t i = 0; i < m_FramesInFlight; i++) {
				m_DebugMouseHitBuffers[i].Terminate();
			}
		}

		void CreateDebugResources() {
			assert(m_FramesInFlight <= 5);
			VkDescriptorPoolSize poolSizes[5];
			for (uint32_t i = 0; i < m_FramesInFlight; i++) {
				if (!m_DebugMouseHitBuffers[i].Create(sizeof(MouseHitBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create debug mouse hit buffer (function Renderer::Buffer::Create in function World::CreateDebugResources)!");
				}
				if (!m_DebugMouseHitBuffers[i].MapMemory(0, sizeof(MouseHitBuffer), (void**)&m_DebugMouseHitBufferMaps[i])) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to map debug mouse hit buffer (function Renderer::Buffer::Map in function World::CreateDebugResources)!");
				}
				poolSizes[i] = {
					.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.descriptorCount = 1,
				};
			}
			m_DebugMouseHitDescriptorPool = m_Renderer.CreateDescriptorPool(0, m_FramesInFlight, m_FramesInFlight, poolSizes);
			if (m_DebugMouseHitDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create debug mouse hit descriptor pool (function Renderer::CreateDescriptorPool in function World::CreateDebugResources)!");
			}
			VkDescriptorSetLayout setLayouts[5];
			for (uint32_t i = 0; i < m_FramesInFlight; i++) {
				setLayouts[i] = m_Pipelines.m_DebugMouseHitDescriptorSetLayout;
			}
			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_DebugMouseHitDescriptorPool,
					m_FramesInFlight, setLayouts, m_DebugMouseHitDescriptorSets)) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to allocate debug mouse hit descriptor sets (function Renderer::CreateDescriptorPool in function World::CreateDebugResources)!");
			}
			VkWriteDescriptorSet writes[5];
			VkDescriptorBufferInfo bufferInfos[5];
			for (uint32_t i = 0; i < m_FramesInFlight; i++) {
				bufferInfos[i] = {
					.buffer = m_DebugMouseHitBuffers[i].m_Buffer,
					.offset = 0,
					.range = sizeof(MouseHitBuffer),
				};
				writes[i] = m_Renderer.GetDescriptorWrite(nullptr, 0, m_DebugMouseHitDescriptorSets[i],
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfos[i]);
			}
			m_Renderer.UpdateDescriptorSets(m_FramesInFlight, writes);
		}

	public:

		void SetGameCameraView(const Vec3& position, const Quaternion& rotation) {
			m_GameCameraPosition = position;
			m_GameCameraRotation = rotation;
			m_GameCamera.m_View = rotation.AsMat4();
		}

		const Vec3& GetCameraPosition() {
			if (m_WasInEditorView) {
				return m_EditorCameraPosition;
			}
			return m_GameCameraPosition;
		}

		uint64_t AddArea(AreaFlags flags) {
			Area* area = m_Areas.Emplace(m_NextObjectID, *this, flags);
			assert(area);
			return m_NextObjectID++;
		}

		Area* GetArea(ObjectID ID) {
			return m_Areas.Find(ID);
		}

		Entity* AddEntity(Entity* entity) {
			assert(entity);
			m_Entities.PushBack(entity);
			return entity;
		}

		bool RemoveEntity(Entity* entity) {
			assert(entity);
			auto iter = m_Entities.begin();
			auto end = m_Entities.end();
			for (; iter != end; iter++) {
				if (*iter == entity) {
					break;
				}
			}
			if (iter != end) {
				entity->Terminate();
				m_Entities.Erase(iter);
				return true;
			}
			return false;
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
					&m_Pipelines.m_TextureDescriptorSetLayoutPBR, &out.m_DescriptorSet)) {
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

		void DestroyTextureMap(const TextureMap& map) const {
			m_Renderer.DestroyDescriptorPool(map.m_DescriptorPool);
			m_Renderer.DestroyImageView(map.m_ImageView);
		}

		RenderID AddRenderData(WorldRenderDataFlags flags, Body& body, const Mat4& transform, const MeshData& meshData) {
			m_RenderDatas.Emplace(m_NextRenderID, flags, transform, meshData);
			bool a = body.AddRenderDataTransform(m_NextRenderID, transform);
			assert(a);
			return m_NextRenderID++;
		}

		WorldRenderData* GetRenderData(uint64_t ID) {
			return m_RenderDatas.Find(ID);
		}

		void RenderDebugWireMesh(const MeshData& mesh, const Mat4& transform, const Vec4& color, Optional<uint32_t> ID = {}) {
			m_DebugWireRenderDatas.EmplaceBack(mesh, transform, color, ID);
		}

		void RenderDebugSolidMesh(const MeshData& mesh, const Mat4& transform, const Vec4& color, Optional<uint32_t> ID = {}) {
			m_DebugSolidRenderDatas.EmplaceBack(mesh, transform, color, ID);
		}

		void LoadSaveState(const SaveState& saveState) {
			for (uint32_t i = 0; i < saveState.m_AreaCount; i++) {
				Area* area = m_Areas.Find(saveState.m_AreaIDs[i]);
				if (!area) {
					PrintError(ErrorOrigin::Engine,
						"couldn't find area from world (in function World::LoadSaveState)!");
					continue;
				}
				area->LoadSaveState(saveState.m_AreaSaveStates[i]);
			}
		}

		uint32_t GetMouseHitID() const {
			return m_DebugMouseHitID;
		}	

	private:

		void DefineInvalidMouseHitID(uint32_t ID) {
			m_InvalidMouseHitID = ID;
		}

		void LogicUpdate() {
			for (Entity* entity : m_Entities) {
				entity->Update(*this);
			}
		}

		void FixedUpdate() {
			for (Area& area : m_Areas) {
				for (Body& body : area.m_Bodies) {
					if (body.IsActive()) {
						body.FixedUpdate();
					}
				}
			}
		}

		void UpdateGameCamera() {
			Mat4 rotMat = m_GameCameraRotation.AsMat4();
			Vec3 front = rotMat * Vec3::Forward();
			Vec3 up = rotMat * Vec3::Up();
			Vec3 right = Cross(up, front).Normalized();
			m_EditorCamera.m_View[0].x = right.x;
			m_EditorCamera.m_View[1].x = right.y;
			m_EditorCamera.m_View[2].x = right.z;
			m_EditorCamera.m_View[0].y = up.x;
			m_EditorCamera.m_View[1].y = up.y;
			m_EditorCamera.m_View[2].y = up.z;
			m_EditorCamera.m_View[0].z = front.x;
			m_EditorCamera.m_View[1].z = front.y;
			m_EditorCamera.m_View[2].z = front.z;
			Vec3 negPos = -m_GameCameraPosition;
			m_EditorCamera.m_View[3] = {
				Dot(right, negPos),
				Dot(up, negPos),
				Dot(front, negPos),
				1.0f,
			};
		}

		void UpdateEditorCamera() {
			Mat4 rotMat 
				= (Quaternion::AxisRotation(Vec3::Right(), m_EditorCameraRotations.x) 
					* Quaternion::AxisRotation(Vec3::Up(), m_EditorCameraRotations.y)).
					AsMat4();
			Vec3 front = rotMat * Vec3::Forward();
			Vec3 up = rotMat * Vec3::Up();
			Vec3 right = Cross(up, front).Normalized();
			m_EditorCamera.m_View[0].x = right.x;
			m_EditorCamera.m_View[1].x = right.y;
			m_EditorCamera.m_View[2].x = right.z;
			m_EditorCamera.m_View[0].y = up.x;
			m_EditorCamera.m_View[1].y = up.y;
			m_EditorCamera.m_View[2].y = up.z;
			m_EditorCamera.m_View[0].z = front.x;
			m_EditorCamera.m_View[1].z = front.y;
			m_EditorCamera.m_View[2].z = front.z;
			Vec3 negPos = -m_EditorCameraPosition;
			negPos.y *= -1;
			m_EditorCamera.m_View[3] = {
				Dot(right, negPos),
				Dot(up, negPos),
				Dot(front, negPos),
				1.0f,
			};
		}

		void EditorUpdate(GLFWwindow* glfwWindow) {

			using Key = Input::Key;
			using MouseButton = Input::MouseButton;

			Vec2 deltaScrollOffset = Input::GetDeltaScrollOffset();
			if (deltaScrollOffset != 0.0f) {
				m_EditorCameraSpeed = Clamp(m_EditorCameraSpeed + 1000 * deltaScrollOffset.y * Time::DeltaTime(), editor_min_camera_speed, editor_max_camera_speed);
			}

			Vec3 movementVector {
				Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
				Input::ReadKeyValue(Key::Space) - Input::ReadKeyValue(Key::LeftShift),
				Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S),
			};

			bool mouseHeld = Input::WasMouseButtonHeld(MouseButton::Right);
			bool moved = movementVector != 0.0f;

			if (mouseHeld || moved) {
				if (mouseHeld) {
					Vec2_T<double> deltaCursorPos = Input::GetDeltaMousePosition();
					m_EditorCameraRotations += Vec2(deltaCursorPos.y, deltaCursorPos.x) * (m_EditorCameraSensitivity * Time::DeltaTime());
					m_EditorCameraRotations.x = fmod(m_EditorCameraRotations.x, 2 * Math::pi);
					m_EditorCameraRotations.y = fmod(m_EditorCameraRotations.y, 2 * Math::pi);
				}
				Quaternion rightRot = Quaternion::AxisRotation(Vec3::Right(), m_EditorCameraRotations.x);
				Quaternion upRot = Quaternion::AxisRotation(Vec3::Up(), m_EditorCameraRotations.y);
				if (moved) {
					float frameSpeed = m_EditorCameraSpeed * Time::DeltaTime();
					float y = movementVector.y * frameSpeed / 2;
					movementVector.y = 0.0f;
					movementVector = movementVector.Normalized() * frameSpeed;
					movementVector = upRot.AsMat4() * movementVector;
					m_EditorCameraPosition += movementVector + Vec3::Up(y);
				}
				Mat4 rotMat = (rightRot * upRot).AsMat4();
				Vec3 front = rotMat * Vec3::Forward();
				Vec3 up = rotMat * Vec3::Up();
				Vec3 right = Cross(up, front).Normalized();
				m_EditorCamera.m_View[0].x = right.x;
				m_EditorCamera.m_View[1].x = right.y;
				m_EditorCamera.m_View[2].x = right.z;
				m_EditorCamera.m_View[0].y = up.x;
				m_EditorCamera.m_View[1].y = up.y;
				m_EditorCamera.m_View[2].y = up.z;
				m_EditorCamera.m_View[0].z = front.x;
				m_EditorCamera.m_View[1].z = front.y;
				m_EditorCamera.m_View[2].z = front.z;
				Vec3 negPos = -m_EditorCameraPosition;
				negPos.y *= -1;
				m_EditorCamera.m_View[3] = {
					Dot(right, negPos),
					Dot(up, negPos),
					Dot(front, negPos),
					1.0f,
				};
			}
		}

		void RenderWorld(const Renderer::DrawData& drawData, bool inEditorView) {

			*m_CameraMatricesMap = inEditorView ? m_EditorCamera : m_GameCamera;
			
			m_WasInEditorView = inEditorView;

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
				for (const WorldRenderData& data : m_RenderDatas) {
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
					matrices[0][3].y *= -1;
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

				vkCmdPipelineBarrier(drawData.m_CommandBuffer, 
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
					0, 0, nullptr, 0, nullptr, image_count, memoryBarriers);
			}

			{
				VkRenderingAttachmentInfo colorAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = drawData.m_SwapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = { .color { .uint32 { 0, 0, 0, 0, }, }, },
				};

				VkRenderingAttachmentInfo depthAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = m_DebugDepthImageViews[drawData.m_CurrentFrame],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = { .depthStencil { .depth = 1.0f, .stencil = 0 }, },
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
					.pDepthAttachment = &depthAttachment,
				};

				m_DebugMouseHitID = m_DebugMouseHitBufferMaps[drawData.m_CurrentFrame]->m_ID;

				*m_DebugMouseHitBufferMaps[drawData.m_CurrentFrame] = {
					.m_Depth = 1.0f,
					.m_ID = m_InvalidMouseHitID,
				};

				struct FPC {
					Vec4 c_Color;
					Vec2 c_CursorPos;
					int c_NoID;
					uint32_t c_ID;
				};

				Vec2 cursorPos = Input::GetMousePosition();

				VkDescriptorSet descriptorSets[2]{
					m_CameraMatricesDescriptorSet,
					m_DebugMouseHitDescriptorSets[drawData.m_CurrentFrame],
				};

				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DebugPipelineLayout, 0, 
					2, descriptorSets, 0, nullptr);
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DebugSolidPipeline);
				for (const DebugRenderData& renderData : m_DebugSolidRenderDatas) {
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &renderData.m_Transform);
					bool hasID = renderData.m_ID.HasValue();
					FPC fpc {
						.c_Color = renderData.m_Color,
						.c_CursorPos = cursorPos,
						.c_NoID = hasID,
						.c_ID = hasID ? renderData.m_ID.GetValue() : m_InvalidMouseHitID,
					};
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof(FPC), &fpc);
					Renderer::DrawIndexed(drawData.m_CommandBuffer, renderData.m_MeshData);
				}
				vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DebugWirePipeline);
				for (const DebugRenderData& renderData : m_DebugWireRenderDatas) {
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &renderData.m_Transform);
					bool hasID = renderData.m_ID.HasValue();
					FPC fpc {
						.c_Color = renderData.m_Color,
						.c_CursorPos = cursorPos,
						.c_NoID = hasID,
						.c_ID = hasID ? renderData.m_ID.GetValue() : m_InvalidMouseHitID,
					};
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DebugPipelineLayout, 
						VK_SHADER_STAGE_FRAGMENT_BIT, 64, sizeof(FPC), &fpc);
					Renderer::DrawIndexed(drawData.m_CommandBuffer, renderData.m_MeshData);
				}
				vkCmdEndRendering(drawData.m_CommandBuffer);
				m_DebugWireRenderDatas.Resize(0);
				m_DebugSolidRenderDatas.Resize(0);
			}
		}

		ObjectID LoadArea(FILE* fileStream) {
			if (!fileStream) {
				PrintError(ErrorOrigin::FileParsing, 
					"attempting to load area with file stream that's null (in function World::LoadArea)!");
				return Invalid_ID;
			}
			Area* area = m_Areas.Emplace(m_NextObjectID++, *this, 0);
			assert(area);
			uint32_t counts[2];
			if (fread(counts, sizeof(uint32_t), 2, fileStream) != 2) {
				PrintError(ErrorOrigin::FileParsing,
					"failed to load area from file stream (eof) (function fread in function World::LoadArea)!");
				return Invalid_ID;
			}
			while (true) {
				char c;
				if (fread(&c, sizeof(char), 1, fileStream) != 1) {
					break;
				}
			}
			return m_NextObjectID - 1;
		}

		void SwapchainCreateCallback(VkExtent2D swapchainExtent, Vec2_T<uint32_t> renderResolution, float aspectRatio, uint32_t imageCount);

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
	};

	class Editor {

		friend class Engine;

	public:

		enum class GizmoID {
			None = 0,
			RotatorX = 1,
			RotatorY = 2,
			RotatorZ = 3,
			ArrowX = 4,
			ArrowY = 5,
			ArrowZ = 6,
			MaxEnum,
		};

		enum GizmoRenderStateBits {
			GizmoRenderState_Rotators = 1,
			GizmoRenderState_Arrows = 2,
		};

		typedef uint32_t GizmoRenderState;

		enum class BodyControlState {
			None = 0,
			Position = 1,
			Rotation = 2,
		};

		struct SerializedObject {

			ObjectID m_ObjectID;
			ObjectID m_AreaID;

			bool Rotate(World& world, uint32_t axis, float amount) const {
				return RotateBody(world, axis, amount);
			}

			bool RotateBody(World& world, uint32_t axis, float amount) const {
				Area* area = world.GetArea(m_AreaID);
				Body* body = area->GetBody(m_ObjectID);
				if (!body) {
					PrintError(ErrorOrigin::Editor,
						"couldn't find body (function World::GetBody in function Editor::SerializedObject::RotateBody)!");
					return false;
				}
				Quaternion rot = body->GetRotation();
				switch (axis) {
				case 1:
					rot = rot * Quaternion::AxisRotation(Vec3::Right(), amount);
					break;
				case 2:
					rot = rot * Quaternion::AxisRotation(Vec3::Up(), amount);
					break;
				case 3:
					rot = rot * Quaternion::AxisRotation(Vec3::Forward(), amount);
					break;
				default:
					return false;
				}
				body->SetRotation(rot);
				return true;
			}
		};

	private:

		static constexpr float gizmo_scale_reference = 5.0f;

		static constexpr Vec4 inactive_axis_colors[3] = {
			{ 1.0f, 0.0f, 0.0f, 0.7f },
			{ 0.0f, 0.7f, 0.0f, 0.7f },
			{ 0.0f, 0.0f, 1.0f, 0.7f },
		};

		static constexpr Vec4 hovered_axis_colors[3] = {
			{ 1.0f, 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.7f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f, 1.0f },
		};

		static constexpr Vec4 held_gizmo_color = { 1.0f, 0.0f, 1.0f, 1.0f };

	public:

		World& m_World;
		PhysicsManager& m_PhysicsManager;
		Renderer& m_Renderer;

	private:

		GLFWwindow* const m_GLFWwindow;
		ObjectID m_InspectedAreaID = Invalid_ID;
		uint32_t m_SelectedObjectIndex = UINT32_MAX;

		GizmoID m_HoveredGizmoID = GizmoID::None;
		GizmoID m_HeldGizmoID = GizmoID::None;
		GizmoRenderState m_GizmoRenderState = 0;
		BodyControlState m_BodyControlState = BodyControlState::Position;

		MeshData m_CubeMeshData{};
		MeshData m_QuadMeshData{};
		MeshData m_ArrowMeshData{};
		MeshData m_TorusMeshData{};

		Mat4 m_ArrowTransforms[3];
		Mat4 m_RotatorTransforms[3];
		Vec4 m_ArrowColors[3];
		Vec4 m_RotatorColors[3];

		ImGuiContext* m_ImGuiContext = nullptr;
		VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
		VkFormat m_ImGuiColorAttachmentFormat = VK_FORMAT_UNDEFINED;
		uint32_t m_LastImageCount = 0;

		Editor(World& world, PhysicsManager& physicsManager, Renderer& renderer, GLFWwindow* glfwWindow) 
			: m_World(world), m_PhysicsManager(physicsManager), m_Renderer(renderer), m_GLFWwindow(glfwWindow) {}

		Editor(const Editor&) = delete;

		Editor(Editor&&) = delete;

		static constexpr void CheckVkResult(VkResult vkRes) {
			if (vkRes != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan,
					"ImGui produced a vulkan error!", vkRes);
			}
		}

	public:

		static void AlignedText(const char* text, Optional<float> x_alignment, Optional<float> y_alignment) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 textSize = ImGui::CalcTextSize(text);
			ImVec2 currentCursorPos = ImGui::GetCursorPos();
			if (x_alignment.HasValue()) {
				float off = (avail.x - textSize.x) * x_alignment.GetValue();
				if (off > 0.0f) {
					ImGui::SetCursorPosX(currentCursorPos.x + off);
				}
			}
			if (y_alignment.HasValue()) {
				float off = (avail.y - textSize.y) * y_alignment.GetValue();
				if (off > 0.0f) {
					ImGui::SetCursorPosY(currentCursorPos.y + off);
				}
			}
			ImGui::Text("%s", text);
			if (x_alignment.HasValue()) {
				ImGui::SetCursorPosX(currentCursorPos.x);
			}
			if (y_alignment.HasValue()) {
				ImGui::SetCursorPosY(currentCursorPos.x);
			}
		}

		static bool AlignedButton(const char* label, Optional<float> x_alignment, Optional<float> y_alignment) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 textSize = ImGui::CalcTextSize(label);
			ImVec2 framePadding = ImGui::GetStyle().FramePadding;
			ImVec2 buttonSize = ImVec2(textSize.x + framePadding.x * 2, textSize.y + framePadding.y * 2);
			ImVec2 currentCursorPos = ImGui::GetCursorPos();
			if (x_alignment.HasValue()) {
				float off = (avail.x - buttonSize.x) * x_alignment.GetValue();
				if (off > 0.0f) {
					ImGui::SetCursorPosX(currentCursorPos.x + off);
				}
			}
			if (y_alignment.HasValue()) {
				float off = (avail.y - buttonSize.y) * y_alignment.GetValue();
				if (off > 0.0f) {
					ImGui::SetCursorPosY(currentCursorPos.y + off);
				}
			}
			bool res = ImGui::Button(label);
			if (x_alignment.HasValue()) {
				ImGui::SetCursorPosX(currentCursorPos.x);
			}
			if (y_alignment.HasValue()) {
				ImGui::SetCursorPosY(currentCursorPos.x);
			}
			return res;
		}

	private:

		void Initialize(GLFWwindow* glfwWindow, const MeshData& cubeMeshData, const MeshData& quadMeshData, const MeshData& arrowMeshData, const MeshData& torusMeshData) {

			m_CubeMeshData = cubeMeshData;
			m_QuadMeshData = quadMeshData;
			m_ArrowMeshData = arrowMeshData;
			m_TorusMeshData = torusMeshData;

			IMGUI_CHECKVERSION();		enum class BodyControlState {
			None = 0,
			Position = 1,
			Rotation = 2,
		};

			m_ImGuiContext = ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

			VkDescriptorPoolSize descriptorPoolSize {
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
			};

			m_ImGuiDescriptorPool = m_Renderer.CreateDescriptorPool(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1, 1, &descriptorPoolSize);

			if (m_ImGuiDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create descriptor pool for ImGui (function Renderer::CreateDescriptorPool in function Editor::Initialize)!");
			}

			m_ImGuiColorAttachmentFormat = m_Renderer.m_SwapchainSurfaceFormat.format;

			ImGui_ImplVulkan_InitInfo ImGuiVulkanInitInfo {
				.Instance = m_Renderer.m_VulkanInstance,
				.PhysicalDevice = m_Renderer.m_Gpu,
				.Device = m_Renderer.m_VulkanDevice,
				.QueueFamily = m_Renderer.m_GraphicsQueueFamilyIndex,
				.Queue = m_Renderer.m_GraphicsQueue,
				.DescriptorPool = m_ImGuiDescriptorPool,
				.RenderPass = nullptr,
				.MinImageCount = m_Renderer.m_FramesInFlight,
				.ImageCount = m_Renderer.m_FramesInFlight,
				.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
				.PipelineCache = nullptr,
				.Subpass = 0,
				.UseDynamicRendering = true,
				.PipelineRenderingCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
					.pNext = nullptr,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachmentFormats = &m_ImGuiColorAttachmentFormat,
					.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
					.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
				},
				.Allocator = m_Renderer.m_VulkanAllocationCallbacks,
				.CheckVkResultFn = CheckVkResult,
				.MinAllocationSize = 1024U * 1024U,
			};

			if (!ImGui_ImplGlfw_InitForVulkan(glfwWindow, true)) {
				CriticalError(ErrorOrigin::Editor,
					"failed to initialize ImGui (function ImGui_ImplGlfw_InitForVulkan in function Editor::Initialize)!");
			}
			if (!ImGui_ImplVulkan_Init(&ImGuiVulkanInitInfo)) {
				CriticalError(ErrorOrigin::Editor,
					"failed to initialize ImGui (function ImGui_ImplVulkan_Init in function Editor::Initialize)!");
			}
			if (!ImGui_ImplVulkan_CreateFontsTexture()) {
				CriticalError(ErrorOrigin::Editor,
					"failed to initialize ImGui (function ImGui_ImplVulkan_CreateFontsTexture in function Editor::Initialize)!");
			}

			m_World.DefineInvalidMouseHitID(0);
		}

		void Terminate() {
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
			m_Renderer.DestroyDescriptorPool(m_ImGuiDescriptorPool);
			m_ImGuiDescriptorPool = VK_NULL_HANDLE;
		}

		void SwapchainCreateCallback(VkExtent2D extent, uint32_t imageCount);

	public:

		void SetInspectedArea(ObjectID ID) {
			m_InspectedAreaID = ID;
		}

	private:

		void ActivateArrows(const Vec3& position) {
			float m = (position - m_World.m_EditorCameraPosition).Magnitude();
			float s = m / gizmo_scale_reference / 3.0f;
			m_ArrowTransforms[0] = Mat4::Transform(position, Quaternion::AxisRotation(Vec3::Up(), Math::pi / 2), Vec3::One(s));
			m_ArrowTransforms[1] = Mat4::Transform(position, Quaternion::AxisRotation(Vec3::Right(), Math::pi / 2), Vec3::One(s));
			m_ArrowTransforms[2] = Mat4::Transform(position, Quaternion::Identity(), Vec3::One(s));
			m_GizmoRenderState |= GizmoRenderState_Arrows;
		}

		void ActivateRotators(const Vec3& position) {
			float m = (position - m_World.m_EditorCameraPosition).Magnitude();
			float s = m / gizmo_scale_reference;
			m_RotatorTransforms[0] = Mat4::Transform(position, Quaternion::AxisRotation(Vec3::Forward(), Math::pi / 2), Vec3::One(s));
			m_RotatorTransforms[1] = Mat4::Transform(position, Quaternion::Identity(), Vec3::One(s));
			m_RotatorTransforms[2] = Mat4::Transform(position, Quaternion::AxisRotation(Vec3::Right(), Math::pi / 2), Vec3::One(s));
			m_GizmoRenderState |= GizmoRenderState_Rotators;
		}

		void ActivateBodyControls(const Vec3& position) {
			m_GizmoRenderState &= ~(GizmoRenderState_Arrows | GizmoRenderState_Rotators);
			switch (m_BodyControlState) {
				using S = BodyControlState;
				case S::None:
					break;
				case S::Position:
					ActivateArrows(position);
					break;
				case S::Rotation:
					ActivateRotators(position);
					break;
			}
		}

		void DeactivateBodyControls() {
			m_GizmoRenderState &= ~(GizmoRenderState_Arrows | GizmoRenderState_Rotators);
		}

		void UpdateGizmoColors() {
			for (uint32_t i = 0; i < 3; i++) {
				m_RotatorColors[i] = inactive_axis_colors[i];
				m_ArrowColors[i] = inactive_axis_colors[i];
			}
			if (m_HeldGizmoID == GizmoID::None) {
				switch (m_HoveredGizmoID) {
					using ID = GizmoID;
					case ID::None:
						break;
					case ID::RotatorX:
						m_RotatorColors[0] = hovered_axis_colors[0];
						break;
					case ID::RotatorY:
						m_RotatorColors[1] = hovered_axis_colors[1];
						break;
					case ID::RotatorZ:
						m_RotatorColors[2] = hovered_axis_colors[2];
						break;
					case ID::ArrowX:
						m_ArrowColors[0] = hovered_axis_colors[0];
						break;
					case ID::ArrowY:
						m_ArrowColors[1] = hovered_axis_colors[1];
						break;
					case ID::ArrowZ:
						m_ArrowColors[2] = hovered_axis_colors[2];
						break;
					default:
						assert(false);
				};
			}
			else {
				switch (m_HeldGizmoID) {
					using ID = GizmoID;
					case ID::RotatorX:
						m_RotatorColors[0] = held_gizmo_color;
						break;
					case ID::RotatorY:
						m_RotatorColors[1] = held_gizmo_color;
						break;
					case ID::RotatorZ:
						m_RotatorColors[2] = held_gizmo_color;
						break;
					case ID::ArrowX:
						m_ArrowColors[0] = held_gizmo_color;
						break;
					case ID::ArrowY:
						m_ArrowColors[1] = held_gizmo_color;
						break;
					case ID::ArrowZ:
						m_ArrowColors[2] = held_gizmo_color;
						break;
					default:
						assert(false);
				}
			}
		}

		void NewFrame() {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
		}

		void Update(EngineState& ioEngineState) {

			using MouseButton = Input::MouseButton;

			IntVec2 glfwWindowSize;
			glfwGetFramebufferSize(m_GLFWwindow, &glfwWindowSize.x, &glfwWindowSize.y);

			ImGui::SetNextWindowSize(ImVec2(glfwWindowSize.x, glfwWindowSize.y));
			ImGui::SetNextWindowPos(ImVec2(0, 0));

			ImGuiWindowFlags dockingSpaceWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
				| ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavFocus
				| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			if (ImGui::Begin("Docking Space", nullptr, dockingSpaceWindowFlags)) {
				ImGui::PopStyleVar(3);
				ImGui::DockSpace(ImGui::GetID("MainDockingSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
			}
			ImGui::End();

			ImGui::SetNextWindowSize({ 200.0f, 100.0f });

			if (ImGui::Begin("Engine Controller", nullptr, ImGuiWindowFlags_NoResize)) {
				if (!(ioEngineState & EngineState_Play)) {
					if (AlignedButton("Play", 0.5f, {})) {
						ioEngineState |= EngineState_Play;
					}
				}
				else if (ioEngineState & EngineState_Pause) {
					if (AlignedButton("Play", 0.5f, {})) {
						ioEngineState &= ~EngineState_Pause;
					}
					if (AlignedButton("Reset", 0.5f, {})) {
						ioEngineState &= ~EngineState_Play;
						ioEngineState &= ~EngineState_Pause;
					}
				}
				else {
					if (AlignedButton("Pause", 0.5f, {})) {
						ioEngineState |= EngineState_Pause;
					}
					if (AlignedButton("Reset", 0.5f, {})) {
						ioEngineState &= ~EngineState_Play;
						ioEngineState &= ~EngineState_Pause;
					}
				}
				if (!(ioEngineState & EngineState_EditorView)) {
					if (AlignedButton("Editor view", 0.5f, {})) {
						ioEngineState |= EngineState_EditorView;
					}
				}
				else if (AlignedButton("Game view", 0.5f, {})) {
					ioEngineState &= ~EngineState_EditorView;
				}
			}
			ImGui::End();

			if (ImGui::Begin("Area")) {
				if (m_InspectedAreaID == Invalid_ID) {
					AlignedText("No area selected", 0.5f, 0.5f);
				}
				else {
					Area* pArea = m_World.GetArea(m_InspectedAreaID);
					if (!pArea) {
						AlignedText("Invalid area ID!", 0.5f, 0.5f);
					}
					else {
						Area& area = *pArea;
						ImGuiStyle& style = ImGui::GetStyle();
						bool buttonActive = false;
						if (ImGui::CollapsingHeader("Bodies", ImGuiTreeNodeFlags_DefaultOpen)) {

							uint32_t index = 0;

							const ObjectID* IDs = area.m_Bodies.GetKeys();
							Body* bodies = area.m_Bodies.GetValues();
							uint32_t bodyCount = area.m_Bodies.Size();

							for (uint32_t i = 0; i < bodyCount; i++) {
								Body& body = bodies[i];
								ObjectID ID = IDs[i];
								ImGui::SetCursorPosX(style.ItemSpacing.x);
								if (m_SelectedObjectIndex == index) {
									m_PhysicsManager.GetBodyDrawFilter().AddBodyID(body.GetPhysicsID());
									ActivateBodyControls(body.GetPosition());
									/*
									Box<float> boundingBox = obstacle.GetBoundingBox();
									Vec3 dimensions = boundingBox.Dimensions();
									Vec3 obstaclePos = obstacle.GetPosition();
									Mat4 transform(1);
									transform[0] *= dimensions.x / 2;
									transform[1] *= dimensions.y / 2;
									transform[2] *= dimensions.z / 2;
									transform[3] = Vec4(obstaclePos, 1.0f);
									m_World.RenderWireMesh(m_CubeMeshData, transform, m_WireColor);
									ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonHovered]);
									*/
									if (ImGui::Button((body.GetName() + ", ID : " + IntToString(ID)).CString())) {
										//Vec3 offset = Vec3(0.0f, boundingBox.m_Max.y + dimensions.y, boundingBox.m_Min.z - dimensions.z);
										Vec3 pos = body.GetPosition();
										Vec3 offset = Vec3(0.0f, 5.0f, -5.0f);
										m_World.m_EditorCameraPosition = pos + offset;
										Vec3 forward = pos - m_World.m_EditorCameraPosition;
										m_World.m_EditorCameraRotations = Vec2(-Angle(Vec3::Forward(), forward), 0.0f);
										m_World.UpdateEditorCamera();
									}
									else if (ImGui::IsItemActive()) {
										buttonActive = true;
									}
									//ImGui::PopStyleColor();

									if (m_HeldGizmoID != GizmoID::None) {
										static constexpr float rot_div = 250.0f;
										static constexpr float pos_div = 250.0f;
										static constexpr float pi_half = Math::pi / 2.0;
										Vec2 deltaCursorPos = Input::GetDeltaMousePosition();
										switch (m_HeldGizmoID) {
											using ID = GizmoID;
											case ID::RotatorX: {
												Quaternion rot = body.GetRotation();
												Vec3 axis = Vec3::Right() * rot.AsMat3();
												body.SetRotation(Quaternion::AxisRotation(axis, deltaCursorPos.y / rot_div) * rot);
												break;
											}
											case ID::RotatorY: {
												Quaternion rot = body.GetRotation();
												Vec3 axis = Vec3::Up() * rot.AsMat3();
												body.SetRotation(Quaternion::AxisRotation(axis, -deltaCursorPos.x / rot_div)* rot);
												break;
											}
											case ID::RotatorZ: {
												Quaternion rot = body.GetRotation();
												Vec3 axis = Vec3::Forward() * rot.AsMat3();
												body.SetRotation(Quaternion::AxisRotation(axis, deltaCursorPos.y / rot_div) * rot);
												break;
											}
											case ID::ArrowX: {
												Vec3 bodyPos = body.GetPosition();
												Vec2 screenPos1 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos, 1.0f);
												Vec2 screenPos2 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos + Vec3::Right(), 1.0f);
												Vec2 screenArrowDirection = screenPos2 - screenPos1;
												screenArrowDirection.y *= -1.0f;
												float angle = Angle(screenArrowDirection, deltaCursorPos);
												body.SetPosition(bodyPos +
													Vec3::Right(deltaCursorPos.Magnitude() * (1.0f - angle / pi_half) / pos_div));
												break;
											}
											case ID::ArrowY: {
												if (Input::WasKeyHeld(Input::Key::Space) && deltaCursorPos.x < 0.0f) {
													int x = 0;
												}
												Vec3 bodyPos = body.GetPosition();
												Vec2 screenPos1 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos, 1.0f);
												Vec2 screenPos2 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos + Vec3::Up(), 1.0f);
												Vec2 screenArrowDirection = screenPos2 - screenPos1;
												float angle = Angle(screenArrowDirection, deltaCursorPos);
												body.SetPosition(bodyPos +
													Vec3::Up(deltaCursorPos.Magnitude() * (1.0f - angle / pi_half) / pos_div));
												break;
											}
											case ID::ArrowZ: {
												Vec3 bodyPos = body.GetPosition();
												Vec2 screenPos1 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos, 1.0f);
												Vec2 screenPos2 = m_World.m_EditorCamera.m_Projection * m_World.m_EditorCamera.m_View * Vec4(bodyPos + Vec3::Forward(), 1.0f);
												Vec2 screenArrowDirection = screenPos2 - screenPos1;
												screenArrowDirection.y *= -1.0f;
												float angle = Angle(screenArrowDirection, deltaCursorPos);
												body.SetPosition(bodyPos +
													Vec3::Forward(deltaCursorPos.Magnitude() * (1.0f - angle / pi_half) / pos_div));
												break;
											}
											default:
												assert(false);
												break;
										}
									}
								}
								else if (ImGui::Button((body.GetName() + ", ID : " + IntToString(ID)).CString())) {
									m_SelectedObjectIndex = index;
									ActivateArrows(body.GetPosition());
									m_PhysicsManager.GetBodyDrawFilter().RemoveAllBodyIDs();
								}
								else if (ImGui::IsItemActive()) {
									buttonActive = true;
								}
								++index;
							}
						}
						if (!buttonActive && ImGui::IsWindowHovered() && Input::WasMouseButtonPressed(MouseButton::Left)) {
							m_SelectedObjectIndex = UINT32_MAX;
							m_PhysicsManager.GetBodyDrawFilter().RemoveAllBodyIDs();
							DeactivateBodyControls();
						}
					}	
				}
			}
			ImGui::End();

			m_HoveredGizmoID = (GizmoID)m_World.GetMouseHitID();
			if ((uint32_t)m_HoveredGizmoID >= (uint32_t)GizmoID::MaxEnum) {
				m_HoveredGizmoID = GizmoID::None;
			}
			if (Input::WasMouseButtonPressed(MouseButton::Left)) {
				if (m_HoveredGizmoID != GizmoID::None) {
					m_HeldGizmoID = m_HoveredGizmoID;
				}	
			}
			else if (!Input::WasMouseButtonHeld(MouseButton::Left)) {
				m_HeldGizmoID = GizmoID::None;
			}

			UpdateGizmoColors();

			if (m_GizmoRenderState & GizmoRenderState_Arrows) {
				m_World.RenderDebugSolidMesh(m_ArrowMeshData, m_ArrowTransforms[0], m_ArrowColors[0], (uint32_t)GizmoID::ArrowX);
				m_World.RenderDebugSolidMesh(m_ArrowMeshData, m_ArrowTransforms[1], m_ArrowColors[1], (uint32_t)GizmoID::ArrowY);
				m_World.RenderDebugSolidMesh(m_ArrowMeshData, m_ArrowTransforms[2], m_ArrowColors[2], (uint32_t)GizmoID::ArrowZ);
			}
			if (m_GizmoRenderState & GizmoRenderState_Rotators) {
				m_World.RenderDebugSolidMesh(m_TorusMeshData, m_RotatorTransforms[0], m_RotatorColors[0], (uint32_t)GizmoID::RotatorX);
				m_World.RenderDebugSolidMesh(m_TorusMeshData, m_RotatorTransforms[1], m_RotatorColors[1], (uint32_t)GizmoID::RotatorY);
				m_World.RenderDebugSolidMesh(m_TorusMeshData, m_RotatorTransforms[2], m_RotatorColors[2], (uint32_t)GizmoID::RotatorZ);
			}
		}

		void Render(const Renderer::DrawData& drawData) const {
			{
				VkRenderingAttachmentInfo colorAttachmentInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, 
					.pNext = nullptr,
					.imageView = drawData.m_SwapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { .color { .uint32 { 0, 0, 0, 0 } } },
				};

				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea { { 0, 0 }, drawData.m_SwapchainExtent },
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachmentInfo,
					.pDepthAttachment = nullptr,
					.pStencilAttachment = nullptr,
				};

				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);

				ImGui::Render();
				ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), drawData.m_CommandBuffer);

				vkCmdEndRendering(drawData.m_CommandBuffer);
			}
		}
	};

	class Engine {

		friend void CriticalError(ErrorOrigin origin, const char* err, VkResult vkErr, const char* libErr);

	private:

		EngineState m_State;

		static constexpr uint32_t sc_RenderResolutionHeight1080p = 400;
		Vec2_T<uint32_t> m_RenderResolution;

		UI m_UI;
		World m_World;
		PhysicsManager m_PhysicsManager;
		Editor m_Editor;
		Renderer m_Renderer;
		TextRenderer m_TextRenderer;
		AssetManager m_AssetManager;

		World::SaveState m_WorldSaveState{};

		StaticMesh m_StaticQuadMesh;
		StaticMesh m_StaticQuadMesh2D;
		StaticMesh m_StaticBoxMesh;
		StaticMesh m_StaticArrowGizmoMesh;
		StaticMesh m_StaticTorusGizmoMesh;

		static constexpr uint32_t quad_vertex_count = 4;

		static constexpr Vertex quad_vertices[quad_vertex_count] {
			{
				.m_Position { -1.0f, 1.0f, 0.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.0f, 0.0f },
			},
			{
				.m_Position { 1.0f, 1.0f, 0.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 1.0f, 0.0f },
			},
			{
				.m_Position { -1.0f, -1.0f, 0.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.0f, 1.0f },
			},
			{
				.m_Position { 1.0f, -1.0f, 0.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 1.0f, 1.0f },
			},	
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

		static constexpr uint32_t quad_index_count = 6;

		static constexpr uint32_t quad_indices[quad_index_count] {
			3, 2, 0,
			1, 3, 0,
		};

		static constexpr uint32_t box_vertex_count = 24;

		static constexpr Vertex box_vertices[box_vertex_count] {
			{
				.m_Position { 1.0f, -1.0f, 1.0f }, 
				.m_Normal { 0.0f, -1.0f, 0.0f },
				.m_UV { 1.0f, 0.333333f },
			},
			{
				.m_Position { -1.0f, -1.0f, 1.0f },
				.m_Normal { 0.0f, -1.0f, 0.0f },
				.m_UV { 1.0f, 0.666667f },
			},
			{
				.m_Position { -1.0f, -1.0f, -1.0f },
				.m_Normal { 0.0f, -1.0f, 0.0f },
				.m_UV { 0.666667f, 0.666667f },
			},
			{
				.m_Position { -1.0f, 1.0f, -1.0f },
				.m_Normal { 0.0f, 1.0f, 0.0f },
				.m_UV { 1.0f, 0.333333f },
			},
			{
				.m_Position { -1.0f, 1.0f, 1.0f },
				.m_Normal { 0.0f, 1.0f, 0.0f },
				.m_UV { 0.666667f, 0.333333f },
			},
			{
				.m_Position { 1.0f, 1.0f, 1.0f },
				.m_Normal { 0.0f, 1.0f, 0.0f },
				.m_UV { 0.666667f, 0.0f },
			},
			{
				.m_Position { 1.0f, 1.0f, -1.0f },
				.m_Normal { 1.0f, 0.0f, 0.0f },
				.m_UV { 0.0f, 0.333333f },
			},
			{
				.m_Position { 1.0f, 1.0f, 1.0f },
				.m_Normal { 1.0f, 0.0f, 0.0f },
				.m_UV { 0.0f, 0.0f },
			},
			{
				.m_Position { 1.0f, -1.0f, 1.0f },
				.m_Normal { 1.0f, 0.0f, 0.0f },
				.m_UV { 0.333333f, 0.0f },
			},
			{
				.m_Position { 1.0f, 1.0f, 1.0f },
				.m_Normal { 0.0f, 0.0f, 1.0f },
				.m_UV { 0.333333f, 0.0f },
			},
			{
				.m_Position { -1.0f, 1.0f, 1.0f },
				.m_Normal { 0.0f, 0.0f, 1.0f },
				.m_UV { 0.666667f, 0.0f },
			},
			{
				.m_Position { -1.0f, -1.0f, 1.0f },
				.m_Normal { 0.0f, 0.0f, 1.0f },
				.m_UV { 0.666667f, 0.333333f },
			},
			{
				.m_Position { -1.0f, -1.0f, 1.0f },
				.m_Normal { -1.0f, 0.0f, 0.0f },
				.m_UV { 0.333333f, 1.0f },
			},
			{
				.m_Position { -1.0f, 1.0f, 1.0f },
				.m_Normal { -1.0f, 0.0f, 0.0f },
				.m_UV { 0.0f, 1.0f },
			},
			{
				.m_Position { -1.0f, 1.0f, -1.0f },
				.m_Normal { -1.0f, 0.0f, 0.0f },
				.m_UV { 0.0f, 0.666667f },
			},
			{
				.m_Position { 1.0f, -1.0f, -1.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.333333f, 0.333333f },
			},
			{
				.m_Position { -1.0f, -1.0f, -1.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.333333f, 0.666667f },
			},
			{
				.m_Position { -1.0f, 1.0f, -1.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.0f, 0.666667f },
			},
			{
				.m_Position { 1.0f, -1.0f, -1.0f },
				.m_Normal { 0.0f, -1.0f, 0.0f },
				.m_UV { 0.666667f, 0.333333f },
			},
			{
				.m_Position { 1.0f, 1.0f, -1.0f },
				.m_Normal { 0.0f, 1.0f, 0.0f },
				.m_UV { 1.0f, 0.0f },
			},
			{
				.m_Position { 1.0f, -1.0f, -1.0f },
				.m_Normal { 1.0f, 0.0f, 0.0f },
				.m_UV { 0.333333f, 0.333333f },
			},
			{
				.m_Position { 1.0f, -1.0f, 1.0f },
				.m_Normal { 0.0f, 0.0f, 1.0f },
				.m_UV { 0.333333f, 0.333333f },
			},
			{
				.m_Position { -1.0f, -1.0f, -1.0f },
				.m_Normal { -1.0f, 0.0f, 0.0f },
				.m_UV { 0.333333f, 0.666667f },
			},
			{
				.m_Position { 1.0f, 1.0f, -1.0f },
				.m_Normal { 0.0f, 0.0f, -1.0f },
				.m_UV { 0.0f, 0.333333f },
			},
		};

		static constexpr uint32_t box_index_count = 36;

		static constexpr uint32_t box_indices[box_index_count] {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
			0, 2, 19, 3, 5, 20, 6, 8, 21, 9, 11, 22, 12, 14, 23, 15, 17,
		};

		static inline Obj s_arrow_gizmo_obj{};
		static inline Obj s_torus_gizmo_obj{};

	public:

		static void SetArrowGizmoObj(const Obj& obj) {
			if (!obj.IsValid()) {
				CriticalError(ErrorOrigin::Engine, "invalid arrow obj set (function Engine::SetArrowGizmoObj)!");
			}
			s_arrow_gizmo_obj = obj;
		}

		static void SetTorusGizmoObj(const Obj& obj) {
			if (!obj.IsValid()) {
				CriticalError(ErrorOrigin::Engine, "invalid torus obj set (function Engine::SetTorusGizmoObj)!");
			}
			s_torus_gizmo_obj = obj;
		}	

		Engine(EngineState state, const String& projectName, GLFWwindow* glfwWindow, size_t maxUIWindows) :
				m_State(UpdateEngineInstance(this, state)),
				m_UI(m_Renderer, m_TextRenderer, maxUIWindows),
				m_World(m_AssetManager, m_PhysicsManager, m_Renderer),
				m_PhysicsManager(m_World, m_Renderer, 65536, 65536, 10240),
				m_Renderer(projectName.CString(), VK_MAKE_API_VERSION(0, 1, 0, 0), glfwWindow, RendererCriticalErrorCallback, SwapchainCreateCallback),
				m_TextRenderer(m_Renderer, TextRendererCriticalErrorCallback),
				m_AssetManager(projectName, m_Renderer),
				m_Editor(m_World, m_PhysicsManager, m_Renderer, glfwWindow),
				m_StaticQuadMesh(m_Renderer),
				m_StaticQuadMesh2D(m_Renderer),
				m_StaticBoxMesh(m_Renderer),
				m_StaticArrowGizmoMesh(m_Renderer),
				m_StaticTorusGizmoMesh(m_Renderer)
		{

			if (!s_arrow_gizmo_obj.IsValid()) {
				CriticalError(ErrorOrigin::Engine,
					"attempting to construct engine when gizmo arrow obj is not set (in Engine constructor)!");
			}

			if (!s_torus_gizmo_obj.IsValid()) {
				CriticalError(ErrorOrigin::Engine,
					"attempting to construct engine when gizmo torus obj is not set (in Engine constructor)!");
			}

			Input input(glfwWindow);

			if (!m_StaticQuadMesh.CreateBuffers(quad_vertex_count, quad_vertices, quad_index_count, quad_indices)) {
				CriticalError(ErrorOrigin::Engine, 
					"failed to create static 3D quad mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}
			if (!m_StaticQuadMesh2D.CreateBuffers(quad_vertex_count, quad_vertices_2D, quad_index_count, quad_indices)) {
				CriticalError(ErrorOrigin::Engine, 
					"failed to create static 2D quad mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}
			if (!m_StaticBoxMesh.CreateBuffers(box_vertex_count, box_vertices, box_index_count, box_indices)) {
				CriticalError(ErrorOrigin::Engine, 
					"failed to create static box mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}

			DynamicArray<Vertex> arrowVertices{};
			DynamicArray<uint32_t> arrowIndices{};

			if (!s_arrow_gizmo_obj.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, arrowVertices, arrowIndices)) {
				CriticalError(ErrorOrigin::Engine,
					"failed to get arrow gizmo mesh from obj (function Obj::GetMesh in Engine constructor)!");
			}

			if (!m_StaticArrowGizmoMesh.CreateBuffers(arrowVertices.Size(), arrowVertices.Data(), arrowIndices.Size(), arrowIndices.Data())) {
				CriticalError(ErrorOrigin::Engine,
					"failed to create arrow gizmo mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
			}

			DynamicArray<Vertex> torusVertices{};
			DynamicArray<uint32_t> torusIndices{};

			if (!s_torus_gizmo_obj.GetMesh(Vertex::SetPosition, Vertex::SetUV, Vertex::SetNormal, torusVertices, torusIndices)) {
				CriticalError(ErrorOrigin::Engine,
					"failed to get torus gizmo mesh from obj (function Obj::GetMesh in Engine constructor)!");
			}

			if (!m_StaticTorusGizmoMesh.CreateBuffers(torusVertices.Size(), torusVertices.Data(), torusIndices.Size(), torusIndices.Data())) {
				CriticalError(ErrorOrigin::Engine,
					"failed to create torus gizmo mesh (function StaticMesh::CreateBuffers in Engine constructor)!");
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
			m_PhysicsManager.Initialize();
			m_UI.Initialize(m_StaticQuadMesh2D);
			m_Editor.Initialize(glfwWindow, m_StaticBoxMesh.GetMeshData(), m_StaticQuadMesh2D.GetMeshData(),
				m_StaticArrowGizmoMesh.GetMeshData(), m_StaticTorusGizmoMesh.GetMeshData());
		}

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;

		~Engine() {
			m_World.Terminate();
			m_StaticQuadMesh.Terminate();
			m_StaticQuadMesh2D.Terminate();
			m_StaticBoxMesh.Terminate();
			m_StaticArrowGizmoMesh.Terminate();
			m_StaticTorusGizmoMesh.Terminate();
			m_UI.Terminate();
			m_Editor.Terminate();
			m_PhysicsManager.Terminate();
			m_Renderer.DestroySampler(FontAtlas::s_Sampler);
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
		}

	public:

		Vec2_T<uint32_t> GetSwapchainResolution() {
			return { m_Renderer.m_SwapchainExtent.width, m_Renderer.m_SwapchainExtent.height };
		}

		UI& GetUI() {
			return m_UI;
		}

		Renderer& GetRenderer() {
			return m_Renderer;
		}

		TextRenderer& GetTextRenderer() {
			return m_TextRenderer;
		}

		World& GetWorld() {
			return m_World;
		}

		Editor& GetEditor() {
			return m_Editor;
		}

		const StaticMesh& GetQuadMesh() const {
			return m_StaticQuadMesh;
		}

		static constexpr void GetQuadMesh(Array<Vertex, quad_vertex_count>& outVertices, Array<uint32_t, quad_index_count>& outIndices) {
			for (uint32_t i = 0; i < quad_vertex_count; i++) {
				outVertices[i] = quad_vertices[i];
			}
			for (uint32_t i = 0; i < quad_index_count; i++) {
				outIndices[i] = quad_indices[i];
			}
		}

		static constexpr uint32_t GetBoxVertexCount() {
			return box_vertex_count;
		}

		static constexpr uint32_t GetBoxIndexCount() {
			return box_index_count;
		}

		const StaticMesh& GetBoxMesh() const {
			return m_StaticBoxMesh;
		}

		static constexpr void GetBoxMesh(Array<Vertex, box_vertex_count>& outVertices, Array<uint32_t, box_index_count>& outIndices) {
			for (uint32_t i = 0; i < box_vertex_count; i++) {
				outVertices[i] = box_vertices[i];
			}
			for (uint32_t i = 0; i < box_index_count; i++) {
				outIndices[i] = box_indices[i];
			}
		}

		bool Loop() {

			Time::BeginFrame();

			glfwPollEvents();

			if (m_State & EngineState_Play && !(m_State & EngineState_Pause)) {
				if (m_State & EngineState_LogicAwake) {
					m_WorldSaveState.Clear();
					m_WorldSaveState.Initialize(m_World);
				}
				m_World.LogicUpdate();
				if (m_PhysicsManager.PhysicsUpdate()) {
					m_World.FixedUpdate();
				}
				m_UI.UILoop();
				m_State |= EngineState_QueueLogicShutdown;
				m_State &= ~EngineState_LogicAwake;
			}
			else if (!(m_State & EngineState_Pause)) {
				m_State |= EngineState_LogicAwake;
				if (m_State & EngineState_QueueLogicShutdown) {
					m_World.LoadSaveState(m_WorldSaveState);
					m_State &= ~EngineState_QueueLogicShutdown;
				}
			}

			if (m_State & EngineState_Editor) {
				m_Editor.NewFrame();
				m_Editor.Update(m_State);
				m_World.EditorUpdate(m_Renderer.m_Window);
				m_PhysicsManager.DebugUpdate();
			}

			Renderer::DrawData drawData;

			if (m_Renderer.BeginFrame(drawData)) {
				m_World.RenderWorld(drawData, m_State & EngineState_EditorView);
				m_UI.RenderUI(drawData);
				if (m_State & EngineState_Editor) {
					m_Editor.Render(drawData);
				}
				m_Renderer.EndFrame(0, nullptr);
			}
			else {
				ImGui::EndFrame();
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
			s_engine_instance->m_Editor.SwapchainCreateCallback(swapchainExtent, imageCount);
		}

		static inline EngineState UpdateEngineInstance(Engine* engine, EngineState state) {
			if (s_engine_instance) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"attempting to initialize engine twice (only one engine allowed)!");
				exit(EXIT_FAILURE);
			}
			s_engine_instance = engine;
			return state;
		}
	};
}
