#include "renderer.hpp"
#include "math.hpp"
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace engine {

	constexpr inline uint64_t StringHash(const char* str) noexcept {
		if (!str) {
			return 0;
		}
		uint64_t res = 37;
		for (char c = str[0]; c; c++) {
			res = (res * 54059) ^ ((uint64_t)c * 76963);
		}
		return res;
	}

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
				delete[] m_Data;
				m_Data = nullptr;
				m_Size = 0;
				m_Capacity = 0;
			}

			constexpr size_t Size() const noexcept {
				return m_Size;
			}

			DynamicArray& Reserve(size_t capacity) {
				if (capacity < m_Capacity) {
					return *this;
				}
				T* temp = new T[capacity];
				for (size_t i = 0; i < m_Size; i++) {
					new (temp[i]) T(std::move(m_Data[i]));
				}
				delete[] m_Data;
				m_Capacity = capacity;
				m_Data = temp;
				return *this;
			}

			DynamicArray& Clear() {
				delete[] m_Data;
				m_Data = nullptr;
				m_Size = 0;
				m_Capacity = 0;
				return *this;
			}

			Iterator PushBack(const T& value) {
				if (m_Size == m_Capacity) {
					Reserve(m_Capacity * 2);
				}
				new (m_Data[m_Size++]) T(value);
				return &m_Data[m_Size];
			}

			template<typename... Args>
			Iterator EmplaceBack(Args&&... args) {
				if (m_Size == m_Capacity) {
					Reserve(m_Capacity * 2);
				}
				new (m_Data[m_Size++]) T(std::forward(args)...);
				return &m_Data[m_Size];
			}

			constexpr Iterator begin() const {
				return m_Data;
			}

			constexpr ConstIterator end() const {
				return m_Data ? m_Data[m_Size] : nullptr;
			}
		};

		template<typename T, typename Hash = T, size_t BucketCapacity = 4>
		class Set {
		public:
	
			typedef T Bucket[BucketCapacity];	

			struct ConstIterator {
			private:

				const Set& m_Set;
				DynamicArray<size_t>::ConstIterator m_IndexIter;
				const size_t m_BucketIndex;

			public:

				constexpr ConstIterator(Set& set, DynamicArray<size_t>::ConstIterator indexIter, size_t bucketIndex) noexcept 
					: m_Set(set), m_IndexIter(indexIter), m_BucketIndex(bucketIndex) {}

				constexpr ConstIterator(const ConstIterator& other) noexcept 
					: m_Set(other.m_Set), m_IndexIter(other.m_IndexIter), m_BucketIndex(other.m_BucketIndex) {}

				T& operator*() const {
					assert(m_IndexIter != nullptr && m_BucketIndex < BucketCapacity 
						&& "error in Engine::Set::ConstIterator (attempting to dereference an invalid iterator!)");
					return m_Set.m_Buckets[*m_IndexIter][m_BucketIndex];
				}
			};

			struct Iterator {
			private:

				const Set& m_Set;
				DynamicArray<size_t>::Iterator m_IndexIter;
				size_t m_BucketIndex;

			public:

				constexpr Iterator(Set& set) 
					noexcept : m_Set(set), m_IndexIter(nullptr), m_BucketIndex(BucketCapacity) {
					if (m_Set.m_BucketIndices.Size()) {
						m_IndexIter = m_Set.m_BucketIndices.begin();
						m_BucketIndex = 0;
					}
				}

				constexpr Iterator(const Iterator& other) noexcept
					: m_Set(other.m_Set), m_IndexIter(other.m_IndexIter), m_BucketIndex(other.m_BucketIndex) {}

				Iterator& operator++() noexcept {
					if (m_Set.m_BucketSizes[*m_IndexIter] > m_BucketIndex + 1) {
						++m_BucketIndex;
						return *this;
					}
					++m_IndexIter;
					if (m_IndexIter != m_Set.m_BucketIndices.end()) {
						m_BucketIndex = 0;
						return *this;
					}
					m_IndexIter = nullptr;
					m_BucketIndex = BucketCapacity;
					return *this;
				}

				Iterator& operator--() noexcept {
					if (m_BucketIndex > 0) {
						--m_BucketIndex;
						return *this;
					}
					if (m_IndexIter == m_Set.m_BucketIndices.begin()) {
						m_IndexIter = nullptr;
						m_BucketIndex = BucketCapacity;
						return *this;
					}
					--m_IndexIter;
					m_BucketIndex = m_Set.m_BucketSizes[*m_IndexIter] - 1;
					return *this;
				}

				T& operator*() const {
					assert(m_IndexIter != nullptr && m_BucketIndex < BucketCapacity 
						&& "error in Engine::Set::Iterator (attempting to dereference an invalid iterator!)");
					return m_Set.m_Buckets[*m_IndexIter][m_BucketIndex];
				}

				bool operator==(const Iterator& other) const noexcept {
					return &m_Set == &other.m_Set && m_IndexIter == other.m_IndexIter && m_BucketIndex == other.m_BucketIndex
						|| (m_IndexIter == nullptr && other.m_IndexIter == nullptr);
				}

				bool operator==(const ConstIterator& other) const noexcept {
					return &m_Set == &other.m_Set && m_IndexIter == other.m_IndexIter && m_BucketIndex == other.m_BucketIndex
						|| (m_IndexIter == nullptr && other.m_IndexIter == nullptr);
				};
			};

		private:

			Bucket* m_Buckets;
			size_t* m_BucketSizes;
			DynamicArray<size_t> m_BucketIndices;
			size_t m_Capacity;

		public:

			Set() : m_Buckets(nullptr), m_BucketSizes(nullptr), m_BucketIndices(), m_Capacity(0) {}

			~Set() {
				delete[] m_Buckets;
				m_BucketSizes = 0;
				m_BucketIndices.Clear();
				m_Capacity = 0;
			}

			void Reserve(size_t capacity) {
				if (capacity <= m_Capacity) {
					return;
				}
				Bucket* tempBuckets = malloc(sizeof(Bucket) * capacity);
				Bucket* tempBucketSizes = new size_t[capacity];
			}	

			T* Insert(const T& value) {
				if (!m_Capacity) {
					Reserve(128);
				}
				uint64_t hash;
				if constexpr (IsSame<T, Hash>::value) {
					hash = value();
				}
				else {
					hash = Hash()(value);
				}
				size_t index = hash & m_Capacity;
				size_t& bucketSize = m_BucketSizes[index];
				Bucket& bucket = m_Buckets[index];
				if (bucketSize) {
					for (size_t i = 0; i < bucketSize; i++) {
						if (m_Buckets[i] == value) {
							return nullptr;
						}
					}
					printf("bad hash!");
					if (bucketSize == BucketCapacity) {
						return nullptr;
					}
				}
				new(&bucket[bucketSize]) T(value);
				return bucket[bucketSize++];
			}

			template<typename... Args>
			T* Emplace(Args&&... args) {
				if (!m_Capacity) {
					Reserve(128);
				}
				T value(std::forward(args)...);
				uint64_t hash;
				if constexpr (IsSame<T, Hash>::value) {
					hash = value();
				}
				else {
					hash = Hash()(value);
				}
				size_t index = hash & m_Capacity;
				size_t& bucketSize = m_BucketSizes[index];
				Bucket& bucket = m_Buckets[index];
				if (bucketSize) {
					bool sameValueFound = false;
					for (size_t i = 0; i < bucketSize; i++) {
						if (m_Buckets[i] == value) {
							sameValueFound = true;
							return nullptr;
						}
					}
					printf("bad hash!");
					if (bucketSize == BucketCapacity) {
						return nullptr;
					}
				}
				new(&bucket[bucketSize]) T(std::move(value));
				return bucket[bucketSize++];
			}

			Iterator begin() const noexcept {
				return Iterator(*this);
			}

			ConstIterator end() const noexcept {
				return ConstIterator(*this, nullptr, BucketCapacity);
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
				return StringHash(m_Data);
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

		class GraphicsPipelineData1 {
		};

		class Entity {
		public:
			String m_Name;
			UID m_UID;
			virtual void Initialize(Engine* pEngine, const String& name, uint64_t UID) = 0;
			virtual void Initialize(Engine* pEngine, FILE* file) = 0;
			virtual void LogicUpdate(Engine* pEngine, bool& outTerminate) {};
			virtual void RenderUpdate(GraphicsPipelineData1* pPipeline, size_t desciptorCount, VkDescriptorSet* outDescriptorSets, 
				VkBuffer* outVertexBuffer, VkBuffer* outIndexBuffer) {};
			virtual void EditorUpdate(Engine* pEngine) = 0;
			virtual void WriteToFile(FILE* file) = 0;
			virtual void Terminate() = 0;

			uint64_t EntityPtrHash() {
				return m_Name() ^ m_UID;
			}
		};

		class EntityAllocator {

			template<typename EntityType>
			EntityType* Allocate() {
				return new EntityType();
			}

			template<typename EntityType>
			void Deallocate() {
				delete EntityType();
			}
		};

		class EntityConstructor {
			const char* m_TypeName;
			Entity* (*m_NewEntityFunction)(EntityAllocator*);
		};

		inline static Engine* s_engine_instance = nullptr;

		const bool m_Initialized;

		EntityAllocator m_EntityAllocator;
		const size_t m_EntityConstructorCount;
		EntityConstructor* const m_pEntityConstructors;	

		Set<Entity*> m_Entities;

		Renderer m_Renderer;

		CameraData1 m_GameCamera;
		CameraData1 m_DebugCamera;
		CameraData2 m_GameCameraData2;
		CameraData2 m_DebugCameraData2;

		static void RendererCriticalErrorCallback(Renderer* renderer, Renderer::ErrorOrigin origin, const char* err, VkFlags vkErr) {
			printf("Renderer called a critical error!\nError origin: %s\nError: %s\n", Renderer::ErrorOriginStr(origin), err);
			if (vkErr != VK_SUCCESS) {
				printf("Vulkan error code: %i\n", (int)vkErr); 
			}
			if (s_engine_instance != nullptr) {
				s_engine_instance->~Engine();
			}
			printf("Stopping program execution...\n");
			exit(EXIT_FAILURE);
		}

		static void RendererErrorCallback(Renderer* renderer, Renderer::ErrorOrigin origin, const char* err, VkFlags vkErr) {
			printf("Renderer called an error!\nError origin: %s\nError: %s\n", Renderer::ErrorOriginStr(origin), err);
			if (vkErr != VK_SUCCESS) {
				printf("Vulkan error code: %i\n", (int)vkErr); 
			}
		}

		static void SwapchainCreateCallback(Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews) {
		}

		static inline bool UpdateEngineInstance(Engine* engine) {
			if (s_engine_instance) {
				printf("attempting to initialize engine twice (only one engine allowed)!");
				exit(EXIT_FAILURE);
			}
			s_engine_instance = engine;
			return true;
		}

		Engine(const char* appName, GLFWwindow* window, size_t entityConstructorCount, EntityConstructor* pEntityConstructors)
			: m_Initialized(UpdateEngineInstance(this)), 
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, RendererCriticalErrorCallback, RendererErrorCallback, SwapchainCreateCallback),
				m_EntityConstructorCount(entityConstructorCount), m_pEntityConstructors(pEntityConstructors) {
		}

		~Engine() {
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
		}

		void DrawLoop() {
			Renderer::DrawData drawData;
			if (m_Renderer.BeginFrame(drawData)) {
				m_Renderer.EndFrame();
			}
		}
	};
}
