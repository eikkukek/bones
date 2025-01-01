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
			OutOfMemory = 3,
			Vulkan = 4,
			Entity = 5,
			DynamicArray = 6,
			FileParsing = 7,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"Renderer",
				"TextRenderer",
				"OutOfMemory",
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

		enum class MeshType {
			Static = 1,
			Dynamic = 2,
		};

		struct MeshData {
			uint32_t m_VertexBufferCount;
			const VkBuffer* m_VertexBuffers;
			const VkDeviceSize* m_VertexBufferOffsets;
			VkBuffer m_IndexBuffer;
			uint32_t m_IndexCount;
		};

		template<MeshType mesh_type_T>
		class Mesh {};

		template<>
		class Mesh<MeshType::Static> {
		public:

			Engine& m_Engine;
			uint32_t m_IndexCount;
			Renderer::Buffer m_VertexBuffer;
			Renderer::Buffer m_IndexBuffer;

			Mesh(Engine& engine) noexcept 
				: m_Engine(engine), m_VertexBuffer(engine.m_Renderer), m_IndexBuffer(engine.m_Renderer), m_IndexCount(0) {}

			Mesh(Mesh&& other) noexcept : m_Engine(other.m_Engine), 
					m_VertexBuffer(std::move(other.m_VertexBuffer)), m_IndexBuffer(std::move(other.m_IndexBuffer)), m_IndexCount(0) {}

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

			bool CreateBuffers(uint32_t vertexCount, const Vertex* pVertices, uint32_t indexCount, const uint32_t* pIndices) {
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
				m_IndexCount = indexCount;
				return true;
			}
		};

		enum class TextureType {
			Static = 1,
			Dynamic = 2,
		};

		template<TextureType texture_type_T>
		class Texture {};

		template<>
		class Texture<TextureType::Static> {
		public:

			Engine& m_Engine;
			VkFormat m_Format;
			VkImage m_Image;
			VkDeviceMemory m_VulkanDeviceMemory;

			Texture(Engine& engine) noexcept 
				: m_Engine(engine), m_Format(VK_FORMAT_UNDEFINED), m_Image(VK_NULL_HANDLE), m_VulkanDeviceMemory(VK_NULL_HANDLE) {}

			bool Create(VkFormat format, uint32_t colorChannels, Vec2_T<uint32_t> extent, uint8_t* image) {
				Renderer& renderer = m_Engine.m_Renderer;
				VkDeviceSize deviceSize = (VkDeviceSize)extent.x * extent.y * colorChannels;
				Renderer::Buffer stagingBuffer(m_Engine.m_Renderer);
				if (!stagingBuffer.Create(deviceSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					PrintError(ErrorOrigin::Renderer, 
						"failed to create staging buffer for texture (function Renderer::Buffer::Create in function Texture::Create!)");
					return false;
				}
				void* stagingMap;
				VkResult vkRes = vkMapMemory(renderer.m_VulkanDevice, 
					stagingBuffer.m_VulkanDeviceMemory, 0, deviceSize, 0, &stagingMap);
				memcpy(stagingMap, image, deviceSize);
				vkUnmapMemory(renderer.m_VulkanDevice, stagingBuffer.m_VulkanDeviceMemory);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to map staging buffer memory (function vkMapMemory in function Texture::Create)!", vkRes);
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
						"failed to create image (function vkCreateImage in function Texture::Create)!", vkRes);
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
						"failed to find memory type index (function Renderer::FindMemoryTypeIndex in function Texture::Create)!");
					Terminate();
					return false;
				}
				vkRes = vkAllocateMemory(renderer.m_VulkanDevice, &imageAllocInfo, 
					renderer.m_VulkanAllocationCallbacks, &m_VulkanDeviceMemory);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to allocate image memory (function vkAllocateMemory in function Texture::Create)!");
					Terminate();
					return false;
				}
				vkRes = vkBindImageMemory(renderer.m_VulkanDevice, m_Image, m_VulkanDeviceMemory, 0);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to bind image memory (function vkBindImageMemory in function Texture::Create)!");
					Terminate();
					return false;
				}
				LockGuard earltGraphicsCommandBufferQueueGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
				Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
					= renderer.m_EarlyGraphicsCommandBufferQueue.New();
				if (!commandBuffer) {
					PrintError(ErrorOrigin::OutOfMemory, 
						"renderer graphics command buffer queue was out of memory (in function Texture::Create)!");
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
						"failed to allocate graphics command buffer (function vkAllocateCommandBuffers in function Texture::Create)", 
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
						"failed to begin graphics command buffer (function vkAllocateCommandBuffers in function Texture::Create)", 
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
						"failed to end graphics command buffer (function vkEndCommandBuffer in function Texture::Create)!", 
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
						"attempting to create image view for texture that's null (in function Texture::CreateImageView)!");
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
						"failed to create image view (function vkCreateImageView in function Texture::CreateImageView)");
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
					PrintError(ErrorOrigin::Entity, "given entity name is longer than entity name max size (in Entity constructor)!");
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
					PrintError(ErrorOrigin::FileParsing, "there was an entity name larger than entity name max size in file (in Entity constructor)!");
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

		const uint32_t m_Initialized;

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

		Mesh<MeshType::Static> m_StaticQuadMesh;

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
			exit(EXIT_FAILURE);
		}

		static void SwapchainCreateCallback(const Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews) {
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

		Engine(const char* appName, GLFWwindow* window, bool includeImGui, size_t maxFontCount, size_t entityConstructorCount, 
			EntityConstructor* pEntityConstructors, size_t entityReservation)
			: m_Initialized(UpdateEngineInstance(this)), 
				m_Renderer(appName, VK_MAKE_API_VERSION(0, 1, 0, 0), window, 
					includeImGui, RendererCriticalErrorCallback, SwapchainCreateCallback),
				m_TextRenderer(m_Renderer, maxFontCount, TextRendererCriticalErrorCallback),
				m_EntityAllocator(), m_EntityConstructorCount(entityConstructorCount), m_pEntityConstructors(pEntityConstructors),
				m_StaticQuadMesh(*this) {
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

		~Engine() {
			m_StaticQuadMesh.Terminate();
			m_Renderer.Terminate();
			s_engine_instance = nullptr;
			for (Entity* entity : m_Entities) {
				entity->Terminate();
				m_EntityAllocator.Deallocate(entity);
			}
		}

		void CriticalError(ErrorOrigin origin, const char* err) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Engine called a critical error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			this->~Engine();
			fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
			glfwTerminate();
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
						vkCmdBindPipeline(drawData.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GpuPipeline);
						for (Entity* entity : pipeline.m_Entites) {
							VkDescriptorSet* pDescriptorSets = nullptr;
							uint32_t meshCount = 0;
							MeshData* meshes = nullptr;
							entity->RenderUpdate(pipeline, *cameras[i], pipeline.m_DescriptorSetCount, &pDescriptorSets, meshCount, &meshes);
							if (pDescriptorSets) {
								vkCmdBindDescriptorSets(drawData.commandBuffer, 
									VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_GpuPipelineLayout, 0, pipeline.m_DescriptorSetCount, pDescriptorSets, 
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

	typedef Engine::MeshType MeshType;
	typedef Engine::Mesh<MeshType::Static> StaticMesh;
	typedef Engine::TextureType TextureType;
	typedef Engine::Texture<TextureType::Static> StaticTexture;
	typedef Engine::Entity Entity;
	typedef Engine::GraphicsPipeline GraphicsPipeline;
	typedef Engine::CameraData1 CameraData;
	typedef Engine::MeshData MeshData;
}
