#pragma once

#include "renderer.hpp"
#include "math.hpp"
#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace engine {	

	class Engine {
	public:

		typedef uint64_t UID;

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
			Entity = 2,
			DynamicArray = 3,
			FileParsing = 4,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"Renderer",
				"Entity",
				"DynamicArray",
				"FileParsing",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		static void PrintError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS) {
			printf("Engine called an error!\nError origin: %s\nError: %s\n", ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				printf("Vulkan error code: %i\n", (int)vkErr);
			}
		}

		template<typename T>
		class DynamicArray {
		public:

			typedef T* Iterator;
			typedef T* const ConstIterator;

			T* m_Data;
			size_t m_Size;
			size_t m_Capacity;

			DynamicArray() : m_Data(nullptr), m_Size(0), m_Capacity(0) {}

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

			DynamicArray& Clear() {
				free(m_Data);
				m_Data = nullptr;
				m_Size = 0;
				m_Capacity = 0;
				return *this;
			}

			T& PushBack(const T& value) {
				if (m_Size == m_Capacity) {
					Reserve(m_Capacity ? m_Capacity * 2 : 2);
				}
				new(&m_Data[m_Size]) T(value);
				return m_Data[m_Size++];
			}

			template<typename... Args>
			T& EmplaceBack(Args&&... args) {
				if (m_Size == m_Capacity) {
					Reserve(m_Capacity ? m_Capacity * 2 : 2);
				}
				new(&m_Data[m_Size]) T(std::forward<Args>(args)...);
				return m_Data[m_Size++];
			}

			Iterator Erase(Iterator where) {
				ptrdiff_t diff = m_Data - where;
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

			Iterator begin() const {
				return m_Data;
			}

			ConstIterator end() const {
				return m_Data ? &m_Data[m_Size] : nullptr;
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
				DynamicArray<size_t>::Iterator m_IndexIter;
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
						printf("bad hash (in Engine::Set::Reserve)!");
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
					printf("bad hash (in function Engine::Set::Insert)!");
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
					printf("bad hash (in function Engine::Set::Emplace)!");
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
			static void GetVertexAttributes(uint32_t& outCount, const VkVertexInputAttributeDescription** outAttributes) {
				static constexpr uint32_t attribute_count = 5;
				static constexpr VkVertexInputAttributeDescription attributes[attribute_count] {
					{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Position) },
					{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Normal) },
					{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, m_UV) },
					{ .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Tangent) },
					{ .location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Bitangent) },
				};
				*outAttributes = attributes;
				outCount = attribute_count;
			}
			Vec3 m_Position;
			Vec3 m_Normal;
			Vec2 m_UV;
			Vec3 m_Tangent;
			Vec3 m_Bitangent;
		};

		enum class MeshType {
			Static = 0,
			Dynamic = 1,
		};

		template<MeshType mesh_type_T>
		class Mesh {};

		template<>
		class Mesh<MeshType::Static> {
		public:

			Engine& m_Engine;
			Renderer::Buffer m_VertexBuffer;
			Renderer::Buffer m_IndexBuffer;

			Mesh(Engine& engine) noexcept 
				: m_Engine(engine), m_VertexBuffer(engine.m_Renderer), m_IndexBuffer(engine.m_Renderer) {}

			Mesh(Mesh&& other) noexcept : m_Engine(other.m_Engine), 
					m_VertexBuffer(std::move(other.m_VertexBuffer)), m_IndexBuffer(std::move(other.m_IndexBuffer)) {}

			void Terminate() {
				m_VertexBuffer.Terminate();
				m_IndexBuffer.Terminate();
			}

			bool CreateBuffers(uint32_t vertexCount, Vertex* pVertices, uint32_t indexCount, uint32_t* pIndices) {
				if (m_VertexBuffer.m_BufferSize || m_IndexBuffer.m_BufferSize) {
					PrintError(ErrorOrigin::Renderer, 
					"attempting to create vertex and index buffers when the buffers have already been created (in function Mesh::CreateBuffers)!");
					return false;
				}
				if (!m_VertexBuffer.CreateWithData(vertexCount * sizeof(Vertex), pVertices, 
						VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
					PrintError(ErrorOrigin::Renderer, "failed to create vertex buffer (in function Mesh::CreateBuffers)!");
					return false;
				}
				if (!m_IndexBuffer.CreateWithData(indexCount * sizeof(uint32_t), pIndices, 
						VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
					PrintError(ErrorOrigin::Renderer, "failed to create index buffer (in function Mesh::CreateBuffers)!");
					m_VertexBuffer.Terminate();
					return false;
				}
				return true;
			}
		};

		struct CameraData1 {
			VkDescriptorSet m_DescriptorSet{};
			Mat4* m_ProjectionMatrixMap{};
			Mat4* m_ViewMatrixMap{};
			Vec2_T<uint32_t> viewArea{};
		};

		struct CameraData2 {
			VkBuffer m_ProjectionMatrixBuffer{};
			VkBuffer m_ViewMatrixBuffer{};
			VkDeviceMemory m_ProjectionMatrixDeviceMemory{};
			VkDeviceMemory m_ViewMatrixDeviceMemory{};
			VkDescriptorPool m_DescriptorPool{};
		};

		struct MeshBufferData {
			size_t vertexBufferCount;
			VkBuffer* vertexBuffers;
			VkDeviceSize* vertexBufferOffsets;
			VkBuffer indexBuffer;
		};

		struct GraphicsPipeline;

		class Entity {
		public:

			static constexpr size_t name_max_length = 63;

			const size_t c_ClassSize;
			char m_Name[name_max_length + 1];
			size_t m_NameLength;
			const UID m_UID;
			Engine* const m_pEngine;
			DynamicArray<GraphicsPipeline> m_GraphicsPipelines;

			Entity(Engine* pEngine, const char* name, UID UID, size_t classSize) noexcept 
				: m_pEngine(pEngine), c_ClassSize(classSize), m_UID(UID) {
				size_t len = strlen(name);
				if (len > name_max_length) {
					PrintError(ErrorOrigin::Entity, "given entity name is longer than entity name max size (in Entity constructor)!");
				}
				size_t i = 0;
				for (; i < len && i < name_max_length; i++) {
					m_Name[i] = name[i];
				}
				m_Name[i] = '\0';
				m_NameLength = i;
			}

			Entity(Engine* pEngine, FILE* file, UID UID, size_t classSize) noexcept : m_pEngine(pEngine), c_ClassSize(classSize), m_UID(UID) {
				char c = fgetchar();
				size_t i = 0;
				for (; i < name_max_length && c != '\n'; i++) {
					m_Name[i] = c;
					c = fgetchar();
				}
				m_Name[i] = '\0';
				m_NameLength = i;
				if (c != '\n') {
					PrintError(ErrorOrigin::FileParsing, "there was an entity name larger than entity name max size in file (in Entity constructor)!");
				}
				while (c != '\n') {
					c = fgetchar();
				}
			}

			virtual void LogicUpdate(Engine* pEngine, bool& outTerminate) {};
			virtual void RenderUpdate(const GraphicsPipeline& pipeline, const CameraData1& camera, const size_t desciptorCount, VkDescriptorSet** outDescriptorSets,
				size_t& meshCount, MeshBufferData** meshes) {};
			virtual void EditorUpdate(Engine* pEngine) = 0;
			virtual void WriteToFile(FILE* file) = 0;
			virtual void OnTerminate() = 0;

			void Terminate() {
				for (GraphicsPipeline& graphicsPipeline : m_GraphicsPipelines) {
					graphicsPipeline.m_Entites.Erase(this);
				}
				OnTerminate();
			}

			virtual ~Entity() {
			}

			struct Hash {
				uint64_t operator()(Entity* entity) {
					return String::Hash()(entity->m_Name) ^ entity->m_UID;
				}
			};
		};

		class EntityAllocator {
		public:

			template<typename EntityType>
			EntityType* Allocate(Engine* pEngine, const char* name, UID UID) {
				return new EntityType(pEngine, name, UID, sizeof(EntityType));
			}

			void Deallocate(Entity* entity) {
				delete entity;
			}
		};

		struct GraphicsPipeline {

			VkPipeline m_GpuPipeline;
			VkPipelineLayout m_GpuPipelineLayout;
			uint32_t m_DescriptorSetCount;
			Set<Entity*, Entity::Hash> m_Entites;

			GraphicsPipeline() 
				: m_GpuPipeline(VK_NULL_HANDLE), m_GpuPipelineLayout(VK_NULL_HANDLE),
					m_DescriptorSetCount(0), m_Entites() {}

			GraphicsPipeline(GraphicsPipeline&& other) noexcept 
				: m_GpuPipeline(other.m_GpuPipeline), m_GpuPipelineLayout(other.m_GpuPipelineLayout),
					m_DescriptorSetCount(other.m_DescriptorSetCount), m_Entites(std::move(other.m_Entites)) {}
		};

		class EntityConstructor {
			const char* m_TypeName;
			Entity* (*m_NewEntityFunction)(EntityAllocator&);
		};

		inline static Engine* s_engine_instance = nullptr;

		const bool m_Initialized;

		EntityAllocator m_EntityAllocator;
		const size_t m_EntityConstructorCount;
		EntityConstructor* const m_pEntityConstructors;	

		Set<Entity*, Entity::Hash> m_Entities{};
		DynamicArray<GraphicsPipeline> m_GraphicsPipelines{};

		Renderer m_Renderer;

		CameraData1 m_GameCamera{};
		CameraData1 m_DebugCamera{};
		CameraData2 m_GameCameraData2{};
		CameraData2 m_DebugCameraData2{};

		static void RendererCriticalErrorCallback(const Renderer* renderer, Renderer::ErrorOrigin origin, const char* err, VkFlags vkErr) {
			printf("Renderer called a critical error!\nError origin: %s\nError: %s\n", Renderer::ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				printf("Vulkan error code: %i\n", (int)vkErr); 
			}
			if (s_engine_instance != nullptr) {
				s_engine_instance->~Engine();
			}
			printf("Stopping program execution...\n");
			exit(EXIT_FAILURE);
		}

		static void RendererErrorCallback(const Renderer* renderer, Renderer::ErrorOrigin origin, const char* err, VkFlags vkErr) {
			printf("Renderer called an error!\nError origin: %s\nError: %s\n", Renderer::ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				printf("Vulkan error code: %i\n", (int)vkErr); 
			}
		}

		static void SwapchainCreateCallback(const Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews) {
		}

		static inline bool UpdateEngineInstance(Engine* engine) {
			if (s_engine_instance) {
				printf("attempting to initialize engine twice (only one engine allowed)!");
				exit(EXIT_FAILURE);
			}
			s_engine_instance = engine;
			return true;
		}	

		Engine(const char* appName, GLFWwindow* window, size_t entityConstructorCount, EntityConstructor* pEntityConstructors, size_t entityReservation)
			: m_Initialized(UpdateEngineInstance(this)), 
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, RendererCriticalErrorCallback, RendererErrorCallback, SwapchainCreateCallback),
				m_EntityAllocator(), m_EntityConstructorCount(entityConstructorCount), m_pEntityConstructors(pEntityConstructors) {
			m_Entities.Reserve(entityReservation);
		}

		~Engine() {
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
			for (Entity* entity : m_Entities) {
				entity->Terminate();
				m_EntityAllocator.Deallocate(entity);
			}
		}

		void CriticalError(ErrorOrigin origin, const char* err) {
			printf("Engine called a critical error!\nError origin: %s\nError: %s\n", ErrorOriginString(origin), err);
			this->~Engine();
			printf("Stopping program execution...\n");
			exit(EXIT_FAILURE);
		}

		GraphicsPipeline& AddGraphicsPipeline(VkPipeline pipeline, VkPipelineLayout pipelineLayout, 
			uint32_t descriptorSetCount, size_t entityReserve) {
			if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Uncategorized, "attempting to add graphics pipeline that's null (function AddGraphicsPipeline)!");
			}
			GraphicsPipeline& res = m_GraphicsPipelines.EmplaceBack();
			res.m_GpuPipeline = pipeline;
			res.m_GpuPipelineLayout = pipelineLayout;
			res.m_DescriptorSetCount = descriptorSetCount;
			res.m_Entites.Reserve(entityReserve);
			return res;
		}

		void DrawLoop() {
			Renderer::DrawData drawData;
			if (m_Renderer.BeginFrame(drawData)) {
				CameraData1* cameras[2] = { &m_GameCamera, &m_DebugCamera };
				for (size_t i = 0; i < 2; i++) {
					for (GraphicsPipeline& pipeline : m_GraphicsPipelines) {
						vkCmdBindPipeline(drawData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GpuPipeline);
						for (Entity* entity : pipeline.m_Entites) {
							VkDescriptorSet* pDescriptorSets = nullptr;
							VkBuffer* pVertexBuffer = nullptr;
							VkBuffer* pIndexBuffer = nullptr;
							size_t meshCount = 0;
							MeshBufferData* meshes = nullptr;
							entity->RenderUpdate(pipeline, *cameras[i], pipeline.m_DescriptorSetCount, &pDescriptorSets, meshCount, &meshes);
							if (pDescriptorSets) {
								vkCmdBindDescriptorSets(drawData.commandBuffer, 
									VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GpuPipelineLayout, 0, pipeline.m_DescriptorSetCount, pDescriptorSets, 
									0, nullptr);
								if (meshCount && !meshes) {
									PrintError(ErrorOrigin::Entity, 
										"Entity::RenderUpdate returned a non zero mesh count but meshes pointer was null (in function DrawLooLoopp)!");
									continue;
								}
								for (size_t i = 0; i < meshCount; i++) {
									vkCmdBindVertexBuffers(drawData.commandBuffer, 
										0, meshes[i].vertexBufferCount, meshes[i].vertexBuffers, meshes[i].vertexBufferOffsets);
									vkCmdBindIndexBuffer(drawData.commandBuffer, meshes[i].indexBuffer, 
										0, VK_INDEX_TYPE_UINT32);
								}
							}
						}
					}
				}
				m_Renderer.EndFrame();
			}
		}
	};
}
