#pragma once

#include "renderer.hpp"
#include "text_renderer.hpp"
#include "math.hpp"
#include "fmt/printf.h"
#include "fmt/color.h"
#include "vulkan/vulkan_core.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#include "imgui.h"
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <chrono>

namespace engine {

	enum EngineModeBits {
		EngineMode_Initialized = 1,
		EngineMode_Play = 2,
		EngineMode_Game = 4,
		EngineMode_Editor = 8,
	};

	typedef uint32_t EngineMode;

	class Engine {
	public:

		typedef uint64_t UID;
		typedef std::lock_guard<std::mutex> LockGuard;

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
			Renderer = 1,
			TextRenderer = 2,
			UI = 3,
			OutOfMemory = 4,
			NullDereference = 5,
			IndexOutOfBounds = 6,
			Vulkan = 7,
			Stb = 8,
			DynamicArray = 9,
			FileParsing = 10,
			GameLogic = 11,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"Renderer",
				"TextRenderer",
				"UI",
				"OutOfMemory",
				"NullDereference",
				"IndexOutOfBounds",
				"Vulkan",
				"stb",
				"DynamicArray",
				"FileParsing",
				"GameLogic",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		static void PrintError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Engine called an error!\nError origin: {}s\nError: {}\n", ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "Vulkan error code: {}\n", (int)vkErr);
			}
		}

		static void PrintWarning(const char* warn) {
			fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, 
				"Engine called a warning:\n {}\n", warn);
		}

		template<typename T>
		class DynamicArray {
		public:

			typedef T* Iterator;
			typedef const T* ConstIterator;

			T* m_Data;
			size_t m_Size;
			size_t m_Capacity;

			DynamicArray() : m_Data(nullptr), m_Size(0), m_Capacity(0) {}

			DynamicArray(size_t size) : m_Data(nullptr), m_Size(0), m_Capacity(0) {
				if (!size) {
					return;
				}
				Reserve(size * 2);
				m_Size = size;
			}

			DynamicArray(DynamicArray&& other) noexcept 
				: m_Data(other.m_Data), m_Size(other.m_Size), m_Capacity(other.m_Capacity) {
				other.m_Data = nullptr;
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
					PrintError(ErrorOrigin::DynamicArray, 
						"attempting to erase from dynamic array with an iterator that's outside the bounds of the array (function Erase)!");
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

			char* m_Data;
			size_t m_Length;
			size_t m_Capacity;

		public:

			constexpr String() noexcept : m_Data(nullptr), m_Length(0), m_Capacity(0) {}

			constexpr String(String&& other) noexcept
				: m_Data(other.m_Data), m_Length(other.m_Length), m_Capacity(other.m_Capacity) {
				other.m_Data = nullptr;
				other.m_Length = 0;
				other.m_Capacity = 0;
			}

			String(const char* str) noexcept {
				size_t len = strlen(str);
				Reserve(len + 1);
				for (size_t i = 0; i != len; i++) {
					m_Data[i] = str[i];
				}
				m_Length = len;
				m_Data[m_Length] = '\0';
			}

			String(char* buf, size_t begin, size_t end) {
				assert(begin < end && "erroneous arguments for Engine::String constructor!");
				Reserve((end - begin) + 1);
				m_Length = end - begin;
				for (; begin != end; begin++) {
					m_Data[begin] = buf[begin];
				}
				m_Data[m_Length] = '\0';
			}

			~String() {
				free(m_Data);
				m_Data = nullptr;
				m_Length = 0;
				m_Capacity = 0;
			}

			String& Reserve(size_t capacity) {
				if (capacity < m_Capacity) {
					return *this;
				}
				char* temp = (char*)malloc(sizeof(char) * capacity);
				for (size_t i = 0; i < m_Length; i++) {
					temp[i] = m_Data[i];
				}
				temp[m_Length] = '\0';
				free(m_Data);
				m_Data = temp;
				m_Capacity = capacity;
				return *this;
			}

			uint64_t operator()() {
				if (!m_Length) {
					return 0;
				}
				uint64_t res = 37;
				for (size_t i = 0; i < m_Length; i++) {
					res = (res * 54059) ^ ((uint64_t)m_Data[i] * 76963);
				}
				return res;
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

		struct MeshData {
			uint32_t m_VertexBufferCount;
			uint32_t m_IndexCount;
			const VkBuffer* m_VertexBuffers;
			const VkDeviceSize* m_VertexBufferOffsets;
			VkBuffer m_IndexBuffer;
		};

		class StaticMesh {
		public:

			Engine& m_Engine;
			uint32_t m_IndexCount;
			Renderer::Buffer m_VertexBuffer;
			Renderer::Buffer m_IndexBuffer;

			StaticMesh(Engine& engine) noexcept : m_Engine(engine), m_VertexBuffer(engine.m_Renderer), 
				m_IndexBuffer(engine.m_Renderer), m_IndexCount(0) {}

			StaticMesh(StaticMesh&& other) noexcept : m_Engine(other.m_Engine), m_VertexBuffer(std::move(other.m_VertexBuffer)), 
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
		public:

			Engine& m_Engine;
			VkFormat m_Format;
			VkImage m_Image;
			VkDeviceMemory m_VulkanDeviceMemory;

			StaticTexture(Engine& engine) noexcept 
				: m_Engine(engine), m_Format(VK_FORMAT_UNDEFINED), m_Image(VK_NULL_HANDLE), m_VulkanDeviceMemory(VK_NULL_HANDLE) {}

			bool Create(VkFormat format, uint32_t colorChannels, Vec2_T<uint32_t> extent, const void* image) {
				Renderer& renderer = m_Engine.m_Renderer;
				VkDeviceSize deviceSize = (VkDeviceSize)extent.x * extent.y * colorChannels;
				Renderer::Buffer stagingBuffer(m_Engine.m_Renderer);
				if (!stagingBuffer.Create(deviceSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					PrintError(ErrorOrigin::Renderer, 
						"failed to create staging buffer for texture (function Renderer::Buffer::Create in function StaticTexture::Create!)");
					return false;
				}
				void* stagingMap;
				VkResult vkRes = vkMapMemory(renderer.m_VulkanDevice, 
					stagingBuffer.m_VulkanDeviceMemory, 0, deviceSize, 0, &stagingMap);
				memcpy(stagingMap, image, deviceSize);
				vkUnmapMemory(renderer.m_VulkanDevice, stagingBuffer.m_VulkanDeviceMemory);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to map staging buffer memory (function vkMapMemory in function StaticTexture::Create)!", vkRes);
					return false;
				}
				uint32_t queueFamilies[2] {
					renderer.m_GraphicsQueueFamilyIndex,
					renderer.m_TransferQueueFamilyIndex,
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
				vkRes = vkCreateImage(renderer.m_VulkanDevice, &imageInfo, renderer.m_VulkanAllocationCallbacks, &m_Image);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to create image (function vkCreateImage in function StaticTexture::Create)!", vkRes);
					return false;
				}
				VkMemoryRequirements memRequirements;
				vkGetImageMemoryRequirements(renderer.m_VulkanDevice, m_Image, &memRequirements);
				VkMemoryAllocateInfo imageAllocInfo {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.pNext = nullptr,
					.allocationSize = memRequirements.size,
				};
				if (!renderer.FindMemoryTypeIndex(memRequirements.memoryTypeBits, 
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageAllocInfo.memoryTypeIndex)) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to find memory type index (function Renderer::FindMemoryTypeIndex in function StaticTexture::Create)!");
					Terminate();
					return false;
				}
				vkRes = vkAllocateMemory(renderer.m_VulkanDevice, &imageAllocInfo, 
					renderer.m_VulkanAllocationCallbacks, &m_VulkanDeviceMemory);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to allocate image memory (function vkAllocateMemory in function StaticTexture::Create)!");
					Terminate();
					return false;
				}
				vkRes = vkBindImageMemory(renderer.m_VulkanDevice, m_Image, m_VulkanDeviceMemory, 0);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to bind image memory (function vkBindImageMemory in function StaticTexture::Create)!");
					Terminate();
					return false;
				}
				LockGuard earlyGraphicsCommandBufferQueueGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
				Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
					= renderer.m_EarlyGraphicsCommandBufferQueue.New();
				if (!commandBuffer) {
					PrintError(ErrorOrigin::OutOfMemory, 
						"renderer graphics command buffer queue was out of memory (function in function StaticTexture::Create)!");
					Terminate();
					return false;
				}
				if (!renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
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
				const Renderer& renderer = m_Engine.m_Renderer; 
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
				VkResult vkRes = vkCreateImageView(renderer.m_VulkanDevice, &imageViewInfo, renderer.m_VulkanAllocationCallbacks, &res);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to create image view (function vkCreateImageView in function StaticTexture::CreateImageView)");
					return VK_NULL_HANDLE;
				}
				return res;
			}

			void Terminate() {
				Renderer& renderer = m_Engine.m_Renderer;
				vkDestroyImage(renderer.m_VulkanDevice, m_Image, renderer.m_VulkanAllocationCallbacks);
				m_Image = VK_NULL_HANDLE;
				vkFreeMemory(renderer.m_VulkanDevice, m_VulkanDeviceMemory, renderer.m_VulkanAllocationCallbacks);
				m_VulkanDeviceMemory = VK_NULL_HANDLE;
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
				MaxEnum = Menu,
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
				MaxEnum = Eight,
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

			static inline Vec2_T<double> s_CursorPosition{};
			static inline Vec2_T<double> s_DeltaCursorPosition{};

			static void KeyCallback(GLFWwindow*, int key, int scancode, int action, int mods) {
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
			};

			Input(GLFWwindow* pGLFWwindow) {
				s_ActiveKeys.Reserve(key_count);
				s_ActiveMouseButtons.Reserve(mouse_button_count);
				glfwSetKeyCallback(pGLFWwindow, KeyCallback);
				glfwSetMouseButtonCallback(pGLFWwindow, MouseButtonCallback);
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
		public:

			friend class Engine;

			template<typename T>
			class Dictionary {
			public:

				static constexpr size_t max_bucket_size = 4;

				typedef const char* KeyType;
				typedef T ValueType;
				typedef KeyType KeyBucket[max_bucket_size];
				typedef ValueType ValueBucket[max_bucket_size];

				const size_t m_Capacity;
				KeyBucket* m_KeyBuckets;
				ValueBucket* m_ValueBuckets;
				uint8_t* m_BucketSizes;
				size_t m_Size;

				Dictionary(size_t capacity) 
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
					other.m_KeyBuckets = nullptr;
					other.m_ValueBuckets = nullptr;
					other.m_BucketSizes = nullptr;
				}

				~Dictionary() {
					free(m_KeyBuckets);
					free(m_ValueBuckets);
					free(m_BucketSizes);
				}

				bool Contains(const char* key) const {
					uint64_t hash = String::Hash()(key);
					size_t index = hash % m_Capacity;
					uint8_t bucketSize = m_BucketSizes[index];
					KeyBucket& keyBucket = m_KeyBuckets[index];
					for (size_t i = 0; i < bucketSize; i++) {
						if (!strcmp(key, keyBucket[i])) {
							return true;
						}
					}
					return false;
				}

				template<typename... Args>
				ValueType* Emplace(const char* key, Args&&... args) {
					if ((float)m_Size / m_Capacity > 0.9f) {
						PrintWarning("dictionary is close to full (in function UI::Dictionary::Emplace)!");
					}
					uint64_t hash = String::Hash()(key);
					size_t index = hash % m_Capacity;
					uint8_t& bucketSize = m_BucketSizes[index];
					KeyBucket& keyBucket = m_KeyBuckets[index];
					ValueBucket& valueBucket = m_ValueBuckets[index];
					if (bucketSize) {
						for (size_t i = 0; i < bucketSize; i++) {
							if (!strcmp(key, keyBucket[i])) {
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
						PrintWarning("dictionary is close to full (in function UI::Dictionary::Insert)!");
					}
					uint64_t hash = String::Hash()(key);
					size_t index = hash % m_Capacity;
					uint8_t& bucketSize = m_BucketSizes[index];
					KeyBucket& keyBucket = m_KeyBuckets[index];
					ValueBucket& valueBucket = m_ValueBuckets[index];
					if (bucketSize) {
						for (size_t i = 0; i < bucketSize; i++) {
							if (!strcmp(key, keyBucket[i])) {
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
			};

			struct Pipeline2D {

				static constexpr const char* vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(push_constant) uniform PushConstant {
	layout(offset = 0) mat4 c_Transform;
} pc;

void main() {
	outUV = inUV;
	gl_Position = pc.c_Transform * vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
}
				)";

				static constexpr const char* fragment_shader = R"(
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushConstant {
	layout(offset = 64) uint c_TextureIndex;
} pc;

void main() {
	outColor = texture(textures[nonuniformEXT(pc.c_TextureIndex)], inUV);
}
				)";

				static constexpr uint32_t max_texture_sets = 500;

				VkPipeline m_Pipeline{};
				VkPipelineLayout m_PipelineLayout{};
				VkDescriptorSetLayout m_DescriptorSetLayout{};
				VkSampler m_TextureSampler{};
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

				void CalcTransform(Vec2_T<uint32_t> renderResolution, Mat4& out) const {
					out.columns[0] = Vec4((float)(m_Max.x - m_Min.x) / renderResolution.x, 0.0f, 0.0f, 0.0f);
					out.columns[1] = Vec4(0.0f, (float)(m_Max.y - m_Min.y) / renderResolution.y, 0.0f, 0.0f);
					out.columns[2] = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
					Vec2 pos = Middle();
					out.columns[3] = Vec4(pos.x / renderResolution.x * 2.0f - 1.0f, pos.y / renderResolution.y * 2.0f - 1.0f, 0.0f, 1.0f);
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
					m_Rect.CalcTransform(m_UI.m_Engine.GetRenderResolution(), m_Transform);
				}

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
					res->m_Rect.CalcTransform(m_UI.m_Engine.GetRenderResolution(), m_Transform);
					return m_Buttons.PushBack(res);
				}

				void Render(VkCommandBuffer commandBuffer) const {
					VkDescriptorSet texDescriptor = VK_NULL_HANDLE;
					uint32_t texIndex = 0;
					VkPipelineLayout pipelineLayout = m_UI.m_Pipeline2D.m_PipelineLayout;
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UI.m_Pipeline2D.m_Pipeline);
					if (m_Pipeline2DRenderCallback && m_Pipeline2DRenderCallback(*this, texDescriptor, texIndex)) {
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
							0, 1, &texDescriptor, 0, nullptr);
						vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &m_Transform);
						vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16, &texIndex);
						vkCmdBindVertexBuffers(commandBuffer, 0, 1, m_UI.m_QuadMesh2DData.m_VertexBuffers, 
							m_UI.m_QuadMesh2DData.m_VertexBufferOffsets);
						vkCmdBindIndexBuffer(commandBuffer, m_UI.m_QuadMesh2DData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
						vkCmdDrawIndexed(commandBuffer, m_UI.m_QuadMesh2DData.m_IndexCount, 1, 0, 0, 0);
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

				void RenderResolutionChange(Vec2_T<uint32_t> renderResolution) {
					m_Rect.CalcTransform(renderResolution, m_Transform);
					for (Button* button : m_Buttons) {
						assert(button);
						button->m_Rect.CalcTransform(renderResolution, button->m_Transform);
					}
				}

				IntVec2 GetPosition() {
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
					Vec2_T<uint32_t> renderResolution = m_UI.m_Engine.GetRenderResolution();
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

		private:

			static inline DynamicArray<GLFWwindow*> s_GLFWwindows{};
			static inline DynamicArray<UI*> s_UIs{};

			static UI* FindUI(const GLFWwindow* pGlfwWindow) {
				size_t count = s_GLFWwindows.m_Size;
				for (size_t i = 0; i < count; i++) {
					if (s_GLFWwindows[i] == pGlfwWindow) {
						return s_UIs[i];
					}
				}
				return nullptr;
			}

		public:

			MeshData m_QuadMesh2DData{};

			Dictionary<Window> m_WindowLookUp;
			DynamicArray<Window*> m_Windows{};

			IntVec2 m_CursorPosition{};

			Engine& m_Engine;
			Pipeline2D m_Pipeline2D{};

			UI(Engine& engine, size_t maxWindows) 
				: m_Engine(engine), m_Pipeline2D(), m_WindowLookUp(maxWindows * 2) {
				m_Windows.Reserve(maxWindows);
				s_UIs.Reserve(4);
				s_GLFWwindows.Reserve(4);
			}

		private:

			void Initialize() {

				s_UIs.PushBack(this);
				GLFWwindow* pGLFWwindow = s_GLFWwindows.PushBack(m_Engine.m_Renderer.m_Window);

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

				m_QuadMesh2DData = m_Engine.m_StaticQuadMesh2D.GetMeshData();

				Renderer& renderer = m_Engine.m_Renderer;

				{

					Renderer::Shader vertexShader(renderer);
					Renderer::Shader fragmentShader(renderer);

					if (!vertexShader.Compile(Pipeline2D::vertex_shader, VK_SHADER_STAGE_VERTEX_BIT)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to compile vertex shader for UI::Pipeline2D (function Renderer::Shader::Compile in function UI::Initialize)!");
					}
					if (!fragmentShader.Compile(Pipeline2D::fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to compile vertex shader for UI::Pipeline2D (function Renderer::Shader::Compile in function UI::Initialize)!");
					}

					VkShaderModule vertexShaderModule = vertexShader.CreateShaderModule();
					VkShaderModule fragmentShaderModule = fragmentShader.CreateShaderModule();

					const VkPipelineShaderStageCreateInfo shaderStages[2] {
						{
							.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							.stage = VK_SHADER_STAGE_VERTEX_BIT,
							.module = vertexShaderModule,
							.pName = "main",
							.pSpecializationInfo = nullptr,
						},
						{
							.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
							.module = fragmentShaderModule,
							.pName = "main",
							.pSpecializationInfo = nullptr,
						},
					};

					VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
						.primitiveRestartEnable = VK_FALSE,
					};

					VkPipelineViewportStateCreateInfo viewPortStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.viewportCount = 1,
						.pViewports = nullptr,
						.scissorCount = 1,
						.pScissors = nullptr,
					};

					VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.depthClampEnable = VK_FALSE,
						.rasterizerDiscardEnable = VK_FALSE,
						.polygonMode = VK_POLYGON_MODE_FILL,
						.cullMode = VK_CULL_MODE_NONE,
						.frontFace = VK_FRONT_FACE_CLOCKWISE,
						.depthBiasClamp = VK_FALSE,
						.lineWidth = 1.0f,
					};

					VkPipelineMultisampleStateCreateInfo multisampleStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT /* renderer.GetMaxColorSamples() */,
						.sampleShadingEnable = VK_FALSE,
						.minSampleShading = 0.2f,
						.pSampleMask = nullptr,
						.alphaToCoverageEnable = VK_FALSE,
						.alphaToOneEnable = VK_FALSE,
					};

					VkPipelineColorBlendAttachmentState colorBlendAttachment {
						.blendEnable = VK_TRUE,
						.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.colorBlendOp = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.alphaBlendOp = VK_BLEND_OP_ADD,
						.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
					};

					VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.logicOpEnable = VK_FALSE,
						.attachmentCount = 1,
						.pAttachments = &colorBlendAttachment,
						.blendConstants = { 0, 0, 0, 0 },
					};

					VkDynamicState dynamicStates[2] {
						VK_DYNAMIC_STATE_VIEWPORT,
						VK_DYNAMIC_STATE_SCISSOR,
					};

					VkPipelineDynamicStateCreateInfo dynamicStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.dynamicStateCount = 2,
						.pDynamicStates = dynamicStates,
					};

					VkPipelineRenderingCreateInfo renderingInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
						.pNext = nullptr,
						.viewMask = 0,
						.colorAttachmentCount = 1,
						.pColorAttachmentFormats = &renderer.m_SwapchainSurfaceFormat.format,
						.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
						.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
					};

					VkPushConstantRange pushConstantRanges[2] {
						{
							.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
							.offset = 0,
							.size = 64,
						},
						{
							.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
							.offset = 64,
							.size = 16,
						},
					};

					VkPipelineLayoutCreateInfo pipelineLayoutInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.setLayoutCount = 0,
						.pSetLayouts = nullptr,
						.pushConstantRangeCount = 2,
						.pPushConstantRanges = pushConstantRanges,
					};

					VkDescriptorSetLayoutBinding descriptorSetLayoutBinding {
						.binding = 0,
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = 64,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
						.pImmutableSamplers = nullptr,
					};

					VkDescriptorBindingFlags textureArrayDescriptorBindingFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

					VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsInfo {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
						.pNext = nullptr,
						.bindingCount = 1,
						.pBindingFlags = &textureArrayDescriptorBindingFlags,
					};

					m_Pipeline2D.m_DescriptorSetLayout 
						= renderer.CreateDescriptorSetLayout(&descriptorSetLayoutBindingFlagsInfo, 1, &descriptorSetLayoutBinding);

					if (m_Pipeline2D.m_DescriptorSetLayout == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create descriptor set layout for 2D UI pipeline (function Renderer::CreateDescriptorSetLayout in function UI::Initialize)!");
					}

					m_Pipeline2D.m_PipelineLayout = renderer.CreatePipelineLayout(1, &m_Pipeline2D.m_DescriptorSetLayout, 2, pushConstantRanges);

					if (m_Pipeline2D.m_PipelineLayout == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create pipeline layout for 2D UI pipeline (function Renderer::CreatePipelineLayout in function UI::Initialize)!");
					}

					VkGraphicsPipelineCreateInfo pipelineInfo {
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &renderingInfo,
						.flags = 0,
						.stageCount = 2,
						.pStages = shaderStages,
						.pVertexInputState = &Vertex2D::GetVertexInputState(),
						.pInputAssemblyState = &inputAssemblyStateInfo,
						.pTessellationState = nullptr,
						.pViewportState = &viewPortStateInfo,
						.pRasterizationState = &rasterizationStateInfo,
						.pMultisampleState = &multisampleStateInfo,
						.pDepthStencilState = nullptr,
						.pColorBlendState = &colorBlendStateInfo,
						.pDynamicState = &dynamicStateInfo,
						.layout = m_Pipeline2D.m_PipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					};
					if (!renderer.CreateGraphicsPipelines(1, &pipelineInfo, &m_Pipeline2D.m_Pipeline)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create UI pipelines (function Renderer::CreateGraphicsPipelines in function UI::Initialize)!");
					}
					vkDestroyShaderModule(renderer.m_VulkanDevice, vertexShaderModule, renderer.m_VulkanAllocationCallbacks);
					vkDestroyShaderModule(renderer.m_VulkanDevice, fragmentShaderModule, renderer.m_VulkanAllocationCallbacks);
				}

				VkSamplerCreateInfo samplerInfo2D {
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.magFilter = VK_FILTER_LINEAR,
					.minFilter = VK_FILTER_LINEAR,
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
					.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
					.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
					.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
					.mipLodBias = 0.0f,
					.anisotropyEnable = VK_FALSE,
					.maxAnisotropy = 0.0f,
					.compareEnable = VK_FALSE,
					.compareOp = VK_COMPARE_OP_NEVER,
					.minLod = 0.0f,
					.maxLod = VK_LOD_CLAMP_NONE,
					.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
					.unnormalizedCoordinates = VK_FALSE,
				};
				VkResult vkRes = vkCreateSampler(renderer.m_VulkanDevice, &samplerInfo2D, 
					renderer.m_VulkanAllocationCallbacks, &m_Pipeline2D.m_TextureSampler);
				if (vkRes != VK_SUCCESS) {
					CriticalError(ErrorOrigin::Vulkan, 
						"failed to create texture sampler for UI::Pipeline2D (function vkCreateSampler in function UI::Initialize)!",
						vkRes);
				}
			}

		public:

			UI(const UI&) = delete;
			UI(UI&&) = delete;	

			void Render(const Renderer::DrawData& drawData) {
				VkRenderingAttachmentInfo colorAttachment {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = drawData.m_SwapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue { 0, 0, 0, 0 },
				};
				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea {
						.offset = { 0, 0 },
						.extent = m_Engine.m_Renderer.m_SwapchainExtent,
					},
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment,
					.pDepthAttachment = nullptr,
					.pStencilAttachment = nullptr,
				};
				vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
				for (Window* window : m_Windows) {
					window->Render(drawData.m_CommandBuffer);
				}
				vkCmdEndRendering(drawData.m_CommandBuffer);
			}

			template<uint32_t texture_count_T>
			bool CreateTexture2DArray(const VkImageView imageViews[texture_count_T], VkDescriptorSet& outDescriptorSet, 
				VkDescriptorPool& outDescriptorPool)
			{
				Renderer& renderer = m_Engine.m_Renderer;
				VkDescriptorPoolSize poolSize {
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = texture_count_T,
				};
				outDescriptorPool = renderer.CreateDescriptorPool(0, 1, 1, &poolSize);
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
				if (!renderer.AllocateDescriptorSets(&descriptorCountAllocInfo, outDescriptorPool, 1,
						&m_Pipeline2D.m_DescriptorSetLayout, &outDescriptorSet)) {
					PrintError(ErrorOrigin::Renderer, 
						"failed to allocate descriptor sets (function Renderer::AllocateDescriptorSets in function UI::CreateTexture2DArray)!");
					renderer.DestroyDescriptorPool(outDescriptorPool);
					outDescriptorPool = VK_NULL_HANDLE;
					return false;
				}
				VkDescriptorImageInfo imageInfos[texture_count_T]{};
				for (uint32_t i = 0; i < texture_count_T; i++) {
					imageInfos[i] = {
						.sampler = m_Pipeline2D.m_TextureSampler,
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
				renderer.UpdateDescriptorSets(1, &write);
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

			void SwapchainCreateCallback(Vec2_T<uint32_t> renderResolution) {
				for (Window* window : m_Windows) {
					assert(window);
					window->RenderResolutionChange(renderResolution);
				}
			}

			void Terminate() {
				m_Engine.m_Renderer.DestroySampler(m_Pipeline2D.m_TextureSampler);
				m_Engine.m_Renderer.DestroyPipeline(m_Pipeline2D.m_Pipeline);
				m_Engine.m_Renderer.DestroyPipelineLayout(m_Pipeline2D.m_PipelineLayout);
				m_Engine.m_Renderer.DestroyDescriptorSetLayout(m_Pipeline2D.m_DescriptorSetLayout);
			}

		public:
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
							uint32_t& vtI = m_VtIndices.EmplaceBack();
							int res = fscanf(fileStream, "%i/%i", &vI, &vtI);
							if (res != 2) {
								m_LinesParsed = 0;
								return false;
							}
							--vI;
							--vtI;
							maxVIndex = Max(vI, maxVIndex);
							maxVtIndex = Max(vtI, maxVtIndex);
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
				if (!(m_VIndices.m_Size == m_VtIndices.m_Size &&
					(!m_VnIndices.m_Size || m_VnIndices.m_Size == m_VtIndices.m_Size) && !(m_VtIndices.m_Size % 3) &&
					maxVIndex < m_Vs.m_Size && maxVtIndex < m_Vts.m_Size && maxVnIndex < m_Vns.m_Size)) {
					m_LinesParsed = 0;
					return false;
				}
				return true;
			}

			const DynamicArray<uint32_t>& GetIndices() const {
				return m_VIndices;
			}

			template<typename VertexType>
			bool GetMesh(void (*setPos)(Vertex&, const Vec3&), void (*setUV)(Vertex&, const Vec2&), 
					void (*setNormal)(Vertex&, const Vec3&), DynamicArray<VertexType>& outVertices, DynamicArray<uint32_t>& outIndices) const {
				if (m_LinesParsed == 0) {
					PrintError(ErrorOrigin::FileParsing, 
						"attempting to get vertices from Engine::Obj which failed to parse (in function Obj::GetVertices)!");
					return false;
				}
				if (!setPos || !setUV || !setNormal && m_VnIndices.m_Size) {
					PrintError(ErrorOrigin::FileParsing, 
						"attempting to get vertices from an obj when a set function is null!");
					return false;
				}
				outVertices.Reserve(m_Vs.m_Size);
				outIndices.Reserve(m_VIndices.m_Size);
				for (uint32_t i = 0; i < m_VIndices.m_Size; i++) {
					VertexType newVertex{};
					setPos(newVertex, m_Vs[m_VIndices[i]]);
					setUV(newVertex, m_Vts[m_VtIndices[i]]);
					if (m_VnIndices.m_Size) {
						setNormal(newVertex, m_Vns[m_VnIndices[i]]);
					}
					size_t j = 0;
					for (; j < outVertices.m_Size; j++) {
						if (outVertices[j] == newVertex) {
							break;
						}
					}
					if (j == outVertices.m_Size) {
						outVertices.PushBack(newVertex);
					}
					outIndices.PushBack(j);
				}
				return true;
			}
		};

		static bool LoadImage(const char* fileName, uint32_t components, uint8_t*& outPixels, Vec2_T<uint32_t>& outExtent) {
			int x, y, texChannels;
			outPixels = stbi_load(fileName, &x, &y, &texChannels, components);
			if (!outPixels) {
				PrintError(ErrorOrigin::Stb, 
					"failed to load image (function stbi_load in function LoadImage)!");
				return false;
			}
			outExtent = { (uint32_t)x, (uint32_t)y };
			return true;
		}

		static void FreeImage(uint8_t* pixels) {
			free(pixels);
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

		template<typename T>
		struct Field;

		template<typename T>
		class Reference {
		public:	

				static_assert(std::is_base_of<Field<T>, T>());

				T* m_Val;

				Reference() noexcept : m_Val(nullptr) {}

				Reference(T& val) noexcept : m_Val(&val) {
					m_Val->AddReference(this);
				}

				Reference(const Reference& other) noexcept : m_Val(other.m_Val) {
					if (m_Val) {
						m_Val->AddReference(this);
					}
				}

				~Reference() {
					RemoveReference();
				}

				void SetReference(T& val) {
					RemoveReference();
					m_Val = &val;
					m_Val->AddReference(this);
				}

				void RemoveReference() {
					if (m_Val) {
						m_Val->RemoveReference(this);
						m_Val = nullptr;
					}
				}
				
				T& operator*() {
					if (!m_Val) {
						CriticalError(ErrorOrigin::NullDereference,
							"attempting to deference null reference (in Reference::operator*)");
					}
					return *m_Val;
				}
		};

		template<typename T>
		class Field {
			
			friend class Reference<T>;
				
		public:

			DynamicArray<Reference<T>*> m_References;

			Field() : m_References() {}

			Field(Field&& other) : m_References(std::move(other.m_References)) {
				for (Reference<T>* reference : m_References) {
					assert(reference);
					reference->m_Val = (T*)this;
				}
			}

			virtual ~Field() {
				for (Reference<T>* reference : m_References) {
					assert(reference);
					reference->m_Val = nullptr;
				}
			}

		private:

			bool AddReference(Reference<T>* reference) {
				if (reference) {
					m_References.PushBack(reference);
					return true;
				}
				return false;
			}

			bool RemoveReference(Reference<T>* reference) {
				if (reference) {
					auto end = m_References.end();
					for (auto iter = m_References.begin(); iter != end; iter++) {
						if (*iter == reference) {
							m_References.Erase(iter);
							return true;
						}
					}
				}
				return false;
			}
		};

		/*
		struct RectCollider {

			friend class Engine;

		private:

			Vec3 m_Position;
			Quaternion m_Rotation;
			Vec3 m_Dimensions;
			Vec3 m_Corners[8]{};
			Box<float> m_AABB{};

		public:

			RectCollider(const Vec3& position = {}, const Quaternion& rotation = {}, const Vec3& dimensions = {})
				: m_Position(position), m_Rotation(rotation), m_Dimensions(dimensions) {
				CalcCorners();
				CalcAABB();
			}

			RectCollider(const RectCollider&) = delete;
			
			RectCollider(RectCollider&&) = default;

		private:

			void CalcCorners() {

				Vec3 halfDimensionsPos = m_Dimensions / 2;
				Vec3 halfDimensionsNeg = -halfDimensionsPos;

				m_Corners[0].x = halfDimensionsPos.x;
				m_Corners[0].y = halfDimensionsPos.y;
				m_Corners[0].z = halfDimensionsPos.z;

				m_Corners[1].x = halfDimensionsNeg.x;
				m_Corners[1].y = halfDimensionsNeg.y;
				m_Corners[1].z = halfDimensionsNeg.z;

				m_Corners[2].x = halfDimensionsNeg.x;
				m_Corners[2].y = halfDimensionsPos.y;
				m_Corners[2].z = halfDimensionsPos.z;

				m_Corners[3].x = halfDimensionsPos.x;
				m_Corners[3].y = halfDimensionsNeg.y;
				m_Corners[3].z = halfDimensionsPos.z;

				m_Corners[4].x = halfDimensionsPos.x;
				m_Corners[4].y = halfDimensionsPos.y;
				m_Corners[4].z = halfDimensionsNeg.z;

				m_Corners[5].x = halfDimensionsPos.x;
				m_Corners[5].y = halfDimensionsNeg.y;
				m_Corners[5].z = halfDimensionsNeg.z;

				m_Corners[6].x = halfDimensionsNeg.x;
				m_Corners[6].y = halfDimensionsPos.y;
				m_Corners[6].z = halfDimensionsNeg.z;

				m_Corners[7].x = halfDimensionsNeg.x;
				m_Corners[7].y = halfDimensionsNeg.y;
				m_Corners[7].z = halfDimensionsPos.z;

				Mat3 transform(m_Rotation.AsMat4());

				for (size_t i = 0; i < 8; i++) {
					m_Corners[i] = m_Corners[i] * transform + m_Position;
				}
			}

			void CalcAABB() {
				static constexpr float max_float = std::numeric_limits<float>::max();
				static constexpr float min_float = -max_float;
				m_AABB.m_Min = { max_float, max_float, max_float };
				m_AABB.m_Max = { min_float, min_float, min_float };
				for (size_t i = 0; i < 8; i++) {
					Vec3& corner = m_Corners[i];
					m_AABB.m_Min.x = Min(corner.x, m_AABB.m_Min.x);
					m_AABB.m_Min.y = Min(corner.y, m_AABB.m_Min.y);
					m_AABB.m_Min.z = Min(corner.z, m_AABB.m_Min.z);
					m_AABB.m_Max.x = Max(corner.x, m_AABB.m_Max.x);
					m_AABB.m_Max.y = Max(corner.y, m_AABB.m_Max.y);
					m_AABB.m_Max.z = Max(corner.z, m_AABB.m_Max.z);
				}
			}

		public:	

			void SetPosition(const Vec3& position) {
				m_Position = position;
				CalcCorners();
				CalcAABB();
			}

			void SetRotation(const Quaternion& rotation) {
				m_Rotation = rotation;
				CalcCorners();
				CalcAABB();
			}

			void SetPositionAndRotation(const Vec3& position, const Quaternion& rotation) {
				m_Position = position;
				m_Rotation = rotation;
				CalcCorners();
				CalcAABB();
			}

			bool CheckCollision(const BoxCollider& collider, Vec3& outPushBack) const {
				if (!m_AABB.OverLaps(collider.m_AABB)) {
					return false;
				}
				fmt::print("colliding!");
				return true;
			}
		};
		*/

		class Collider {

			friend class Engine;
		
		public:

			enum class Type {
				Fence = 1,
				Pole = 2,
			};

			struct Fence {

				struct CreateInfo {
					Vec3 m_Dimensions{};
					float m_YRotation{};
				};

				void Create(const CreateInfo& info) {
					m_HalfDimensions = info.m_Dimensions / 2;
					m_YRotation = info.m_YRotation;
					m_RotationMatrix = Quaternion::AxisRotation(Vec3::Forward(), m_YRotation).AsMat4();
				}

				Vec3 m_HalfDimensions{};
				float m_YRotation{};
				Mat3 m_RotationMatrix{};
			};

			struct Pole {

				struct CreateInfo {
					float m_Radius{};
					float m_Height{};
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
					}
				}

				Fence m_Fence;
				Pole m_Pole;
			};

			struct CreateInfo {
				Vec3 m_LocalPosition;
				Type m_Type;
				TypeCreateInfo_U u_TypeInfo;
			};

		private:

			const Type m_Type;
			Collider_U u_Collider;
			Vec3 m_LocalPosition{};
			const Vec3& m_BodyPosition;

			Collider(Vec3& bodyPosition, const CreateInfo& info) 
				: m_BodyPosition(bodyPosition), m_LocalPosition(info.m_LocalPosition), m_Type(info.m_Type), u_Collider(info.m_Type, info.u_TypeInfo) {
			}
	
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
					Vec3 aPosRelRotated = (Vec3)aPosRel * bFence.m_RotationMatrix;
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
						Mat3 invRot = Quaternion::AxisRotation(Vec3(0.0f, 0.0f, 1.0f), -bFence.m_YRotation).AsMat4();
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

					const Line aLines[2] { 
						{ aPos2D , (Vec3::Right() * aFence.m_RotationMatrix).Normalized() }, 
						{ aPos2D, (Vec3::Up() * aFence.m_RotationMatrix).Normalized() },
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
						{ bPos2D, (Vec3::Right() * bFence.m_RotationMatrix).Normalized() },
						{ bPos2D, (Vec3::Up() * bFence.m_RotationMatrix).Normalized() },
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
						const Vec2 signedDistances(get_signed_distance(aPos2D, aLines[0], bCorners[i], thisRelX), get_signed_distance(aPos2D, aLines[1], bCorners[i], thisRelY));
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
						const Vec2 signedDistances(get_signed_distance(bPos2D, bLines[0], aCorners[i], thisRelX), get_signed_distance(bPos2D, bLines[1], aCorners[i], thisRelY));
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

		struct GroundInfo {
			Rect<float> m_TopViewBoundingRect{};
		};

		struct Ground : Field<Ground> {

			friend class Engine;

		private:

			const uint64_t m_ObjectID;
			Rect<float> m_TopViewBoundingRect;
			Vec3 m_Scale;
			Vec3 m_CenterPosition;
			Vec2_T<uint32_t> m_HeightMapExtent;
			uint32_t m_HeightMapMin;
			uint32_t m_HeightMapMax;
			uint32_t* m_HeightMap;

			Ground(const GroundInfo& groundInfo, uint64_t objectID) 
				: Field<Ground>(), m_ObjectID(objectID), m_TopViewBoundingRect(groundInfo.m_TopViewBoundingRect) {}

			Ground(const Ground&) = delete;

			Ground(Ground&& other) noexcept = default;

		public:

			/*
			void SetScale(const Vec3& scale) {
				m_BoundingBox.m_Max = m_BoundingBox.m_Min + 
					Vec3(m_HeightMapExtent.x * scale.x,
						(m_HeightMapMax - m_HeightMapMin) * scale.y,
						m_HeightMapExtent.y * scale.z);
				m_Scale = scale;
			}
			*/

			float GetHeightAtPosition(const Vec3& position) const {
				Vec2 pos2D(position.x, position.z);
				if (!m_TopViewBoundingRect.IsPointInside(pos2D)) {
					return std::numeric_limits<float>::max();
				}
				return 0.0f;
				Vec3 relative = pos2D - m_TopViewBoundingRect.m_Min;
				Vec2 dimensions = m_TopViewBoundingRect.Dimensions();
				Vec2_T<uint32_t> heightMapCoords(relative.x / dimensions.x * m_HeightMapExtent.x,
					relative.y / dimensions.y * m_HeightMapExtent.y);
				size_t index = heightMapCoords.x * m_HeightMapExtent.y + heightMapCoords.y;
				size_t heightMapSize = m_HeightMapExtent.x * m_HeightMapExtent.y;
				assert(index < heightMapSize);
				return (float)m_HeightMap[index] / UINT32_MAX * m_Scale.y + m_CenterPosition.y;
			}
		};

		struct ObstacleInfo {
			Vec3 m_Position{};
			Quaternion m_Rotation{};
			Vec3 m_Dimensions{};
			Collider::CreateInfo m_ColliderInfo{};
		};

		struct Obstacle : Field<Obstacle> {

			friend class Engine;

		private:

			uint64_t m_ObjectID;
			Vec3 m_Position;
			Collider m_Collider;

		public:

			Obstacle(const ObstacleInfo& info, uint64_t objectID) noexcept 
				: Field<Obstacle>(), m_ObjectID(objectID), m_Position(info.m_Position), m_Collider(m_Position, info.m_ColliderInfo) {}

			Obstacle(const Obstacle&) = delete;

			Obstacle(Obstacle&& other) noexcept = default;

			bool Collides(const Collider& collider, const Vec3& colliderVelocity, Vec3& outColliderPushBack) const {
				return Collider::ColliderToStaticColliderCollides(collider, colliderVelocity, m_Collider, outColliderPushBack);
			}
		};

		class Chunk {

			friend class Engine;

			const Rect<float> m_BoundingRect;
			const Vec2_T<uint32_t> m_ChunkMatrixCoords;
			DynamicArray<Reference<Ground>> m_Grounds{};
			DynamicArray<Reference<Obstacle>> m_Obstacles{};
			
			Chunk(Vec2_T<uint32_t> chunkCoords, Vec2 min, Vec2 dimensions) noexcept 
				: m_BoundingRect { .m_Min { min }, .m_Max { min + dimensions } }, m_ChunkMatrixCoords(chunkCoords), m_Grounds() {}

			bool IsPointInside(const Vec3& point) const {
				return m_BoundingRect.IsPointInside(Vec2(point.x, point.z));
			}

			const Ground* FindGround(const Vec3& oldPosition, const Vec3& deltaPosition, Vec2_T<bool>& outAxisBlocked) const {
				outAxisBlocked = { false, false };
				Vec2 newPosTopView(oldPosition.x + deltaPosition.x, oldPosition.z + deltaPosition.z);
				Vec2 oldPositionTopView(oldPosition.x, oldPosition.z);
				for (const Reference<Ground>& ground : m_Grounds) {
					if (!ground.m_Val) {
						continue;
					}
					Rect<float> boundingBox = ground.m_Val->m_TopViewBoundingRect;
					if (boundingBox.IsPointInside(newPosTopView)) {
						return ground.m_Val;
					}
					else if (boundingBox.IsPointInside(oldPositionTopView)) {
						outAxisBlocked = {
							boundingBox.m_Min.x >= newPosTopView.x || boundingBox.m_Max.x <= newPosTopView.x,
							boundingBox.m_Min.y >= newPosTopView.y || boundingBox.m_Max.y <= newPosTopView.y,
						};
						return ground.m_Val;
					}
				}
				return nullptr;
			}
		};

		class Creature : Field<Creature> {

			friend class Engine;

		private:

			const Chunk* m_Chunk;
			Vec3 m_Position;
			const uint64_t m_ObjectID;
			Collider m_Collider;

			Creature(uint64_t objectID, const Vec3& position, const Chunk* chunk, const Collider::CreateInfo& colliderInfo)
				: Field<Creature>(), m_Position(position), m_Chunk(chunk), m_ObjectID(objectID),
					m_Collider(m_Position, colliderInfo) {
				assert(chunk);
				Vec2_T<bool> _;
				const Ground* ground = chunk->FindGround(position, {}, _);
				m_Position.y = ground ? ground->GetHeightAtPosition(position) : 0.0f;
			}

			Creature(const Creature&) = delete;

			Creature(Creature&& other) noexcept = default;

		public:

			Vec3 (*m_MovementVectorUpdate)(const Creature& creature){};
			void (*m_MoveCallback)(const Creature& creature, const Vec3& position, const Vec3& deltaPosition){};
			void (*m_CameraFollowCallback)(const Creature& creature, Mat4& outViewMatrix);

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
				Vec2_T<bool> axisBlocked;
				const Ground* ground = m_Chunk->FindGround(m_Position, deltaPos, axisBlocked);
				if (ground) {
					m_Position.x = !axisBlocked.x ? position.x : m_Position.x;
					m_Position.z = !axisBlocked.y ? position.z : m_Position.z; 
					float height = ground->GetHeightAtPosition(m_Position);
					assert(height != std::numeric_limits<float>::max());
					m_Position.y = height;
				}
				for (const Reference<Obstacle>& obstacle : m_Chunk->m_Obstacles) {
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

		class World {

			friend class Engine;

		public:

			struct Pipelines {

				VkPipeline m_DrawPipelinePBR = VK_NULL_HANDLE;
				VkPipelineLayout m_DrawPipelineLayoutPBR = VK_NULL_HANDLE;

				VkPipeline m_RenderPipelinePBR = VK_NULL_HANDLE;
				VkPipelineLayout m_RenderPipelineLayoutPBR = VK_NULL_HANDLE;

				VkPipeline m_DebugPipeline = VK_NULL_HANDLE;
				VkPipelineLayout m_DebugPipelineLayout = VK_NULL_HANDLE;
				VkDescriptorSetLayout m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
				VkDescriptorSetLayout m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;
			};

			struct RenderData : Field<RenderData> {

				friend class Engine;

			private:

	
				RenderData(uint64_t objectID, const Mat4& transform, const MeshData& meshData) noexcept 
					: Field<RenderData>(), m_ObjectID(objectID), m_Transform(transform), m_MeshData(meshData) {}

				RenderData(const RenderData&) = delete;

				RenderData(RenderData&& other) noexcept = default;
	
				const uint64_t m_ObjectID;

			public:

				Mat4 m_Transform;
				MeshData m_MeshData;
			};

			struct DebugRenderData : Field<DebugRenderData> {

				friend class Engine;

			private:

				DebugRenderData(uint64_t objectID, const Mat4& transform, const Vec4& wireColor, const MeshData& meshData) noexcept 
					: Field<DebugRenderData>(), m_ObjectID(objectID), m_Transform(transform), m_WireColor(wireColor), m_MeshData(meshData) {}

				DebugRenderData(const DebugRenderData&) = delete;

				DebugRenderData(DebugRenderData&& other) noexcept = default;

				const uint64_t m_ObjectID;

			public:

				Mat4 m_Transform;
				Vec4 m_WireColor;
				MeshData m_MeshData;
			};

			struct CameraMatricesBuffer {
				Mat4 m_Projection;
				Mat4 m_View;
			};

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

		outUV = inUV;

		mat4 normalMat = transpose(inverse(pc.c_Transform));

		outPosition = vec3(pc.c_Transform * vec4(inPosition, 1.0f));
		outNormal = normalize(vec3(normalMat * vec4(inNormal, 0.0f)));

		gl_Position = camera_matrices.c_Projection * camera_matrices.c_View * pc.c_Transform * vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
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

	void main() {
		outDiffuseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		outPositionAndMetallic = vec4(inPosition, 1.0f);
		outNormalAndRougness = vec4(inNormal, 1.0f);
	}
			)";

			static constexpr const char* pbr_render_pipeline_vertex_shader = R"(
	#version 450

	layout(location = 0) in vec3 inPosition;
	layout(location = 1) in vec2 inUV;

	layout(location = 0) out vec2 outUV;

	layout(push_constant) uniform PushConstant {
		mat4 c_Transform;
	} pc;

	void main() {
		outUV = inUV;
		gl_Position = pc.c_Transform * vec4(vec3(inPosition.x, -inPosition.y, inPosition.z), 1.0f);
	}
			)";

			static constexpr const char* pbr_render_pipeline_fragment_shader = R"(
	#version 450

	layout(location = 0) in vec2 inUV;

	layout(location = 0) out vec4 outColor;

	layout(set = 0, binding = 0) uniform sampler2D diffuse_colors;
	layout(set = 0, binding = 1) uniform sampler2D position_and_metallic;
	layout(set = 0, binding = 2) uniform sampler2D normal_and_roughness;

	void main() {

		const vec3 point_light_pos = vec3(0.0f, 3.0f, 0.0f);

		vec3 pos = vec3(texture(position_and_metallic, inUV));
		vec3 normal = vec3(texture(normal_and_roughness, inUV));

		const vec3 light_dir = normalize(point_light_pos - pos);

		float diff = max(dot(normal, light_dir), 0.0f);

		vec3 diffuse = diff * vec3(1.0f, 0.5f, 0.5f);
	
		vec3 color = (vec3(0.2f, 0.2f, 0.2f) + diffuse) * vec3(texture(diffuse_colors, inUV));

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

		private:

			Engine& m_Engine;

			uint64_t m_NextObjectID{};
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
			Mat4 m_RenderPBRQuadTransform{1};
			MeshData m_StaticQuadMeshDataPBR;
			
			DynamicArray<DebugRenderData> m_DebugRenderDatas{};

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
			DynamicArray<VkSampler> m_RenderPBRImagesSamplers{};
			Renderer::Buffer m_CameraMatricesBuffer;

			World(Engine& engine) : m_Engine(engine), m_CameraMatricesBuffer(m_Engine.m_Renderer) {}

			World(const World&) = delete;
			World(World&&) = delete;

			void Initialize() {

				m_StaticQuadMeshDataPBR = m_Engine.m_StaticQuadMesh2D.GetMeshData();

				Renderer& renderer = m_Engine.m_Renderer;
				// world pipeline
				VkDescriptorSetLayoutBinding vertexUniformBufferDescriptorSetBinding {
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.pImmutableSamplers = nullptr,
				};

				m_Pipelines.m_CameraDescriptorSetLayout = renderer.CreateDescriptorSetLayout(nullptr, 1, &vertexUniformBufferDescriptorSetBinding);

				if (m_Pipelines.m_CameraDescriptorSetLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create camera descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::Initialize)!");
				}

				VkPushConstantRange pbrDrawPushConstantRange {
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = 128,
				};

				m_Pipelines.m_DrawPipelineLayoutPBR = renderer.CreatePipelineLayout(1, &m_Pipelines.m_CameraDescriptorSetLayout, 1, &pbrDrawPushConstantRange);

				if (m_Pipelines.m_DrawPipelineLayoutPBR == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create pbr draw pipeline layout for world (function Renderer::CreatePipelineLayout in function World::Initialize)!");
				}

				VkPushConstantRange pbrRenderPushConstantRange {
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = 64,
				};

				m_Pipelines.m_RenderPipelineLayoutPBR = renderer.CreatePipelineLayout(1, &m_Pipelines.m_RenderPBRImagesDescriptorSetLayout, 1, &pbrRenderPushConstantRange);

				if (m_Pipelines.m_RenderPipelineLayoutPBR == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"faileld to create pbr render pipeline layout for world (function Renderer::CreatePipelineLayout in function World::Initialize)!");
				}

				VkPushConstantRange debugPushConstantRanges[2] {
					{
						.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
						.offset = 0,
						.size = 64,
					},
					{
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
						.offset = 64,
						.size = 16,
					},
				};

				m_Pipelines.m_DebugPipelineLayout = renderer.CreatePipelineLayout(1, &m_Pipelines.m_CameraDescriptorSetLayout, 2, debugPushConstantRanges);

				if (m_Pipelines.m_DebugPipelineLayout == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create debug pipeline layout for world (function Renderer::CreatePipelineLayout in function World::initialize)");
				}

				Renderer::Shader pbrDrawVertexShader(renderer);
				Renderer::Shader pbrDrawFragmentShader(renderer);

				if (!pbrDrawVertexShader.Compile(pbr_draw_pipeline_vertex_shader, VK_SHADER_STAGE_VERTEX_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr draw vertex shader code (function Renderer::Shader::Compile in function World::Initialize)!");
				}

				if (!pbrDrawFragmentShader.Compile(pbr_draw_pipeline_fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr draw fragment shader code (function Renderer::Shader::Compile in function World::Initialize)!");
				}

				VkShaderModule pbrDrawShaderModules[2] {
					pbrDrawVertexShader.CreateShaderModule(),
					pbrDrawFragmentShader.CreateShaderModule(),
				};

				if (pbrDrawShaderModules[0] == VK_NULL_HANDLE || pbrDrawShaderModules[1] == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create shader modules for world pipeline (function Renderer::Shader::CreateShaderModule in function World::Initialize)!");
				}

				VkPipelineShaderStageCreateInfo pbrDrawPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaderModules[0], VK_SHADER_STAGE_VERTEX_BIT),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT),
				};

				Renderer::Shader pbrRenderVertexShader(renderer);
				Renderer::Shader pbrRenderFragmentShader(renderer);

				if (!pbrRenderVertexShader.Compile(pbr_render_pipeline_vertex_shader, VK_SHADER_STAGE_VERTEX_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr render vertex shader code (function Renderer::Shader::Compile in function World::Initialize)!");
				}

				if (!pbrRenderFragmentShader.Compile(pbr_render_pipeline_fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile pbr render fragment shader code (function Renderer::Shader::Compile in function World::Initialize)");
				}

				VkShaderModule pbrRenderShaderModules[2] {
					pbrRenderVertexShader.CreateShaderModule(),
					pbrRenderFragmentShader.CreateShaderModule(),
				};

				if (pbrRenderShaderModules[0] == VK_NULL_HANDLE || pbrRenderShaderModules[1] == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create shader modules for world pipeline (function Renderer::Shader::CreateShaderModule in function World::Initialize)!");
				}

				VkPipelineShaderStageCreateInfo pbrRenderPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaderModules[0], VK_SHADER_STAGE_VERTEX_BIT),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT),
				};

				Renderer::Shader debugPipelineVertexShader(renderer);
				Renderer::Shader debugPipelineFragmentShader(renderer);

				if (!debugPipelineVertexShader.Compile(debug_pipeline_vertex_shader, VK_SHADER_STAGE_VERTEX_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile vertex shader code (function Renderer::Shader::Compile in function World::Initialize)!");
				}

				if (!debugPipelineFragmentShader.Compile(debug_pipeline_fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to compile fragment shader code (function Renderer::Shader::Compile in function World::Initialize)!");
				}

				VkShaderModule debugPipelineShaderModules[2] {
					debugPipelineVertexShader.CreateShaderModule(),
					debugPipelineFragmentShader.CreateShaderModule(),
				};

				if (debugPipelineShaderModules[0] == VK_NULL_HANDLE || debugPipelineShaderModules[1] == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create shader modules for world pipeline (function Renderer::Shader::CreateShaderModule in function World::Initialize)!");
				}

				VkPipelineShaderStageCreateInfo debugPipelineShaderStageInfos[2] {
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugPipelineShaderModules[0], VK_SHADER_STAGE_VERTEX_BIT),
					Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugPipelineShaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT),
				};

				VkFormat pbrDrawRenderingColorFormats[3] {
					m_ColorImageResourcesFormat,
					m_ColorImageResourcesFormat,
					m_ColorImageResourcesFormat,
				};

				VkPipelineRenderingCreateInfo pbrDrawPipelineRenderingInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(3, pbrDrawRenderingColorFormats, m_Engine.m_Renderer.m_DepthOnlyFormat);

				VkPipelineRenderingCreateInfo pbrRenderPipelineRenderingInfo
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, VK_FORMAT_UNDEFINED);

				VkPipelineRenderingCreateInfo debugPipelineRenderingInfo 
					= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &m_Engine.m_Renderer.m_SwapchainSurfaceFormat.format, m_Engine.m_Renderer.m_DepthOnlyFormat);

				VkPipelineColorBlendStateCreateInfo pbrDrawPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
				pbrDrawPipelineColorBlendState.attachmentCount = 3;
				VkPipelineColorBlendAttachmentState pbrDrawPipelineColorAttachmentStates[3] {
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

				VkGraphicsPipelineCreateInfo graphicsPipelineInfos[3] = { 
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
						.layout = m_Pipelines.m_DrawPipelineLayoutPBR,
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
						.layout = m_Pipelines.m_RenderPipelineLayoutPBR,
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
						.layout = m_Pipelines.m_DebugPipelineLayout,
						.renderPass = VK_NULL_HANDLE,
						.subpass = 0,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					},
				};

				VkPipeline pipelines[3];

				if (!renderer.CreateGraphicsPipelines(3, graphicsPipelineInfos, pipelines)) {
					CriticalError(ErrorOrigin::Renderer, "failed to create world graphics pipeline (function Renderer::CreateGraphicsPipelines in function World::Initialize)!");
				}

				m_Pipelines.m_DrawPipelinePBR = pipelines[0];
				m_Pipelines.m_RenderPipelinePBR = pipelines[1];
				m_Pipelines.m_DebugPipeline = pipelines[2];

				renderer.DestroyShaderModule(pbrDrawShaderModules[0]);
				renderer.DestroyShaderModule(pbrDrawShaderModules[1]);
				renderer.DestroyShaderModule(pbrRenderShaderModules[0]);
				renderer.DestroyShaderModule(pbrRenderShaderModules[1]);
				renderer.DestroyShaderModule(debugPipelineShaderModules[0]);
				renderer.DestroyShaderModule(debugPipelineShaderModules[1]);

				if (!m_CameraMatricesBuffer.Create(sizeof(CameraMatricesBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create camera matrices buffer (function Renderer::Buffer::Create in function World::Initialize)!");
				}

				VkResult vkRes = vkMapMemory(renderer.m_VulkanDevice, m_CameraMatricesBuffer.m_VulkanDeviceMemory, 0, 
					sizeof(CameraMatricesBuffer), 0, (void**)&m_CameraMatricesMap);
				if (vkRes != VK_SUCCESS) {
					CriticalError(ErrorOrigin::Vulkan, 
						"failed to map camera matrices buffer (function vkMapMemory in function World::Initialize)!");
				}

				m_CameraMatricesMap->m_Projection = Mat4::Projection(pi / 4, 1.0f, 0.1f, 100.0f);
				m_CameraMatricesMap->m_View = Mat4::LookAt({ 0.0f, 0.0f, 0.0f }, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 3.0f));

				VkDescriptorPoolSize camPoolSize {
					.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
				};

				m_CameraMatricesDescriptorPool = renderer.CreateDescriptorPool(0, 1, 1, &camPoolSize);

				if (m_CameraMatricesDescriptorPool == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create camera matrices descriptor pool (function Renderer::CreateDescriptorPool in function World::Initialize)");
				}

				if (!renderer.AllocateDescriptorSets(nullptr, m_CameraMatricesDescriptorPool, 1, 
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

				renderer.UpdateDescriptorSets(1, &cameraDescriptorSetWrite);
			}

			void Terminate() {
				m_CameraMatricesBuffer.Terminate();
				Renderer& renderer = m_Engine.m_Renderer;
				renderer.DestroyDescriptorSetLayout(m_Pipelines.m_CameraDescriptorSetLayout);
				renderer.DestroyDescriptorPool(m_CameraMatricesDescriptorPool);
				renderer.DestroyDescriptorSetLayout(m_Pipelines.m_RenderPBRImagesDescriptorSetLayout);
				renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
				for (VkSampler sampler : m_RenderPBRImagesSamplers) {
					renderer.DestroySampler(sampler);
				}
				renderer.DestroyPipeline(m_Pipelines.m_DrawPipelinePBR);
				renderer.DestroyPipelineLayout(m_Pipelines.m_DrawPipelineLayoutPBR);
				renderer.DestroyPipeline(m_Pipelines.m_RenderPipelinePBR);
				renderer.DestroyPipelineLayout(m_Pipelines.m_RenderPipelineLayoutPBR);
				renderer.DestroyPipeline(m_Pipelines.m_DebugPipeline);
				renderer.DestroyPipelineLayout(m_Pipelines.m_DebugPipelineLayout);
				DestroyImageResources();
			}

			void DestroyImageResources() {
				Renderer& renderer = m_Engine.m_Renderer;
				for (size_t i = 0; i < m_DepthImages.m_Size; i++) {
					renderer.DestroyImageView(m_DepthImageViews[i]);
					renderer.DestroyImageView(m_DiffuseImageViews[i]);
					renderer.DestroyImageView(m_PositionAndMetallicImageViews[i]);
					renderer.DestroyImageView(m_NormalAndRougnessImageViews[i]);
					renderer.DestroyImage(m_DepthImages[i]);
					renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
					renderer.DestroyImage(m_DiffuseImages[i]);
					renderer.FreeVulkanDeviceMemory(m_DiffuseImagesMemory[i]);
					renderer.DestroyImage(m_PositionAndMetallicImages[i]);
					renderer.FreeVulkanDeviceMemory(m_PositionAndMetallicImagesMemory[i]);
					renderer.DestroyImage(m_NormalAndRougnessImages[i]);
					renderer.FreeVulkanDeviceMemory(m_NormalAndRougnessImagesMemory[i]);
				}
			}

		public:

			Vec2_T<bool> IsOnChunkBorder(const Vec3& position) {
				Vec2 dimensionsHalf = m_WorldRect.Dimensions() / 2;
				Vec2 pos = Vec2(position.x, position.z) + dimensionsHalf;
				Vec2 frac(pos.x / m_ChunkDimensions.x, pos.y / m_ChunkDimensions.y);
				IntVec2 intgr = frac;
				return { frac.x == (float)intgr.x, frac.y == (float)intgr.y };
			}

			Creature& AddCreature(const Vec3& position, const Collider::CreateInfo& colliderInfo) {
				if (!m_ChunkMatrix.m_Size) {
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

			Reference<RenderData> AddRenderData(const Creature& creature, const Mat4& transform, const MeshData& meshData) {
				return m_RenderDatas.EmplaceBack(creature.m_ObjectID, transform, meshData);
			}

			Reference<RenderData> AddRenderData(const Ground& ground, const Mat4& transform, const MeshData& meshData) {
				return m_RenderDatas.EmplaceBack(ground.m_ObjectID, transform, meshData);
			}
				
			Reference<RenderData> AddRenderData(const Obstacle& obstacle, const Mat4& transform, const MeshData& meshData) {
				return m_RenderDatas.EmplaceBack(obstacle.m_ObjectID, transform, meshData);
			}

			Reference<DebugRenderData> AddDebugRenderData(const Obstacle& obstacle, const Mat4& transform, const Vec4& wireColor, const MeshData& meshData) {
				return m_DebugRenderDatas.EmplaceBack(obstacle.m_ObjectID, transform, wireColor, meshData);
			}

			const DynamicArray<Ground>& GetGrounds() {
				return m_Grounds;
			}
				
			const DynamicArray<Obstacle>& GetObstacles() {
				return m_Obstacles;
			}

			Engine& GetEngine() const {
				return m_Engine;
			}

			bool RemoveCreature(Creature& creature) {
				RemoveRenderDatas(creature.m_ObjectID);
				if (m_CameraFollowObjectID == creature.m_ObjectID) {
						m_CameraFollowObjectID = UINT64_MAX;
				}
				return m_Creatures.Erase(&creature);
			}

		private:

			void SwapchainCreateCallback(VkExtent2D swapchainExtent, uint32_t imageCount) {
				Renderer& renderer = m_Engine.m_Renderer;
				if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
					VkFormat colorImageResourcesFormatCandidates[2] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_B8G8R8A8_SRGB };
					m_ColorImageResourcesFormat = renderer.FindSupportedFormat(1, colorImageResourcesFormatCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
					if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
						CriticalError(ErrorOrigin::Renderer, 
							"couldn't find suitable format for color image resources (function Renderer::FindSupportedFormat in function World::SwapchainCreateCallback)!");
					}
				}
				if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {

					VkDescriptorSetLayoutBinding imageSamplerDescriptorSetBinding {
						.binding = 0,
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = 1,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
						.pImmutableSamplers = nullptr,
					};

					VkDescriptorSetLayoutBinding pbrRenderPipelineDescriptorSetBindings[3] {
						imageSamplerDescriptorSetBinding,
						imageSamplerDescriptorSetBinding,
						imageSamplerDescriptorSetBinding,
					};

					pbrRenderPipelineDescriptorSetBindings[1].binding = 1;
					pbrRenderPipelineDescriptorSetBindings[2].binding = 2;

					m_Pipelines.m_RenderPBRImagesDescriptorSetLayout = renderer.CreateDescriptorSetLayout(nullptr, 3, pbrRenderPipelineDescriptorSetBindings);

					if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create pbr render pipeline samplers descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::SwapchainCreateCallback)!");
					}
				}

				if (m_RenderPBRImagesSamplers.m_Size != imageCount) {
					for (VkSampler sampler : m_RenderPBRImagesSamplers) {
						renderer.DestroySampler(sampler);
					}
					m_RenderPBRImagesSamplers.Resize(imageCount);
					for (VkSampler& sampler : m_RenderPBRImagesSamplers) {
						sampler = renderer.CreateSampler(renderer.GetDefaultSamplerInfo());
						if (sampler == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create pbr rendering pipeline image sampler for world (function Renderer::CreateSampler in function World::SwapchainCreateCallback)!");
						}
					}
				}

				renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
				VkDescriptorPoolSize poolSize {
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
				};
				VkDescriptorPoolSize poolSizes[3] {
					poolSize,
					poolSize,
					poolSize,
				};
				m_RenderPBRImagesDescriptorPool = renderer.CreateDescriptorPool(0, imageCount, 3, poolSizes);

				m_RenderPBRImagesDescriptorSets.Resize(imageCount);

				DynamicArray<VkDescriptorSetLayout> setLayouts(imageCount);

				for (VkDescriptorSetLayout& set : setLayouts) {
					set = m_Pipelines.m_RenderPBRImagesDescriptorSetLayout;
				}

				if (!renderer.AllocateDescriptorSets(nullptr, m_RenderPBRImagesDescriptorPool, imageCount, setLayouts.m_Data, m_RenderPBRImagesDescriptorSets.m_Data)) {
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
				VkFormat depthFormat = renderer.m_DepthOnlyFormat;
				VkExtent3D imageExtent {
					.width = swapchainExtent.width,
					.height = swapchainExtent.height,
					.depth = 1,
				};
				uint32_t colorImageQueueFamilies[2] { renderer.m_GraphicsQueueFamilyIndex, renderer.m_ComputeQueueFamilyIndex };
				uint32_t colorImageQueueFamilyCount = 2;
				VkSharingMode colorImageSharingMode = VK_SHARING_MODE_CONCURRENT;
				if (colorImageQueueFamilies[0] == colorImageQueueFamilies[1]) {
					colorImageQueueFamilyCount = 1;
					colorImageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				}
				LockGuard graphicsQueueLockGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
				Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
					= renderer.m_EarlyGraphicsCommandBufferQueue.New();
				if (!commandBuffer) {
					CriticalError(ErrorOrigin::Renderer,
						"renderer graphics command buffer was out of memory (in function World::SwapchainCreateCallback)!");
				}
				if (!renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
						&commandBuffer->m_CommandBuffer)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function Wrold::SwapchainCreateCallback)");
				}
				if (!renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to begin command buffer (function Renderer::BeginCommandBuffer in function World::SwapchainCreateCallback)");
				}
				for (uint32_t i = 0; i < imageCount; i++) {
					{
						VkImage& image = m_DiffuseImages[i];
						VkDeviceMemory& imageMemory = m_DiffuseImagesMemory[i];
						VkImageView& imageView = m_DiffuseImageViews[i];
						image = renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
						if (image == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world diffuse image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
						}
						imageMemory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
						if (imageMemory == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to allocate world diffuse image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
						}
						imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
						if (imageView == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world diffuse image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
						}
					}
					{
						VkImage& image = m_PositionAndMetallicImages[i];
						VkDeviceMemory& imageMemory = m_PositionAndMetallicImagesMemory[i];
						VkImageView& imageView = m_PositionAndMetallicImageViews[i];
						image = renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
						if (image == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create position/metallic image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
						}
						imageMemory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
						if (imageMemory == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to allocate position/metallic image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)");
						}
						imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
						if (imageView == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world position/metallic image view (function Renderer::CreateImageView in function World::Initialize)");
						}
					}
					{
						VkImage& image = m_NormalAndRougnessImages[i];
						VkDeviceMemory& imageMemory = m_NormalAndRougnessImagesMemory[i];
						VkImageView& imageView = m_NormalAndRougnessImageViews[i];
						image = renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
						if (image == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create normal/roughness image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
						}
						imageMemory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
						if (imageMemory == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to allocate normal/roughness image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
						}
						imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
						if (imageView == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world normal/roughness image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
						}
					}
					{
						VkImage& image = m_DepthImages[i];
						VkDeviceMemory& imageMemory = m_DepthImagesMemory[i];
						VkImageView& imageView = m_DepthImageViews[i];
						image = renderer.CreateImage(VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1, 
							VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
								VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &renderer.m_GraphicsQueueFamilyIndex);
						if (image == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world depth image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
						}
						imageMemory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
						if (imageMemory == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to allocate world depth image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)!");
						}
						imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
						if (imageView == VK_NULL_HANDLE) {
							CriticalError(ErrorOrigin::Renderer, 
								"failed to create world depth image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)!");
						}
					}

					VkDescriptorImageInfo descriptorImageInfos[3] {
						{
							.sampler = m_RenderPBRImagesSamplers[i],
							.imageView = m_DiffuseImageViews[i],
							.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						},
						{
							.sampler = m_RenderPBRImagesSamplers[i],
							.imageView = m_PositionAndMetallicImageViews[i],
							.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						},
						{
							.sampler = m_RenderPBRImagesSamplers[i],
							.imageView = m_NormalAndRougnessImageViews[i],
							.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						},
					};
					VkWriteDescriptorSet descriptorWrites[3] {};
					for (uint32_t j = 0; j < 3; j++) {
						descriptorWrites[j] 
							= Renderer::GetDescriptorWrite(nullptr, j, m_RenderPBRImagesDescriptorSets[i], 
								VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[j], nullptr);
					}
					renderer.UpdateDescriptorSets(3, descriptorWrites);

					VkImage colorImages[3] {
						m_DiffuseImages[i],
						m_PositionAndMetallicImages[i],
						m_NormalAndRougnessImages[i],
					};

					VkImageMemoryBarrier memoryBarriers[3];

					for (size_t i = 0; i < 3; i++) {
						memoryBarriers[i] = {
							.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
							.pNext = nullptr,
							.srcAccessMask = 0,
							.dstAccessMask = 0,
							.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = colorImages[i],
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
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 3, memoryBarriers);
				}
				VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
				if (vkRes != VK_SUCCESS) {
					CriticalError(ErrorOrigin::Vulkan, 
						"failed to end command buffer (function vkEndCommandBuffer in function World::SwapchainCreateCallback)!", 
						vkRes);
				}
				commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;
			}

			void Load(Vec2_T<uint32_t> worldDimensions, Vec2_T<uint32_t> chunkMatrixSize, 
						uint32_t groundCount, GroundInfo groundInfos[], uint32_t obstacleCount, ObstacleInfo obstacleInfos[]) {
				m_Grounds.Reserve(groundCount);
				for (uint32_t i = 0; i < groundCount; i++) {
					m_Grounds.EmplaceBack(groundInfos[i], m_NextObjectID++);
				}
				m_Obstacles.Reserve(groundCount);
				for (uint32_t i = 0; i < obstacleCount; i++) {
					m_Obstacles.EmplaceBack(obstacleInfos[i], m_NextObjectID++);
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

			void Unload() {
				m_Grounds.Clear();
				m_ChunkMatrix.Clear();
				m_Creatures.Clear();
				m_RenderDatas.Clear();
			};

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
							creature.m_CameraFollowCallback(creature, m_CameraMatricesMap->m_View);
						}
					}
				}
			}

			void RenderWorld(const Renderer::DrawData& drawData) const {
				{
					VkRenderingAttachmentInfo colorAttachments[3] {
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
						.renderArea { .offset {}, .extent { m_Engine.m_Renderer.m_SwapchainExtent } },
						.layerCount = 1,
						.viewMask = 0,
						.colorAttachmentCount = 3,
						.pColorAttachments = colorAttachments,
						.pDepthAttachment = &depthAttachment,
					};

					vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
					vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipelinePBR);
					vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_DrawPipelineLayoutPBR, 
						0, 1, &m_CameraMatricesDescriptorSet, 0, nullptr);;
					for (const RenderData& data : m_RenderDatas) {
						Mat4 matrices[2] {
							data.m_Transform,
							Transpose(Invert(data.m_Transform)),
						};
						vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_DrawPipelineLayoutPBR,
							VK_SHADER_STAGE_VERTEX_BIT, 0, 128, &matrices);
						vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, data.m_MeshData.m_VertexBuffers, 
							data.m_MeshData.m_VertexBufferOffsets);
						vkCmdBindIndexBuffer(drawData.m_CommandBuffer, data.m_MeshData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
						vkCmdDrawIndexed(drawData.m_CommandBuffer, data.m_MeshData.m_IndexCount, 1, 0, 0, 0);
					}
					vkCmdEndRendering(drawData.m_CommandBuffer);
				}
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
						.renderArea = { .offset {}, .extent { m_Engine.m_Renderer.m_SwapchainExtent } },
						.layerCount = 1,
						.viewMask = 0,
						.colorAttachmentCount = 1,
						.pColorAttachments = &colorAttachment,
						.pDepthAttachment = nullptr,
					};

					VkImage images[3] {
						m_DiffuseImages[drawData.m_CurrentFrame],
						m_PositionAndMetallicImages[drawData.m_CurrentFrame],
						m_NormalAndRougnessImages[drawData.m_CurrentFrame],
					};

					VkImageMemoryBarrier memoryBarriers[3]{};

					for (size_t i = 0; i < 3; i++) {
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
						0, 0, nullptr, 0, nullptr, 3, memoryBarriers);

					vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
					vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_RenderPipelinePBR);
					vkCmdBindDescriptorSets(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.m_RenderPipelineLayoutPBR, 0, 1, 
						&m_RenderPBRImagesDescriptorSets[drawData.m_CurrentFrame], 0, nullptr);
					vkCmdPushConstants(drawData.m_CommandBuffer, m_Pipelines.m_RenderPipelineLayoutPBR, VK_SHADER_STAGE_VERTEX_BIT, 
						0, 64, &m_RenderPBRQuadTransform);
					vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, m_StaticQuadMeshDataPBR.m_VertexBufferCount, 
						m_StaticQuadMeshDataPBR.m_VertexBuffers, m_StaticQuadMeshDataPBR.m_VertexBufferOffsets);
					vkCmdBindIndexBuffer(drawData.m_CommandBuffer, m_StaticQuadMeshDataPBR.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(drawData.m_CommandBuffer, m_StaticQuadMeshDataPBR.m_IndexCount, 1, 0, 0, 0);
					vkCmdEndRendering(drawData.m_CommandBuffer);

					for (size_t i = 0; i < 3; i++) {
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
						0, 0, nullptr, 0, nullptr, 3, memoryBarriers);
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
				if (index >= m_ChunkMatrix.m_Size) {
					return nullptr;
				}
				return &m_ChunkMatrix[index];
			}

			Vec2_T<bool> BoundsCheck(Vec2_T<uint32_t> chunkMatrixCoords) {
				return { chunkMatrixCoords.x >= m_ChunkMatrixSize.x, chunkMatrixCoords.y >= m_ChunkMatrixSize.y };
			}
		};

		class Editor {

			World& m_World;
			ImGuiContext* m_ImGuiContext;
			VkDescriptorPool m_ImGuiDescrptorPool;

			Editor(World& world) : m_World(world) {}
		};

	private:

		inline static Engine* s_engine_instance = nullptr;

		static void CriticalError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Engine called a critical error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"Vulkan error code: {}\n", (int)vkErr);
			}
			s_engine_instance->~Engine();
			s_engine_instance = nullptr;
			fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
			glfwTerminate();
