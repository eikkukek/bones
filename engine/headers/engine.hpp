#pragma once

#include "renderer.hpp"
#include "text_renderer.hpp"
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
			IndexOutOfBounds = 5,
			Vulkan = 6,
			Entity = 7,
			DynamicArray = 8,
			FileParsing = 9,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"Renderer",
				"TextRenderer",
				"UI",
				"OutOfMemory",
				"IndexOutOfBounds",
				"Vulkan",
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

			T& operator[](size_t index) {
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

		struct MeshData {
			uint32_t m_VertexBufferCount;
			const VkBuffer* m_VertexBuffers;
			const VkDeviceSize* m_VertexBufferOffsets;
			VkBuffer m_IndexBuffer;
			uint32_t m_IndexCount;
		};

		class StaticMesh {
		public:

			Engine& m_Engine;
			uint32_t m_IndexCount;
			Renderer::Buffer m_VertexBuffer;
			Renderer::Buffer m_IndexBuffer;

			StaticMesh(Engine& engine) noexcept 
				: m_Engine(engine), m_VertexBuffer(engine.m_Renderer), m_IndexBuffer(engine.m_Renderer), m_IndexCount(0) {}

			StaticMesh(StaticMesh&& other) noexcept : m_Engine(other.m_Engine), 
					m_VertexBuffer(std::move(other.m_VertexBuffer)), m_IndexBuffer(std::move(other.m_IndexBuffer)), m_IndexCount(0) {}

			StaticMesh(const StaticMesh&) = delete;

			MeshData GetMeshData() {
				constexpr static VkDeviceSize offset = 0;
				MeshData data {
					.m_VertexBufferCount = 1,
					.m_VertexBuffers = &m_VertexBuffer.m_Buffer,
					.m_VertexBufferOffsets = &offset,
					.m_IndexBuffer = m_IndexBuffer.m_Buffer,
					.m_IndexCount = m_IndexCount,
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
				LockGuard earltGraphicsCommandBufferQueueGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
				Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
					= renderer.m_EarlyGraphicsCommandBufferQueue.New();
				if (!commandBuffer) {
					PrintError(ErrorOrigin::OutOfMemory, 
						"renderer graphics command buffer queue was out of memory (in function StaticTexture::Create)!");
					Terminate();
					return false;
				}
				VkCommandBufferAllocateInfo commandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = renderer.GetCommandPool<Renderer::Queue::Graphics>(),
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				vkRes = vkAllocateCommandBuffers(renderer.m_VulkanDevice, &commandBufferAllocInfo, 
					&commandBuffer->m_CommandBuffer);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to allocate graphics command buffer (function vkAllocateCommandBuffers in function StaticTexture::Create)", 
						vkRes);
					Terminate();
					return false;
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

		struct GraphicsPipeline;

		class Entity {
		public:

			static constexpr size_t name_max_length = 63;

			const size_t c_ClassSize;
			char m_Name[name_max_length + 1];
			size_t m_NameLength;
			const UID m_UID;
			Engine& m_Engine;
			DynamicArray<GraphicsPipeline> m_GraphicsPipelines;

			Entity(Engine& engine, const char* name, UID UID, size_t classSize) noexcept 
				: m_Engine(engine), c_ClassSize(classSize), m_UID(UID) {
				size_t len = strlen(name);
				if (len > name_max_length) {
					PrintError(ErrorOrigin::Entity, 
						"given entity name is longer than entity name max size (in Entity constructor)!");
				}
				size_t i = 0;
				for (; i < len && i < name_max_length; i++) {
					m_Name[i] = name[i];
				}
				m_Name[i] = '\0';
				m_NameLength = i;
			}

			Entity(Engine& engine, FILE* file, UID UID, size_t classSize) noexcept : m_Engine(engine), c_ClassSize(classSize), m_UID(UID) {
				char c = fgetchar();
				size_t i = 0;
				for (; i < name_max_length && c != '\n'; i++) {
					m_Name[i] = c;
					c = fgetchar();
				}
				m_Name[i] = '\0';
				m_NameLength = i;
				if (c != '\n') {
					PrintError(ErrorOrigin::FileParsing, 
						"there was an entity name larger than entity name max size in file (in Entity constructor)!");
				}
				while (c != '\n') {
					c = fgetchar();
				}
			}

			virtual bool LogicUpdate() = 0;
			virtual void RenderUpdate(const GraphicsPipeline& pipeline, const CameraData1& camera, const uint32_t desciptorCount, 
				VkDescriptorSet** outDescriptorSets, uint32_t& meshCount, MeshData** meshes) = 0;
			virtual void EditorUpdate() {};
			virtual void WriteToFile(FILE* file) = 0;
			virtual void OnTerminate() {};

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

			VkPipeline m_Pipeline;
			uint32_t m_PushConstantCount;
			VkPipelineLayout m_PipelineLayout;
			uint32_t m_DescriptorSetCount;
			Set<Entity*, Entity::Hash> m_Entites;

			GraphicsPipeline() 
				: m_Pipeline(VK_NULL_HANDLE), m_PipelineLayout(VK_NULL_HANDLE),
					m_DescriptorSetCount(0), m_Entites() {}

			GraphicsPipeline(GraphicsPipeline&& other) noexcept 
				: m_Pipeline(other.m_Pipeline), m_PipelineLayout(other.m_PipelineLayout),
					m_DescriptorSetCount(other.m_DescriptorSetCount), m_Entites(std::move(other.m_Entites)) {}

			GraphicsPipeline(const GraphicsPipeline&) = delete;
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

				struct Vertex {
					Vec3 m_Position{};
					Vec2 m_UV{};
				};

				static constexpr uint32_t vertex_input_attributes_count = 2;

				static constexpr VkVertexInputAttributeDescription vertex_input_attributes[vertex_input_attributes_count] {
					{
						.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, m_Position),
					},
					{
						.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, m_UV),
					},
				};

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
	gl_Position = pc.c_Transform * vec4(inPosition, 1.0f);
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

				static constexpr uint32_t quad_vertex_count = 4;

				static constexpr Vertex quad_vertices[quad_vertex_count] {

					{
						.m_Position { -1.0f, 1.0f, 0.0f },
						.m_UV { 0.0f, 1.0f },
					},
					{
						.m_Position { 1.0f, 1.0f, 0.0f },
						.m_UV { 1.0f, 1.0f },
					},
					{
						.m_Position { -1.0f, -1.0f, 0.0f },
						.m_UV { 0.0f, 0.0f },
					},
					{
						.m_Position { 1.0f, -1.0f, 0.0f },
						.m_UV { 1.0f, 0.0f },
					},
				};

				static constexpr uint32_t quad_index_count = 6;

				static constexpr uint32_t quad_indices[quad_index_count] {
					3, 2, 0,
					1, 3, 0,
				};

				VkPipeline m_Pipeline;
				VkPipelineLayout m_PipelineLayout;
				VkDescriptorSetLayout m_DescriptorSetLayout;
			};

			struct Rect {

				IntVec2 m_Min{};
				IntVec2 m_Max{};

				bool IsPointInside(IntVec2 point) const {
					return point.x >= m_Min.x && point.y >= m_Min.y 
						&& point.x <= m_Max.x && point.y <= m_Max.y;
				}

				Vec2 CalcMiddle() const {
					return Vec2(m_Min.x + m_Max.x / 2.0f, m_Min.y + m_Max.y / 2.0f);
				}

				void CalcTransform(Vec2_T<uint32_t> renderResolution, Mat4& out) const {
					out.columns[0] = Vec4((float)(m_Max.x - m_Min.x) / renderResolution.x, 0.0f, 0.0f, 0.0f);
					out.columns[1] = Vec4(0.0f, (float)(m_Max.y - m_Min.y) / renderResolution.y, 0.0f, 0.0f);
					out.columns[2] = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
					Vec2 pos = CalcMiddle();
					out.columns[3] = Vec4(pos.x / renderResolution.x * 2.0f - 1.0f, pos.y / renderResolution.y * 2.0f - 1.0f, 0.0f, 1.0f);
				}
			};

			class Button {
			public:

				typedef bool (*Pipeline2DRenderCallback)(const Button& button, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);
				
				const char* const m_StringID;
				Rect m_Rect{};

				void (*m_HoverCallback)(Button& button){};

				Pipeline2DRenderCallback m_Pipeline2DRenderCallback{};
				Mat4 m_Transform{};

				IntVec2 GetPos() {
					return { m_Rect.m_Min.x, m_Rect.m_Max.y };
				}

				void SetPos(IntVec2 pos, Vec2_T<uint32_t> renderResolution) {
					IntVec2 oldPos = GetPos();
					IntVec2 deltaPos = pos - oldPos;
					IntVec2 size = m_Rect.m_Max - m_Rect.m_Min;
					m_Rect.m_Min = { pos.x, pos.y - size.y };
					m_Rect.m_Max = { pos.x + size.x, pos.y };
					m_Rect.CalcTransform(renderResolution, m_Transform);
				}

				bool RenderCallback2D(VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex) const {
					return m_Pipeline2DRenderCallback && m_Pipeline2DRenderCallback(*this, outTextureDescriptor, outTextureIndex);
				}

				uint64_t operator()() {
					return String::Hash()(m_StringID);
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

				typedef bool (*Pipeline2DRenderCallback)(const Window& button, VkDescriptorSet& outTextureDescriptor, uint32_t& outTextureIndex);

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

				IntVec2 GetPos() {
					return { m_Rect.m_Min.x, m_Rect.m_Max.y };
				}

				/*! @brief Sets the position of window.
				* @param[in] pos: New window position from top left of the window.
				*/
				void SetPosition(IntVec2 pos) {
					IntVec2 oldPos = GetPos();
					IntVec2 deltaPos = pos - oldPos;
					IntVec2 size = m_Rect.m_Max - m_Rect.m_Min;
					m_Rect.m_Min = { pos.x, pos.y };
					m_Rect.m_Max = pos + size;
					Vec2_T<uint32_t> renderResolution = m_UI.m_Engine.GetRenderResolution();
					m_Rect.CalcTransform(renderResolution, m_Transform);
					for (Button* button : m_Buttons) {
						assert(button);
						Rect& rect = button->m_Rect;
						IntVec2 buttonSize = rect.m_Max - rect.m_Min;
						IntVec2 buttonPos = button->GetPos() + deltaPos;
					}
				}

				uint64_t operator()() {
					return String::Hash()(m_StringID);
				}
			};

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
			StaticMesh m_StaticQuadMesh2D;

			UI(Engine& engine, size_t maxWindows) 
				: m_Engine(engine), m_Pipeline2D(), m_StaticQuadMesh2D(engine), m_WindowLookUp(maxWindows * 2) {
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
				if (!m_StaticQuadMesh2D.CreateBuffers(Pipeline2D::quad_vertex_count, Pipeline2D::quad_vertices, 
					Pipeline2D::quad_index_count, Pipeline2D::quad_indices)) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create static 2D quad mesh (function StaticMesh::CreateBuffers in UI constructor)!");
				}

				m_QuadMesh2DData = m_StaticQuadMesh2D.GetMeshData();

				Renderer& renderer = m_Engine.m_Renderer;

				{

					Renderer::Shader vertexShader(renderer);
					Renderer::Shader fragmentShader(renderer);

					if (!vertexShader.Compile(Pipeline2D::vertex_shader, VK_SHADER_STAGE_VERTEX_BIT)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to compile vertex shader for UI::Pipeline2D (function Renderer::Shader::Compile in UI::Pipeline2D constructor)!");
					}
					if (!fragmentShader.Compile(Pipeline2D::fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT)) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to compile vertex shader for UI::Pipeline2D (function Renderer::Shader::Compile in UI::Pipeline2D constructor)!");
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

					VkVertexInputBindingDescription vertexBinding {
						.binding = 0,
						.stride = sizeof(Vertex),
						.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
					};

					VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.vertexBindingDescriptionCount = 1,
						.pVertexBindingDescriptions = &vertexBinding,
						.vertexAttributeDescriptionCount = Pipeline2D::vertex_input_attributes_count,
						.pVertexAttributeDescriptions = Pipeline2D::vertex_input_attributes,
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
						.rasterizationSamples = renderer.GetMaxColorSamples(),
						.sampleShadingEnable = VK_TRUE,
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
							"failed to create descriptor set layout for 2D UI pipeline (function Renderer::CreateDescriptorSetLayout in UI constructor)!");
					}

					m_Pipeline2D.m_PipelineLayout = renderer.CreatePipelineLayout(1, &m_Pipeline2D.m_DescriptorSetLayout, 2, pushConstantRanges);

					if (m_Pipeline2D.m_PipelineLayout == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create pipeline layout for 2D UI pipeline (function Renderer::CreatePipelineLayout in UI constructor)!");
					}

					VkGraphicsPipelineCreateInfo pipelineInfo {
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.pNext = &renderingInfo,
						.flags = 0,
						.stageCount = 2,
						.pStages = shaderStages,
						.pVertexInputState = &vertexInputStateInfo,
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
							"failed to create UI pipelines (function Renderer::CreateGraphicsPipelines in UI constructor)!");
					}
					vkDestroyShaderModule(renderer.m_VulkanDevice, vertexShaderModule, renderer.m_VulkanAllocationCallbacks);
					vkDestroyShaderModule(renderer.m_VulkanDevice, fragmentShaderModule, renderer.m_VulkanAllocationCallbacks);
				}
			}

		public:

			UI(const UI&) = delete;
			UI(UI&&) = delete;

			Window* AddWindow(const char* stringID, WindowState state, IntVec2 pos, Vec2_T<uint32_t> size) {
				if (m_WindowLookUp.Contains(stringID)) {
					PrintError(ErrorOrigin::UI, 
						"attempting to add window with an already existing string ID (in function UI::AddWindow)!");
					return nullptr;
				}
				Window* res = m_WindowLookUp.Emplace(stringID, *this, stringID, state, 
					Rect { .m_Min { pos.x, pos.y }, .m_Max { pos.x + (int)size.x, pos.y + (int)size.y } });
				if (!res) {
					PrintError(ErrorOrigin::UI, "failed to add window (function UI::Dictionary::Emplace in function UI::AddWindow)!");
					return nullptr;
				}
				m_Windows.PushBack(res);
				return res;
			}

		private:

			void SwapchainCreateCallback(Vec2_T<uint32_t> renderResolution) {
				for (Window* window : m_Windows) {
					assert(window);
					window->RenderResolutionChange(renderResolution);
				}
			}

			void Terminate() {
				m_StaticQuadMesh2D.Terminate();
				m_Engine.m_Renderer.DestroyPipeline(m_Pipeline2D.m_Pipeline);
				m_Engine.m_Renderer.DestroyPipelineLayout(m_Pipeline2D.m_PipelineLayout);
				m_Engine.m_Renderer.DestroyDescriptorSetLayout(m_Pipeline2D.m_DescriptorSetLayout);
			}

		public:
		};

		class EntityConstructor {
			const char* m_TypeName;
			Entity* (*m_NewEntityFunction)(EntityAllocator&);
		};

		inline static Engine* s_engine_instance = nullptr;

		const uint32_t m_Initialized;

		UI m_UI;
		Renderer m_Renderer;
		TextRenderer m_TextRenderer;

		EntityAllocator m_EntityAllocator;
		const size_t m_EntityConstructorCount;
		EntityConstructor* const m_pEntityConstructors;	

		Set<Entity*, Entity::Hash> m_Entities{};
		DynamicArray<GraphicsPipeline> m_GraphicsPipelines{};

		CameraData1 m_GameCamera{};
		CameraData1 m_DebugCamera{};
		CameraData2 m_GameCameraData2{};
		CameraData2 m_DebugCameraData2{};

		StaticMesh m_StaticQuadMesh;

		static void CriticalError(ErrorOrigin origin, const char* err) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Engine called a critical error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			s_engine_instance->~Engine();
			s_engine_instance = nullptr;
			fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
			glfwTerminate();
#ifdef DEBUG
			assert(false);
#endif
			exit(EXIT_FAILURE);
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
			Vec2_T<uint32_t> renderResolution { extent.width, extent.height };
			s_engine_instance->m_UI.SwapchainCreateCallback(renderResolution);
		}

		static inline bool UpdateEngineInstance(Engine* engine) {
			if (s_engine_instance) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"attempting to initialize engine twice (only one engine allowed)!");
				exit(EXIT_FAILURE);
			}
			s_engine_instance = engine;
			return true;
		}	

		Engine(const char* appName, GLFWwindow* window, size_t maxUIWindows, size_t entityConstructorCount, 
			EntityConstructor* pEntityConstructors, size_t entityReservation)
			: m_Initialized(UpdateEngineInstance(this)), 
				m_UI(*this, maxUIWindows),
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, 
					false, RendererCriticalErrorCallback, SwapchainCreateCallback),
				m_TextRenderer(m_Renderer, TextRendererCriticalErrorCallback),
				m_EntityAllocator(), m_EntityConstructorCount(entityConstructorCount), m_pEntityConstructors(pEntityConstructors),
				m_StaticQuadMesh(*this) 
		{
			m_UI.Initialize();
			m_Entities.Reserve(entityReservation);
			static constexpr Vertex quadVertices[4] {
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
			static constexpr uint32_t quadIndices[6] {
				3, 2, 0,
				1, 3, 0,
			};
			m_StaticQuadMesh.CreateBuffers(4, quadVertices, 6, quadIndices);
		}

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;

		~Engine() {
			m_StaticQuadMesh.Terminate();
			m_UI.Terminate();
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
			for (Entity* entity : m_Entities) {
				entity->Terminate();
				m_EntityAllocator.Deallocate(entity);
			}
		}

		Vec2_T<uint32_t> GetRenderResolution() {
			return { m_Renderer.m_SwapchainExtent.width, m_Renderer.m_SwapchainExtent.height };
		}

		UI& GetUI() {
			return m_UI;
		}

		GraphicsPipeline& AddGraphicsPipeline(VkPipeline pipeline, VkPipelineLayout pipelineLayout, 
			uint32_t pushConstantCount, uint32_t descriptorSetCount, size_t entityReserve) {
			if (pipeline == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Uncategorized, "attempting to add graphics pipeline that's null (function AddGraphicsPipeline)!");
			}
			GraphicsPipeline& res = m_GraphicsPipelines.EmplaceBack();
			res.m_Pipeline = pipeline;
			res.m_PushConstantCount = pushConstantCount;
			res.m_PipelineLayout = pipelineLayout;
			res.m_DescriptorSetCount = descriptorSetCount;
			res.m_Entites.Reserve(entityReserve);
			return res;
		}

		void Render() {
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
				vkCmdSetViewport(drawData.commandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(drawData.commandBuffer, 0, 1, &scissor);
				VkRenderingAttachmentInfo colorAttachment{
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.pNext = nullptr,
					.imageView = drawData.swapchainImageView,
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.resolveMode = VK_RESOLVE_MODE_NONE,
					.resolveImageView = VK_NULL_HANDLE,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = { .color { .float32 { 0.0f, 0.0f, 0.0f, 1.0f } } },
				};
				VkRenderingInfo renderingInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.pNext = nullptr,
					.flags = 0,
					.renderArea = scissor,
					.layerCount = 1,
					.viewMask = 0,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachment,
					.pDepthAttachment = nullptr,
				};
				vkCmdBeginRendering(drawData.commandBuffer, &renderingInfo);
				CameraData1* cameras[2] = { &m_GameCamera, &m_DebugCamera };
				for (size_t i = 0; i < 1; i++) {
					for (GraphicsPipeline& pipeline : m_GraphicsPipelines) {
						vkCmdBindPipeline(drawData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_Pipeline);
						for (Entity* entity : pipeline.m_Entites) {
							VkDescriptorSet* pDescriptorSets = nullptr;
							uint32_t meshCount = 0;
							MeshData* meshes = nullptr;
							entity->RenderUpdate(pipeline, *cameras[i], pipeline.m_DescriptorSetCount, &pDescriptorSets, meshCount, &meshes);
							if (pDescriptorSets) {
								vkCmdBindDescriptorSets(drawData.commandBuffer, 
									VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_PipelineLayout, 0, pipeline.m_DescriptorSetCount, pDescriptorSets, 
									0, nullptr);
							}
							if (meshCount && !meshes) {
								PrintError(ErrorOrigin::Entity, 
									"Entity::RenderUpdate returned a non zero mesh count but meshes pointer was null (in function Render)!");
								continue;
							}
							for (uint32_t j = 0; j < meshCount; j++) {
								vkCmdBindVertexBuffers(drawData.commandBuffer, 
									0, meshes[j].m_VertexBufferCount, meshes[j].m_VertexBuffers, meshes[j].m_VertexBufferOffsets);
								vkCmdBindIndexBuffer(drawData.commandBuffer, meshes[j].m_IndexBuffer, 
									0, VK_INDEX_TYPE_UINT32);
								vkCmdDrawIndexed(drawData.commandBuffer, meshes[j].m_IndexCount, 1, 0, 0, 0);
							}
						}
					}
				}
				vkCmdEndRendering(drawData.commandBuffer);
				m_Renderer.EndFrame();
			}
		}
	};

	typedef Engine::UI UI;
	typedef Engine::StaticMesh StaticMesh;
	typedef Engine::StaticTexture StaticTexture;
	typedef Engine::Entity Entity;
	typedef Engine::GraphicsPipeline GraphicsPipeline;
	typedef Engine::CameraData1 CameraData;
	typedef Engine::MeshData MeshData;
}