#ifdef DEBUG
			assert(false);
#endif
			exit(EXIT_FAILURE);
		}

		static void Assert(bool expression, ErrorOrigin origin, const char* err) {
			if (!expression) {
				CriticalError(origin, err);
			}
		}

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

		static void SwapchainCreateCallback(const Renderer& renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews) {
			assert(s_engine_instance);
			s_engine_instance->m_UI.SwapchainCreateCallback({ extent.width, extent.height });
			s_engine_instance->m_World.SwapchainCreateCallback(extent, imageCount);
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

		EngineMode m_Mode;

		UI m_UI;
		World m_World;
		Renderer m_Renderer;
		TextRenderer m_TextRenderer;

		DynamicArray<Ground> m_Grounds{};

		StaticMesh m_StaticQuadMesh;
		StaticMesh m_StaticQuadMesh2D;

	public:

		Engine(EngineMode mode, const char* appName, GLFWwindow* window, size_t maxUIWindows) :
				m_Mode(UpdateEngineInstance(this, mode)),
				m_UI(*this, maxUIWindows),
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, RendererCriticalErrorCallback, SwapchainCreateCallback),
				m_TextRenderer(m_Renderer, TextRendererCriticalErrorCallback),
				m_StaticQuadMesh(*this),
				m_StaticQuadMesh2D(*this),
				m_World(*this)
		{
			Input input(window);

			static constexpr uint32_t quad_vertex_count = 4;

			static constexpr Vertex quad_vertices[quad_vertex_count] {
				{
					.m_Position { 1.0f, -1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, -1.0f },
					.m_UV { 1.0f, 0.0f },
				},	
				{
					.m_Position { 1.0f, 1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, -1.0f },
					.m_UV { 1.0f, 1.0f },
				},
				{
					.m_Position { -1.0f, -1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, -1.0f },
					.m_UV { 0.0f, 0.0f },
				},
				{
					.m_Position { -1.0f, 1.0f, 0.0f },
					.m_Normal { 0.0f, 0.0f, -1.0f },
					.m_UV { 0.0f, 1.0f },
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

			m_StaticQuadMesh.CreateBuffers(quad_vertex_count, quad_vertices, quad_index_count, quad_indices);
			m_StaticQuadMesh2D.CreateBuffers(quad_vertex_count, quad_vertices_2D, quad_index_count, quad_indices);

			m_World.Initialize();
			m_UI.Initialize();
		}

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;

		~Engine() {
			m_World.Terminate();
			m_StaticQuadMesh.Terminate();
			m_StaticQuadMesh2D.Terminate();
			m_UI.Terminate();
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
		}

		Vec2_T<uint32_t> GetRenderResolution() {
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

		const StaticMesh& GetQuadMesh() const {
			return m_StaticQuadMesh;
		}

		World& LoadWorld(Vec2_T<uint32_t> worldDimensions, Vec2_T<uint32_t> chunkMatrixSize, 
				uint32_t groundCount, GroundInfo groundInfos[], uint32_t obstacleCount, ObstacleInfo obstacleInfos[]) {
			m_World.Unload();
			m_World.Load(worldDimensions, chunkMatrixSize, groundCount, groundInfos, obstacleCount, obstacleInfos);
			return m_World;
		}

		bool Loop() {

			Time::BeginFrame();

			glfwPollEvents();

			if (m_Mode & EngineMode_Play) {
				m_World.LogicUpdate();
			}

			if (m_Mode & EngineMode_Editor) {
			}

			Renderer::DrawData drawData;
			if (m_Renderer.BeginFrame(drawData)) {
				VkViewport viewport {
					.x = 0.0f,
					.y = 0.0f,
					.width = (float)m_Renderer.m_SwapchainExtent.width,
					.height = (float)m_Renderer.m_SwapchainExtent.height,
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				};
				VkRect2D scissor {
					.offset = { 0, 0 },
					.extent = { m_Renderer.m_SwapchainExtent.width, m_Renderer.m_SwapchainExtent.height },
				};
				vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
				m_World.Render(drawData);
				m_UI.Render(drawData);
				m_Renderer.EndFrame(0, nullptr);
			}

			Input::ResetInput();

			bool closing = glfwWindowShouldClose(m_Renderer.m_Window);

			Time::EndFrame();

			return !closing;
		}
	};

	typedef Engine::Time Time;
	typedef Engine::Input Input;
	typedef Engine::UI UI;
	typedef Engine::StaticMesh StaticMesh;
	typedef Engine::StaticTexture StaticTexture;
	typedef Engine::MeshData MeshData;
	typedef Engine::Collider Collider;
	typedef Engine::Creature Creature;
	typedef Engine::World World;
}
