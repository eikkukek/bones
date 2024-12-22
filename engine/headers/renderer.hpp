#pragma once

#include "math.hpp"
#include "vulkan/vulkan.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glslang_c_interface.h"
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <new>
#include <thread>
#include <mutex>
#include <utility>

namespace engine {

#ifndef BOOL_RENDERER_DEBUG 
#define BOOL_RENDERER_DEBUG true 
#endif

	class Renderer {
	public:

		typedef void (*SwapchainCreateCallback)(const Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews);
		typedef std::lock_guard<std::mutex> LockGuard;

		enum class ErrorOrigin {
			Uncategorized = 0,
			InitializationFailed = 1,
			Vulkan = 2,
			OutOfMemory = 3,
			NullDereference = 4,
			IndexOutOfBounds = 5,
			Shader = 6,
			Buffer = 7,
			Threading = 8,
			MaxEnum,
		};

		typedef void (*ErrorCallback)(const Renderer* renderer, ErrorOrigin origin, const char* err, VkFlags vkErr);

		enum class Queue {
			Graphics = 0,
			Transfer = 1,
			Present = 2,
		};

		struct Stack {

			template<typename T>
			struct Array {

				T* const m_Data;
				const size_t m_Size;

				Array(size_t size, T* data) : m_Data(data), m_Size(size) {}
	
				bool Contains(const T& value) {
					T* iter = m_Data;
					T* const end = m_Data ? &m_Data[m_Size] : nullptr;
					for (; iter != end; iter++) {
						if (*iter == value) {
							return true;
						}
					}
					return false;
				}

				bool Contains(const T& value, bool(*cmp)(const T& a, const T& b)) {
					T* iter = m_Data;
					T* const end = m_Data ? &m_Data[m_Size] : nullptr;
					for (; iter != end; iter++) {
						if (cmp(value, *iter)) {
							return true;
						}
					}
					return false;
				}

				T* begin() {
					return m_Data;
				}

				T* const end() {
					return m_Data ? &m_Data[m_Size] : nullptr;
				}

				T& operator[](size_t index) {
					if (index >= m_Size) {
						PrintError("index out of bounds (engine::Renderer::Stack::Array::operator[])!", ErrorOrigin::IndexOutOfBounds);
						assert(false);
					}
					return m_Data[index];
				}
			};

			uint8_t* const m_Data;
			const size_t m_MaxSize;
			size_t m_UsedSize;

			Stack(size_t maxSize, uint8_t* data)
				: m_Data(data), m_MaxSize(maxSize), m_UsedSize(0) {
				if (!m_Data) {
					PrintWarning("data was null (engine::Renderer::Stack constructor)!");
				}
			}

			template<typename T, typename... Args>
			T* AllocateSingle(Args&&... args) {
				T* res = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T)) > m_MaxSize) {
					PrintError("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::OutOfMemory);
					return nullptr;
				}
				new(res) T(std::forward<Args>(args)...);
				return res;
			}

			template<typename T>
			Array<T> Allocate(size_t count) {
				T* res = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintError("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::OutOfMemory);
					return Array<T>(0, nullptr);
				}
				T* iter = res;
				T* end = &res[count];
				for (; iter != end; iter++) {
					new(iter) T();
				}
				return Array<T>(count, res);
			}

			template<typename T>
			bool Allocate(size_t count, Array<T>* out) {
				T* res = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintError("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::OutOfMemory);
					return false;
				}
				T* iter = res;
				T* end = &res[count];
				for (; iter != end; iter++) {
					new(iter) T();
				}
				new(out) Array<T>(count, res);
				return true;
			}

			bool Allocate(size_t size, void** const out) {
				if (out == nullptr) {
					PrintError("pointer passed to stack was null (function Stack::Allocate)!", ErrorOrigin::NullDereference);
					return false;
				}
				*out = m_Data + m_UsedSize;
				if ((m_UsedSize += size) > m_MaxSize) {
					PrintError("stack out of memory (function Stack::Allocate)!", ErrorOrigin::OutOfMemory);
					return false;
				}
				return true;
			}

			template<typename T>
			void Deallocate(size_t count) {
				size_t size = sizeof(T) * count;
				m_UsedSize -= size < m_UsedSize ? size : m_UsedSize;
			}

			void Clear() {
				m_UsedSize = 0;
			}
		};

		class DescriptorPool {
		public:

			const Renderer& m_Renderer;
			const uint32_t m_MaxSets;
			VkDescriptorPool m_DescriptorPool;

			DescriptorPool(Renderer& renderer, uint32_t maxSets) noexcept : m_Renderer(renderer), m_MaxSets(maxSets), m_DescriptorPool(VK_NULL_HANDLE) {}

			~DescriptorPool() {
				Terminate();
			}

			void Terminate() {
				if (m_DescriptorPool != VK_NULL_HANDLE) {
					vkDestroyDescriptorPool(m_Renderer.m_GpuDevice, m_DescriptorPool, m_Renderer.m_GpuAllocationCallbacks);
					m_DescriptorPool = VK_NULL_HANDLE;
				}
			}

			bool CreatePool(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes) {
				if (m_DescriptorPool != VK_NULL_HANDLE) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Uncategorized, 
						"attempting to create descriptor pool (in function DescriptorPool::CreatePool) that has already been created!", VK_SUCCESS);
					return false;
				}
				VkDescriptorPoolCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.maxSets = m_MaxSets,
					.poolSizeCount = poolSizeCount,
					.pPoolSizes = pPoolSizes,
				};
				VkResult vkRes = vkCreateDescriptorPool(m_Renderer.m_GpuDevice, &createInfo, m_Renderer.m_GpuAllocationCallbacks, &m_DescriptorPool);
				if (vkRes != VK_SUCCESS) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Vulkan, "failed to create descriptor pool (function vkCreateDescriptorPool in function DescriptorPool::CreatePool)!", vkRes);
					m_DescriptorPool = VK_NULL_HANDLE;
					return false;
				}
				return true;
			}
		};

		struct Shader {

			static constexpr glslang_stage_t GetGlslangStage(VkShaderStageFlagBits shaderStage) noexcept {
				switch (shaderStage) {
				case VK_SHADER_STAGE_VERTEX_BIT:
					return GLSLANG_STAGE_VERTEX;
				case VK_SHADER_STAGE_FRAGMENT_BIT:
					return GLSLANG_STAGE_FRAGMENT;
				default:
					return GLSLANG_STAGE_ANYHIT;
				}
			}

			const Renderer& m_Renderer;
			glslang_shader_t* m_GlslangShader;
			glslang_program_t* m_GlslangProgram;

			Shader(Renderer& renderer) noexcept 
				: m_Renderer(renderer), m_GlslangShader(nullptr), m_GlslangProgram(nullptr) {}

			bool NotCompiled() const {
				return !m_GlslangShader || !m_GlslangProgram;
			}

			bool Compile(const char* shaderCode, VkShaderStageFlagBits shaderStage) {

				const glslang_resource_t resource {
					.max_draw_buffers = (int)m_Renderer.m_GpuMaxFragmentOutputAttachments,
				};

				const glslang_input_t input = {
					.language = GLSLANG_SOURCE_GLSL,
					.stage = GetGlslangStage(shaderStage),
					.client = GLSLANG_CLIENT_VULKAN,
					.client_version = GLSLANG_TARGET_VULKAN_1_3,
					.target_language = GLSLANG_TARGET_SPV,
					.target_language_version = GLSLANG_TARGET_SPV_1_6,
					.code = shaderCode,
					.default_version = 100,
					.default_profile = GLSLANG_NO_PROFILE,
					.force_default_version_and_profile = false,
					.forward_compatible = false,
					.messages = GLSLANG_MSG_DEFAULT_BIT,
					.resource = &resource,
				};

				if (!glslang_initialize_process()) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, "failed to initialize glslang process (in Shader constructor)!", VK_SUCCESS);
					return false;
				}

				m_GlslangShader = glslang_shader_create(&input);

				if (!glslang_shader_preprocess(m_GlslangShader, &input)) {
					const char* log = glslang_shader_get_info_log(m_GlslangShader);
					const char* dLog = glslang_shader_get_info_debug_log(m_GlslangShader);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, log, VK_SUCCESS);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, dLog, VK_SUCCESS);
					glslang_shader_delete(m_GlslangShader);
					m_GlslangShader = nullptr;
					return false;
				}

				if (!glslang_shader_parse(m_GlslangShader, &input)) {
					const char* log = glslang_shader_get_info_log(m_GlslangShader);
					const char* dLog = glslang_shader_get_info_debug_log(m_GlslangShader);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, log, VK_SUCCESS);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, dLog, VK_SUCCESS);
					glslang_shader_delete(m_GlslangShader);
					m_GlslangShader = nullptr;
					return false;
				}

				m_GlslangProgram = glslang_program_create();
				glslang_program_add_shader(m_GlslangProgram, m_GlslangShader);

				if (!glslang_program_link(m_GlslangProgram, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
					const char* log = glslang_program_get_info_log(m_GlslangProgram);
					const char* dLog = glslang_program_get_info_debug_log(m_GlslangProgram);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, log, VK_SUCCESS);
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, dLog, VK_SUCCESS);
					glslang_shader_delete(m_GlslangShader);
					glslang_program_delete(m_GlslangProgram);
					m_GlslangShader = nullptr;
					m_GlslangProgram = nullptr;
					return false;
				}

				glslang_program_SPIRV_generate(m_GlslangProgram, input.stage);

				if (glslang_program_SPIRV_get_messages(m_GlslangProgram)) {
					PrintMessage(glslang_program_SPIRV_get_messages(m_GlslangProgram));
				}
			}

			uint32_t GetCodeSize() const noexcept {
				if (NotCompiled()) {
					return 0;
				}
				return (uint32_t)glslang_program_SPIRV_get_size(m_GlslangProgram) * sizeof(uint32_t);
			}

			const unsigned int* GetBinary() const noexcept {
				if (NotCompiled()) {
					return nullptr;
				}
				return glslang_program_SPIRV_get_ptr(m_GlslangProgram);
			}

			VkShaderModule CreateShaderModule() {
				if (NotCompiled()) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Shader, 
						"attempting to create shader module with a shader (in function CreateShaderModule) with a shader that hasn't been compiled", VK_SUCCESS);
					return VK_NULL_HANDLE;
				}
				VkShaderModuleCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.codeSize = GetCodeSize(),
					.pCode = GetBinary(),
				};
				VkShaderModule res;	
				if (!m_Renderer.VkCheck(vkCreateShaderModule(m_Renderer.m_GpuDevice, &createInfo, m_Renderer.m_GpuAllocationCallbacks, &res), 
					"failed to create shader module (function vkCreateShaderModule in function Shader::CreateShaderModule)!")) {
					return VK_NULL_HANDLE;
				}
				return res;
			}

			~Shader() {
				if (m_GlslangShader) {
					glslang_shader_delete(m_GlslangShader);
					m_GlslangShader = nullptr;
				}
				if (m_GlslangProgram) {
					glslang_program_delete(m_GlslangProgram);
					m_GlslangProgram = nullptr;
				}
			}
		};

		struct CommandBufferFreeList {

			static constexpr size_t max_command_buffers_per_frame = 1000;

			const Renderer& m_Renderer;
			VkCommandPool m_GpuCommandPool;
			uint32_t m_FramesInFlight; 
			VkCommandBuffer(*m_Data)[max_command_buffers_per_frame];
			uint32_t* m_Counts;

			CommandBufferFreeList(Renderer& renderer)
				: m_Renderer(renderer), m_GpuCommandPool(VK_NULL_HANDLE), m_FramesInFlight(0), m_Data(nullptr), m_Counts(nullptr) {}

			~CommandBufferFreeList() {
				free(m_Data);
				delete[] m_Counts;
			}

			void Initialize(VkCommandPool commandPool) {
				if (m_FramesInFlight != 0) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::InitializationFailed, 
						"attempting to initialize command buffer free list more than once (function CommandBufferFreeList::Initialize)!",
						VK_SUCCESS);
					return;
				}
				m_GpuCommandPool = commandPool;
				m_FramesInFlight = m_Renderer.m_FramesInFlight;
				m_Data = (VkCommandBuffer(*)[max_command_buffers_per_frame])malloc(m_FramesInFlight 
					* sizeof(VkCommandBuffer[max_command_buffers_per_frame]));
				m_Counts = new uint32_t[m_FramesInFlight]{};
			}

			void Reallocate() {
				FreeAll();
				if (m_FramesInFlight != m_Renderer.m_FramesInFlight) {
					m_FramesInFlight = m_Renderer.m_FramesInFlight;
					free(m_Data);
					delete[] m_Counts;
					m_Data = (VkCommandBuffer(*)[max_command_buffers_per_frame])malloc(m_FramesInFlight 
						* sizeof(VkCommandBuffer[max_command_buffers_per_frame]));
					m_Counts = new uint32_t[m_FramesInFlight]{};
				}
			}

			void Push(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
				if (currentFrame >= m_FramesInFlight) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::IndexOutOfBounds, 
						"current frame goes out of bounds of command buffer free list frames in flight (function CommandBufferFreeList::Push)!", 
						VK_SUCCESS);
					return;
				}
				uint32_t& count = m_Counts[currentFrame];
				if (count >= max_command_buffers_per_frame) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::OutOfMemory, 
						"command buffer free list was out of memory (function CommandBufferFreeList::Push)!", VK_SUCCESS);
					return;
				}
				m_Data[currentFrame][count++] = commandBuffer;
			}

			void Free(uint32_t currentFrame) {
				if (currentFrame >= m_FramesInFlight) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::IndexOutOfBounds, 
						"given frame goes out of bounds of command buffer free list frames in flight (function CommandBufferFreeList::Free)!", 
						VK_SUCCESS);
					return;
				}
				uint32_t& count = m_Counts[currentFrame];
				if (count) {
					vkFreeCommandBuffers(m_Renderer.m_GpuDevice, m_GpuCommandPool, 
						count, m_Data[currentFrame]);
					count = 0;
				}
			}

			void FreeAll() {
				for (uint32_t i = 0; i < m_FramesInFlight; i++) {
					uint32_t& count = m_Counts[i];
					if (count) {
						vkFreeCommandBuffers(m_Renderer.m_GpuDevice, m_GpuCommandPool, 
							count, m_Data[i]);
						count = 0;
					}
				}
			}
		};

		struct Thread {

			static constexpr size_t max_free_command_buffer_count = 1000;

			Renderer& m_Renderer;
			std::thread m_Thread;
			const std::thread::id m_ID;
			VkCommandPool m_GpuGraphicsCommandPool;
			VkCommandPool m_GpuTransferCommandPool;
			CommandBufferFreeList m_GraphicsCommandBufferFreeList;
			CommandBufferFreeList m_TransferCommandBufferFreeList;

			Thread(Renderer& renderer, std::thread&& thread) noexcept 
				: m_Renderer(renderer), m_Thread(std::move(thread)), m_ID(thread.get_id()), 
					m_GpuGraphicsCommandPool(VK_NULL_HANDLE), m_GpuTransferCommandPool(VK_NULL_HANDLE),
					m_GraphicsCommandBufferFreeList(m_Renderer), m_TransferCommandBufferFreeList(m_Renderer) {
				VkCommandPoolCreateInfo graphicsCommandPoolInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.queueFamilyIndex = m_Renderer.m_GpuGraphicsQueueFamilyIndex,
				};

				VkCommandPoolCreateInfo transferCommandPoolInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.queueFamilyIndex = m_Renderer.m_GpuTransferQueueFamilyIndex,
				};

				m_Renderer.VkAssert(
					vkCreateCommandPool(m_Renderer.m_GpuDevice, &graphicsCommandPoolInfo, 
						m_Renderer.m_GpuAllocationCallbacks, &m_GpuGraphicsCommandPool), 
					"failed to create command pool for thread (function vkCreateCommandPool in Thread constructor)");

				m_Renderer.VkAssert(
					vkCreateCommandPool(m_Renderer.m_GpuDevice, &transferCommandPoolInfo, 
						m_Renderer.m_GpuAllocationCallbacks, &m_GpuTransferCommandPool), 
					"failed to create command pool for thread (function vkCreateCommandPool in Thread constructor)");

				m_GraphicsCommandBufferFreeList.Initialize(m_GpuGraphicsCommandPool);
				m_TransferCommandBufferFreeList.Initialize(m_GpuTransferCommandPool);
			}

			void Terminate() {
				if (std::this_thread::get_id() != m_Renderer.m_MainThreadID) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Threading, 
						"attempting to terminate thread from a thread that isn't the programs main thread (in function Thread::Terminate)!", 
						VK_SUCCESS);
					return;
				}
				m_Thread.join();
				m_GraphicsCommandBufferFreeList.FreeAll();
				m_TransferCommandBufferFreeList.FreeAll();
				vkDestroyCommandPool(m_Renderer.m_GpuDevice, m_GpuGraphicsCommandPool, m_Renderer.m_GpuAllocationCallbacks);
				m_GpuGraphicsCommandPool = VK_NULL_HANDLE;
				vkDestroyCommandPool(m_Renderer.m_GpuDevice, m_GpuTransferCommandPool, m_Renderer.m_GpuAllocationCallbacks);
				m_GpuTransferCommandPool = VK_NULL_HANDLE;
			}
		};

		struct ThreadList {

			typedef Thread* Iterator;
			typedef Thread* const ConstItererator;

			static constexpr size_t max_count = 256;

			Thread* const m_Data;
			size_t m_Size;

			ThreadList() : m_Data((Thread*)malloc(max_count)), m_Size(0) {}

			~ThreadList() {
				free(m_Data);
			}

			Thread* Push(Renderer& renderer, std::thread&& thread) {
				if (m_Size >= max_count) {
					PrintError("thread list was out of memory (function ThreadList::Push)!", ErrorOrigin::OutOfMemory);
					return nullptr;
				}
				return new(&m_Data[m_Size++]) Thread(renderer, std::move(thread));
			}

			Thread* Find(std::thread::id id) const {
				for (size_t i = 0; i < m_Size; i++) {
					if (m_Data[i].m_ID == id) {
						return &m_Data[i];
					}
				}
				PrintError("failed to find thread (function ThreadList::Find)", ErrorOrigin::Threading);
				return nullptr;
			}

			Iterator begin() const {
				return m_Data;
			}

			ConstItererator end() const {
				return &m_Data[m_Size];
			}
		};

		struct Buffer;

		enum class CommandBufferUsage {
			DoNothing = 0,
			Free = 1,
			DestroyStagingBuffer = 2,
		};	

		typedef uint32_t CommandBufferUsageFlags;

		template<Queue queue_T>
		struct CommandBuffer {
			VkCommandBuffer m_GpuCommandBuffer{};
			Thread* m_Thread{};
			CommandBufferUsageFlags m_Flags{};
			VkBuffer m_GpuBuffer{};
			VkDeviceMemory m_GpuDeviceMemory{};
		};

		template<Queue queue_T, uint32_t max_buffers_T>
		struct CommandBufferQueue {

			typedef CommandBuffer<queue_T> CommandBuffer;
			typedef CommandBuffer* Iterator;
			typedef CommandBuffer* const ConstItererator;

			const Renderer& m_Renderer;
			std::mutex m_Mutex{};
			CommandBuffer* const m_Data;
			uint32_t m_Size;

			CommandBufferQueue(Renderer& renderer) noexcept : m_Renderer(renderer), m_Size(0), 
				m_Data((CommandBuffer*)malloc(max_buffers_T * sizeof(CommandBuffer))) {}

			~CommandBufferQueue() {
				free(m_Data);
			}

			CommandBuffer& New() {
				if (m_Size >= max_buffers_T) {
					m_Renderer.m_CriticalErrorCallback(&m_Renderer, ErrorOrigin::OutOfMemory, 
						"command buffer queue was full (in function CommandBufferQueue::New)!", VK_SUCCESS);
				}
				return *new(&m_Data[m_Size++]) CommandBuffer();
			}

			void Clear() {
				m_Size = 0;
			}

			void Lock() {
				m_Mutex.lock();
			}

			void Unlock() {
				m_Mutex.unlock();
			}

			Iterator begin() const {
				return m_Data;
			}

			ConstItererator end() const {
				return m_Data[m_Size];
			}
		};

		struct Buffer {

			Renderer& m_Renderer;
			VkBuffer m_GpuBuffer;
			VkDeviceMemory m_GpuDeviceMemory;
			VkDeviceSize m_BufferSize;	

			Buffer(Renderer& renderer) noexcept 
				: m_Renderer(renderer), m_GpuBuffer(VK_NULL_HANDLE), m_GpuDeviceMemory(VK_NULL_HANDLE), m_BufferSize(0) {}

			Buffer(Buffer&& other) 
				: m_Renderer(other.m_Renderer), m_GpuBuffer(other.m_GpuBuffer), m_GpuDeviceMemory(other.m_GpuDeviceMemory),
					m_BufferSize(other.m_BufferSize) {
				other.m_GpuBuffer = VK_NULL_HANDLE;
				other.m_GpuDeviceMemory = VK_NULL_HANDLE;
				other.m_BufferSize = 0;
			}

			~Buffer() {
				Terminate();
			}

			void Terminate() {
				vkFreeMemory(m_Renderer.m_GpuDevice, m_GpuDeviceMemory, m_Renderer.m_GpuAllocationCallbacks);
				m_GpuDeviceMemory = VK_NULL_HANDLE;
				vkDestroyBuffer(m_Renderer.m_GpuDevice, m_GpuBuffer, m_Renderer.m_GpuAllocationCallbacks);
				m_GpuBuffer = VK_NULL_HANDLE;
				m_BufferSize = 0;
			}

			bool Create(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, 
				VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queueFamilyIndexCount = 0, 
				const uint32_t* pQueueFamilyIndices = nullptr) {
				if (m_GpuBuffer != VK_NULL_HANDLE || m_GpuDeviceMemory != VK_NULL_HANDLE) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Uncategorized, 
						"attempting to create buffer (in function Buffer::Create) when the buffer has already been created!", VK_SUCCESS);
					return false;
				}
				VkBufferCreateInfo bufferInfo {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.size = size,
					.usage = bufferUsage,
					.sharingMode = sharingMode,
					.queueFamilyIndexCount = queueFamilyIndexCount,
					.pQueueFamilyIndices = pQueueFamilyIndices,
				};
				if (!m_Renderer.VkCheck(vkCreateBuffer(m_Renderer.m_GpuDevice, &bufferInfo, m_Renderer.m_GpuAllocationCallbacks, &m_GpuBuffer),
						"failed to create buffer (function vkCreateBuffer in function Buffer::Create)!")) {
					m_GpuBuffer = VK_NULL_HANDLE;
					return {};
				}
				VkMemoryRequirements memRequirements{};
				vkGetBufferMemoryRequirements(m_Renderer.m_GpuDevice, m_GpuBuffer, &memRequirements);
				VkMemoryAllocateInfo allocInfo {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.pNext = nullptr,
					.allocationSize = memRequirements.size,
				};
				if (!m_Renderer.FindMemoryTypeIndex(memRequirements.memoryTypeBits, bufferProperties, allocInfo.memoryTypeIndex)) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Vulkan,
						"failed to find memory type index when creating buffer (function FindMemoryTypeIndex in function Buffer::Create)!", 
						VK_SUCCESS);
					Terminate();
					return {};
				}
				if (!m_Renderer.VkCheck(
						vkAllocateMemory(m_Renderer.m_GpuDevice, &allocInfo, m_Renderer.m_GpuAllocationCallbacks, &m_GpuDeviceMemory),
						"failed to allocate memory for buffer (function vkAllocateMemory in function Buffer::Create)!")) {
					Terminate();
					return {};
				}
				if (!m_Renderer.VkCheck(vkBindBufferMemory(m_Renderer.m_GpuDevice, m_GpuBuffer, m_GpuDeviceMemory, 0),
						"failed to bind buffer memory (function vkBindBufferMemory in function Buffer::Create)!")) {
					Terminate();
					return {};
				}
				return true;
			}

			bool CreateWithData(VkDeviceSize size, const void* data,
				VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties,
				VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queueFamilyIndexCount = 0, 
				const uint32_t* pQueueFamilyIndices = nullptr) {

				Buffer stagingBuffer(m_Renderer);
				if (!stagingBuffer.Create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Buffer, 
						"failed to create buffer (function Buffer::Create in function Buffer::CreateWithData)!", VK_SUCCESS);
				}

				void* stagingMap;
				vkMapMemory(m_Renderer.m_GpuDevice, stagingBuffer.m_GpuDeviceMemory, 0, size, 0, &stagingMap);
				memcpy(stagingMap, data, size);
				vkUnmapMemory(m_Renderer.m_GpuDevice, stagingBuffer.m_GpuDeviceMemory);

				if (!Create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage, 
						bufferProperties, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices)) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Buffer, 
						"failed to create buffer (function Buffer::Create in function Buffer::CreateWithData)!", VK_SUCCESS);
					return false;
				}

				VkCommandBufferAllocateInfo commandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = m_Renderer.GetThisThreadCommandPool<Queue::Transfer>(),
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				if (commandBufferAllocInfo.commandPool == VK_NULL_HANDLE) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Threading, 
						"couldn't find command pool for thread (function GetThisThreadCommandPool in function Buffer::CreateWithData)!", 
						VK_SUCCESS);
					Terminate();
					return false;
				}
				m_Renderer.m_TransferCommandBufferQueue.Lock();
				CommandBuffer<Queue::Transfer>& commandBuffer = m_Renderer.m_TransferCommandBufferQueue.New();
				if (!m_Renderer.VkCheck(vkAllocateCommandBuffers(m_Renderer.m_GpuDevice, 
						&commandBufferAllocInfo, &commandBuffer.m_GpuCommandBuffer),
						"failed to allocate command buffer for staging transfer (function vkAllocateCommandBuffers in function Buffer::CreateWithData)!"
					)) {
					Terminate();
					return false;
				}
				VkCommandBufferBeginInfo commandBufferBeginInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext = nullptr,
					.flags = 0,
					.pInheritanceInfo = nullptr,
				};
				if (!m_Renderer.VkCheck(vkBeginCommandBuffer(commandBuffer.m_GpuCommandBuffer, &commandBufferBeginInfo), 
						"failed to begin command buffer for staging transfer (function vkBeginCommandBuffer in function Buffer::CreateWithData)!"
				)) {
					Terminate();
					return false;
				}
				VkBufferCopy copyRegion {
					.srcOffset = 0,
					.dstOffset = 0,
					.size = size,
				};
				vkCmdCopyBuffer(commandBuffer.m_GpuCommandBuffer, stagingBuffer.m_GpuBuffer, m_GpuBuffer, 1, &copyRegion);
				if (!m_Renderer.VkCheck(vkEndCommandBuffer(commandBuffer.m_GpuCommandBuffer),
						"failed to end command buffer (function vkEndCommandBuffer in function Buffer::CreateWithData)")) {
					return false;
				}
				commandBuffer.m_Flags = (uint32_t)CommandBufferUsage::DestroyStagingBuffer | (uint32_t)CommandBufferUsage::Free;
				commandBuffer.m_GpuBuffer = stagingBuffer.m_GpuBuffer;
				stagingBuffer.m_GpuBuffer = VK_NULL_HANDLE;
				commandBuffer.m_GpuDeviceMemory = stagingBuffer.m_GpuDeviceMemory;
				stagingBuffer.m_GpuDeviceMemory = VK_NULL_HANDLE;
				m_Renderer.m_TransferCommandBufferQueue.Unlock();
				return true;
			}

			bool CopyBuffer(Buffer& dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size) const {
				if (m_GpuBuffer == VK_NULL_HANDLE || dst.m_GpuBuffer == VK_NULL_HANDLE) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + srcOffset is larger than source size (in function Buffer::CopyBuffer)!", 
						VK_SUCCESS);
				}
				if (m_BufferSize < size + srcOffset) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + srcOffset is larger than source size (in function Buffer::CopyBuffer)!", 
						VK_SUCCESS);
					return false;
				}
				if (dst.m_BufferSize < size + dstOffset) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + dstOffset is larger than destination size (in function Buffer::CopyBuffer)!", 
						VK_SUCCESS);
					return false;
				}
				m_Renderer.m_TransferCommandBufferQueue.Lock();
				CommandBuffer<Queue::Transfer> commandBuffer = m_Renderer.m_TransferCommandBufferQueue.New();
				VkCommandBufferAllocateInfo commandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = m_Renderer.GetThisThreadCommandPool<Queue::Transfer>(),
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				if (!m_Renderer.VkCheck(vkAllocateCommandBuffers(m_Renderer.m_GpuDevice, &commandBufferAllocInfo, 
						&commandBuffer.m_GpuCommandBuffer), 
						"failed to allocate command buffer (function vkAllocateCommandBuffers in function Buffer::CopyBuffer)!")) {
					return false;
				}
				VkCommandBufferBeginInfo commandBufferBeginInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext = nullptr,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
					.pInheritanceInfo = nullptr,
				};
				if (!m_Renderer.VkCheck(vkBeginCommandBuffer(commandBuffer.m_GpuCommandBuffer, &commandBufferBeginInfo), 
						"failed to begin command buffer (function vkBeginCommandBuffer in function Buffer::CopyBuffer)!")) {
					return false;
				}
				VkBufferCopy copyRegion {
					.srcOffset = srcOffset,
					.dstOffset = dstOffset,
					.size = size,
				};
				vkCmdCopyBuffer(commandBuffer.m_GpuCommandBuffer, m_GpuBuffer, dst.m_GpuBuffer, 1, &copyRegion);
				if (!m_Renderer.VkCheck(vkEndCommandBuffer(commandBuffer.m_GpuCommandBuffer),
						"failed to end command buffer (function vkEndCommandBuffer in function Buffer::CopyBuffer)")) {
					return false;
				}
				m_Renderer.m_TransferCommandBufferQueue.Unlock();
				return true;
			}
		};

		struct StagingBufferDestroyList {

			struct BufferData {
				VkBuffer m_GpuBuffer{};
				VkDeviceMemory m_GpuDeviceMemory{};
			};
			
			static constexpr size_t max_buffers_per_frame = 1000;

			const Renderer& m_Renderer;
			uint32_t m_FramesInFlight;
			BufferData(*m_Data)[max_buffers_per_frame];
			size_t* m_Counts;

			StagingBufferDestroyList(Renderer& renderer) 
				: m_Renderer(renderer), m_FramesInFlight(0), m_Data(nullptr), m_Counts(nullptr) {}

			void Terminate() {
				DestroyAll();
			}

			~StagingBufferDestroyList() {
				free(m_Data);
				delete[] m_Counts;
			}

			void Initialize() {
				if (m_FramesInFlight != 0) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::InitializationFailed, 
						"attempting to initialize command buffer free list more than once (function CommandBufferFreeList::Initialize)!",
						VK_SUCCESS);
					return;
				}
				m_FramesInFlight = m_Renderer.m_FramesInFlight;
				m_Data = (BufferData(*)[max_buffers_per_frame])malloc(m_FramesInFlight 
					* sizeof(BufferData[max_buffers_per_frame]));
				m_Counts = new size_t [m_FramesInFlight]{};
			}

			void Push(uint32_t currentFrame, VkBuffer buffer, VkDeviceMemory deviceMemory) {
				if (currentFrame >= m_FramesInFlight) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::IndexOutOfBounds, 
					"current frame goes out of bounds of staging buffer destroy list frames in flight (function StagingBufferDestroyList::Push)!", 
						VK_SUCCESS);
					return;
				}
				size_t& count = m_Counts[currentFrame];
				if (count >= max_buffers_per_frame) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::OutOfMemory, 
						"staging buffer destroy list was out of memory (function StagingBufferDestroyList::Push)!", VK_SUCCESS);
					return;
				}
				m_Data[currentFrame][count++] = { buffer, deviceMemory };
			}

			void Destroy(uint32_t currentFrame) {
				if (currentFrame >= m_FramesInFlight) {
					m_Renderer.m_ErrorCallback(&m_Renderer, ErrorOrigin::IndexOutOfBounds, 
					"given frame goes out of bounds of staging buffer destroy list frames in flight (function StagingBufferDestroyList::Destroy)!", 
						VK_SUCCESS);
					return;
				}
				size_t& count = m_Counts[currentFrame];
				BufferData(&buffers)[max_buffers_per_frame] = m_Data[currentFrame];
				for (size_t i = 0; i < count; i++) {
					BufferData& data = buffers[i];
					vkDestroyBuffer(m_Renderer.m_GpuDevice, data.m_GpuBuffer, m_Renderer.m_GpuAllocationCallbacks);
					vkFreeMemory(m_Renderer.m_GpuDevice, data.m_GpuDeviceMemory, m_Renderer.m_GpuAllocationCallbacks);
				}
				count = 0;
			}

			void DestroyAll() {
				for (uint32_t i = 0; i < m_FramesInFlight; i++) {
					size_t& count = m_Counts[i];
					for (size_t j = 0; j < count; j++) {
						BufferData& data = m_Data[i][j];
						vkDestroyBuffer(m_Renderer.m_GpuDevice, data.m_GpuBuffer, m_Renderer.m_GpuAllocationCallbacks);
						vkFreeMemory(m_Renderer.m_GpuDevice, data.m_GpuDeviceMemory, m_Renderer.m_GpuAllocationCallbacks);
					}
					count = 0;
				}
			}	

			void Reallocate() {
				DestroyAll();
				if (m_FramesInFlight != m_Renderer.m_FramesInFlight) {
					m_FramesInFlight = m_Renderer.m_FramesInFlight;
					free(m_Data);
					delete[] m_Counts;
					m_Data = (BufferData(*)[max_buffers_per_frame])malloc(m_FramesInFlight 
						* sizeof(BufferData[max_buffers_per_frame]));
					m_Counts = new size_t[m_FramesInFlight]{};
				}
			}
		};

		struct Fence {

			enum class State {
				None = 0,
				Resettable = 1,
			};

			VkFence fence{};
			State state{};
		};

		static constexpr uint32_t desired_frames_in_flight = 2;
		static constexpr const char* gpu_validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* gpu_dynamic_rendering_extension_name = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
		static constexpr const char* gpu_timeline_semaphore_extension_name = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		static constexpr const char* gpu_swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		static constexpr size_t single_thread_stack_size = 524288;
		static constexpr size_t max_pending_graphics_command_buffer_count = 250000;
		static constexpr size_t max_pending_transfer_command_buffer_count = 250000;
		static constexpr size_t graphics_command_buffer_stack_size 
			= sizeof(CommandBuffer<Queue::Graphics>) * max_pending_graphics_command_buffer_count;
		static constexpr size_t transfer_command_buffer_stack_size 
			= sizeof(CommandBuffer<Queue::Transfer>) * max_pending_transfer_command_buffer_count;
		static constexpr size_t in_flight_render_stack_size = 512;

		static constexpr size_t max_model_descriptor_sets = 250000;

		static const char* ErrorOriginString(ErrorOrigin origin) {
			static constexpr const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"InitializationFailed",
				"Vulkan",
				"OutOfMemory",
				"NullDereference",
				"IndexOutOfBounds",
				"Shader",
				"Buffer",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		static void PrintMessage(const char* msg) {
			printf("Renderer message: %s\n", msg);
		}

		static void PrintWarning(const char* warn) {
			printf("Renderer warning: %s\n", warn);
		}

		static void PrintError(const char* err, ErrorOrigin origin) {
			printf("Renderer error: %s\n Error origin: %s\n", err, Renderer::ErrorOriginString(origin));
		}

		Stack m_SingleThreadStack;
		Stack m_InFlightRenderStack;

		VkDevice m_GpuDevice = VK_NULL_HANDLE;
		VkAllocationCallbacks* m_GpuAllocationCallbacks = nullptr;
		VkPhysicalDevice m_Gpu = VK_NULL_HANDLE;
		std::mutex m_ThreadStackMutex{};
		const std::thread::id m_MainThreadID;
		CommandBufferFreeList m_GraphicsCommandBufferFreeList;
		CommandBufferFreeList m_TransferCommandBufferFreeList;
		StagingBufferDestroyList m_StagingBufferDestroyList;
		ThreadList m_ThreadList;
		VkCommandPool m_GpuGraphicsCommandPool = VK_NULL_HANDLE;
		VkCommandPool m_GpuTransferCommandPool = VK_NULL_HANDLE;

		CommandBufferQueue<Queue::Graphics, 100000> m_GraphicsCommandBufferQueue;
		CommandBufferQueue<Queue::Transfer, 100000> m_TransferCommandBufferQueue;

		VkSampleCountFlags m_GpuColorMsaaSamples = 1;
		VkSampleCountFlags m_GpuDepthMsaaSamples = 1;
		VkSurfaceKHR m_GpuSurface = VK_NULL_HANDLE;
		uint32_t m_GpuMaxFragmentOutputAttachments;

		VkDescriptorSetLayout m_ModelDescriptorSetLayout;
		DescriptorPool m_ModelDescriptorPool { *this, max_model_descriptor_sets };

		Stack::Array<VkCommandBuffer> m_GpuRenderCommandBuffers { 0, nullptr };
		Stack::Array<VkSemaphore> m_GpuRenderFinishedSemaphores { 0, nullptr };
		Stack::Array<VkSemaphore> m_GpuRenderWaitSemaphores { 0, nullptr };
		Stack::Array<Fence> m_GpuInFlightGraphicsFences { 0, nullptr };
		Stack::Array<VkSemaphore> m_GpuTransferFinishedSemaphores { 0, nullptr };
		Stack::Array<VkSemaphore> m_GpuTransferWaitSemaphores { 0, nullptr };
		Stack::Array<Fence> m_GpuInFlightTransferFences { 0, nullptr };

		Stack::Array<VkImageView> m_GpuSwapchainImageViews { 0, nullptr };
		VkQueue m_GpuGraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_GpuTransferQueue = VK_NULL_HANDLE;

		VkQueue m_GpuPresentQueue = VK_NULL_HANDLE;
		VkExtent2D m_GpuSwapchainExtent = { 0, 0 };
		uint32_t m_FramesInFlight = 0;
		uint32_t m_CurrentFrame = 0;

		uint32_t m_GpuGraphicsQueueFamilyIndex = 0;
		uint32_t m_GpuTransferQueueFamilyIndex = 0;
		uint32_t m_GpuPresentQueueFamilyIndex = 0;

		GLFWwindow* m_Window = nullptr;
		VkSwapchainKHR m_GpuSwapchain = VK_NULL_HANDLE;
		Stack::Array<VkImage> m_GpuSwapchainImages { 0, 0 };
		VkSurfaceFormatKHR m_GpuSwapchainSurfaceFormat{};
		VkPresentModeKHR m_GpuPresentMode{};
		SwapchainCreateCallback m_SwapchainCreateCallback;

		VkInstance m_GpuInstance = VK_NULL_HANDLE;

		ErrorCallback m_ErrorCallback;
		ErrorCallback m_CriticalErrorCallback;

		bool VkAssert(VkResult result, const char* err) const {
			if (result != VK_SUCCESS) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, err, result);
				return false;
			}
			return true;
		}

		bool VkCheck(VkResult result, const char* err) const {
			if (result != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, err, result);
				return false;
			}
			return true;
		}

		template<typename T>
		T* PtrAssert(T* ptr, const char* err) const {
			if (!ptr) {
				m_CriticalErrorCallback(this, ErrorOrigin::NullDereference, err, VK_SUCCESS);
			}
			return ptr;
		}

		Renderer(const char* appName, uint32_t appVersion, GLFWwindow* window, ErrorCallback criticalErrorCallback, ErrorCallback errorCallback, 
			SwapchainCreateCallback swapchainCreateCallback)
			: m_SingleThreadStack(single_thread_stack_size, (uint8_t*)malloc(single_thread_stack_size)), 
				m_InFlightRenderStack(in_flight_render_stack_size, (uint8_t*)malloc(in_flight_render_stack_size)), m_Window(window),
				m_ThreadList(),
				m_GraphicsCommandBufferQueue(*this),
				m_TransferCommandBufferQueue(*this),
				m_GraphicsCommandBufferFreeList(*this),
				m_TransferCommandBufferFreeList(*this),
				m_StagingBufferDestroyList(*this),
				m_MainThreadID(std::this_thread::get_id()),
				m_CriticalErrorCallback(criticalErrorCallback), m_ErrorCallback(errorCallback), m_SwapchainCreateCallback(swapchainCreateCallback) {

			static_assert(sizeof(size_t) >= 4, "size of size_t isn't big enough!");

			assert(m_CriticalErrorCallback && "critical error callback was null (renderer)!");
			assert(m_ErrorCallback && "error callback was null (renderer)!");
			assert(m_SwapchainCreateCallback && "swapchain create callback was null (renderer)!");

			uint32_t instanceExtensionCount;
			const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCount);

			uint32_t instanceExtensionsNotFoundCount = instanceExtensionCount;

			uint32_t availableInstanceExtensionCount;
			vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, nullptr);
			Stack::Array<VkExtensionProperties> availableGpuInstanceExtensions 
				= m_SingleThreadStack.Allocate<VkExtensionProperties>(availableInstanceExtensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, availableGpuInstanceExtensions.m_Data);
			for (VkExtensionProperties& extension : availableGpuInstanceExtensions) {
				const char** iter = instanceExtensions;
				const char** const end = &instanceExtensions[instanceExtensionCount];
				for (; iter != end && availableInstanceExtensionCount; iter++) {
					if (!strcmp(extension.extensionName, *iter)) {
						--instanceExtensionsNotFoundCount;
						continue;
					}
				}
			}

			bool includeGpuValidationLayer = false;
			if constexpr (BOOL_RENDERER_DEBUG) {
				uint32_t availableInstanceLayerCount;
				vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount, nullptr);
				Stack::Array<VkLayerProperties> availableGpuInstanceLayers 
					= m_SingleThreadStack.Allocate<VkLayerProperties>(availableInstanceLayerCount);
				vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount, availableGpuInstanceLayers.m_Data);
				for (VkLayerProperties& layer : availableGpuInstanceLayers) {
					if (!strcmp(layer.layerName, gpu_validation_layer_name)) {
						includeGpuValidationLayer = true;
						break;
					}
				}
				if (!includeGpuValidationLayer) {
					PrintWarning("Vulkan Khronos validation not supported (in Renderer constructor)!");
				}
			}

			VkApplicationInfo appInfo {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pNext = nullptr,
				.pApplicationName = appName,
				.applicationVersion = appVersion,
				.pEngineName = "bones engine",
				.engineVersion = VK_MAKE_API_VERSION(0, 0, 5, 0),
				.apiVersion = VK_API_VERSION_1_3,
			};

			VkInstanceCreateInfo instanceCreateInfo {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = includeGpuValidationLayer ? 1U : 0U,
				.ppEnabledLayerNames = includeGpuValidationLayer ? &gpu_validation_layer_name : nullptr,
				.enabledExtensionCount = instanceExtensionCount,
				.ppEnabledExtensionNames = instanceExtensions,
			};

			if (!VkAssert(vkCreateInstance(&instanceCreateInfo, m_GpuAllocationCallbacks, &m_GpuInstance), 
					"failed to create vulkan instance (function vkCreateInstance in Renderer constructor)!")) {
				return;
			}

			if (!VkAssert(glfwCreateWindowSurface(m_GpuInstance, m_Window, m_GpuAllocationCallbacks, &m_GpuSurface), 
					"failed to create window surface (function glfwCreateWindowSurface in Renderer constructor)!")) {
				return;
			}

			uint32_t gpuCount;
			vkEnumeratePhysicalDevices(m_GpuInstance, &gpuCount, nullptr);
			Stack::Array<VkPhysicalDevice> gpus = m_SingleThreadStack.Allocate<VkPhysicalDevice>(gpuCount);
			vkEnumeratePhysicalDevices(m_GpuInstance, &gpuCount, gpus.m_Data);

			int bestGpuScore = 0;
			VkPhysicalDevice bestGpu = VK_NULL_HANDLE;
			uint32_t bestGpuQueueFamilyIndices[3];
			VkSampleCountFlags bestGpuColorSamples = 1;
			VkSampleCountFlags bestGpuDepthSamples = 1;

			for (VkPhysicalDevice gpu : gpus) {
				uint32_t surfaceFormatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_GpuSurface, &surfaceFormatCount, nullptr);
				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_GpuSurface, &presentModeCount, nullptr);
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(gpu, &features);		
				if (!surfaceFormatCount || !presentModeCount || !features.samplerAnisotropy || !features.fillModeNonSolid) {
					continue;
				}
				uint32_t deviceExtensionCount;
				vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, nullptr);
				Stack::Array<VkExtensionProperties> deviceExtensions = m_SingleThreadStack.Allocate<VkExtensionProperties>(deviceExtensionCount);
				vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, deviceExtensions.m_Data);
				bool dynamicRenderingExtensionFound = false;
				bool timelineSemaphoreExtensionFound = false;
				uint32_t deviceExtensionsNotFound = 0;
				for (VkExtensionProperties& extension : deviceExtensions) {
					if (dynamicRenderingExtensionFound && timelineSemaphoreExtensionFound) {
						break;
					}
					if (!dynamicRenderingExtensionFound && !strcmp(gpu_dynamic_rendering_extension_name, extension.extensionName)) {
						dynamicRenderingExtensionFound = true;
					}
					if (!timelineSemaphoreExtensionFound && !strcmp(gpu_timeline_semaphore_extension_name, extension.extensionName)) {
						timelineSemaphoreExtensionFound = true;
					}
				}
				m_SingleThreadStack.Deallocate<VkExtensionProperties>(deviceExtensions.m_Size);
				if (!dynamicRenderingExtensionFound || !timelineSemaphoreExtensionFound) {
					continue;
				}
				uint32_t queueFamilyCount;
				vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
				Stack::Array<VkQueueFamilyProperties> queueFamilies = m_SingleThreadStack.Allocate<VkQueueFamilyProperties>(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.m_Data);
				uint32_t queueFamilyIndices[3]; 
				bool graphicsQueueFound = false;
				bool transferQueueFound = false;
				bool presentQueueFound = false;
				uint32_t queueFamilyIndex = 0;
				for (VkQueueFamilyProperties& queueFamily : queueFamilies) {
					if (!graphicsQueueFound && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
						queueFamilyIndices[0] = queueFamilyIndex++;
						graphicsQueueFound = true;
						continue;
					}
					if (!transferQueueFound && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						queueFamilyIndices[1] = queueFamilyIndex++;
						transferQueueFound = true;
						continue;
					}
					if (!presentQueueFound) {
						VkBool32 presentSupported;
						vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queueFamilyIndex, m_GpuSurface, &presentSupported);
						if (presentSupported) {
							queueFamilyIndices[2] = queueFamilyIndex++;
							presentQueueFound = true;
						}
					}
					if (graphicsQueueFound && transferQueueFound && presentQueueFound) {
						break;
					}
					++queueFamilyIndex;
				}
				m_SingleThreadStack.Deallocate<VkQueueFamilyProperties>(queueFamilies.m_Size);
				int score = 10;
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(gpu, &properties);
				if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
					score += 100;
				}
				if (score > bestGpuScore) {
					m_GpuMaxFragmentOutputAttachments = properties.limits.maxFragmentOutputAttachments;
					bestGpuScore = score;
					bestGpu = gpu;
					bestGpuQueueFamilyIndices[0] = queueFamilyIndices[0];
					bestGpuQueueFamilyIndices[1] = queueFamilyIndices[1];
					bestGpuQueueFamilyIndices[2] = queueFamilyIndices[2];
					bestGpuColorSamples = properties.limits.sampledImageColorSampleCounts;
					bestGpuDepthSamples = properties.limits.sampledImageColorSampleCounts;
				}
			}

			if (bestGpu == VK_NULL_HANDLE) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "failed to find suitable gpu (in Renderer constructor)!", VK_SUCCESS);
			}

			m_Gpu = bestGpu;
			m_GpuColorMsaaSamples = bestGpuColorSamples;
			m_GpuDepthMsaaSamples = bestGpuDepthSamples;
			m_GpuGraphicsQueueFamilyIndex = bestGpuQueueFamilyIndices[0];
			m_GpuTransferQueueFamilyIndex = bestGpuQueueFamilyIndices[1];
			m_GpuPresentQueueFamilyIndex = bestGpuQueueFamilyIndices[2];

			VkDeviceQueueCreateInfo deviceQueueInfos[3];
			float queuePriority = 1.0f;
			for (size_t i = 0; i < 3; i++) {
				deviceQueueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				deviceQueueInfos[i].pNext = nullptr;
				deviceQueueInfos[i].flags = 0;
				deviceQueueInfos[i].queueFamilyIndex = bestGpuQueueFamilyIndices[i];
				deviceQueueInfos[i].queueCount = 1;
				deviceQueueInfos[i].pQueuePriorities = &queuePriority;
			}

			VkPhysicalDeviceFeatures gpuFeatures {
				.sampleRateShading = VK_TRUE,
				.fillModeNonSolid = VK_TRUE,
				.samplerAnisotropy = VK_TRUE,
			};

			VkPhysicalDeviceVulkan12Features gpuFeaturesVulkan12 {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.pNext = nullptr,
				.timelineSemaphore = VK_TRUE,
			};

			VkPhysicalDeviceVulkan13Features gpuFeaturesVulkan13 {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &gpuFeaturesVulkan12,
				.dynamicRendering = VK_TRUE,
			};

			VkDeviceCreateInfo deviceInfo {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &gpuFeaturesVulkan13,
				.queueCreateInfoCount = 3,
				.pQueueCreateInfos = deviceQueueInfos,
				.enabledExtensionCount = 1,
				.ppEnabledExtensionNames = &gpu_swapchain_extension_name,
				.pEnabledFeatures = &gpuFeatures,
			};

			if (!VkAssert(vkCreateDevice(m_Gpu, &deviceInfo, m_GpuAllocationCallbacks, &m_GpuDevice), 
					"failed to create vulkan device (function vkCreateDevice in Renderer constructor)!")) {
				return;
			}

			vkGetDeviceQueue(m_GpuDevice, m_GpuGraphicsQueueFamilyIndex, 0, &m_GpuGraphicsQueue);
			vkGetDeviceQueue(m_GpuDevice, m_GpuTransferQueueFamilyIndex, 0, &m_GpuTransferQueue);
			vkGetDeviceQueue(m_GpuDevice, m_GpuPresentQueueFamilyIndex, 0, &m_GpuPresentQueue);	

			m_SingleThreadStack.Clear();

			VkCommandPoolCreateInfo graphicsCommandPoolInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_GpuGraphicsQueueFamilyIndex,
			};

			VkAssert(vkCreateCommandPool(m_GpuDevice, &graphicsCommandPoolInfo, m_GpuAllocationCallbacks, &m_GpuGraphicsCommandPool),
				"failed to create transfer command pool (function vkCreateCommandPool in Renderer constructor)!");

			VkCommandPoolCreateInfo transferCommandPoolInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_GpuTransferQueueFamilyIndex,
			};

			VkAssert(vkCreateCommandPool(m_GpuDevice, &transferCommandPoolInfo, m_GpuAllocationCallbacks, &m_GpuTransferCommandPool),
				"failed to create transfer command pool (function vkCreateCommandPool in Renderer constructor)!");

			CreateSwapchain();

			m_GraphicsCommandBufferFreeList.Initialize(m_GpuGraphicsCommandPool);
			m_TransferCommandBufferFreeList.Initialize(m_GpuTransferCommandPool);
			m_StagingBufferDestroyList.Initialize();

			VkDescriptorPoolSize modelPoolSize {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2,
			};

			m_ModelDescriptorPool.CreatePool(1, &modelPoolSize);
		}	

		~Renderer() {
			Terminate();
			free(m_SingleThreadStack.m_Data);
			free(m_InFlightRenderStack.m_Data);
		}

		void Terminate() {
			if (m_GpuDevice) {
				m_ModelDescriptorPool.Terminate();
				vkDeviceWaitIdle(m_GpuDevice);
				for (size_t i = 0; i < m_FramesInFlight; i++) {
					vkDestroyImageView(m_GpuDevice, m_GpuSwapchainImageViews[i], m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuRenderFinishedSemaphores[i], m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuRenderWaitSemaphores[i], m_GpuAllocationCallbacks);
					vkDestroyFence(m_GpuDevice, m_GpuInFlightGraphicsFences[i].fence, m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuTransferFinishedSemaphores[i], m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuTransferWaitSemaphores[i], m_GpuAllocationCallbacks);
					vkDestroyFence(m_GpuDevice, m_GpuInFlightTransferFences[i].fence, m_GpuAllocationCallbacks);
				}
				vkDestroySwapchainKHR(m_GpuDevice, m_GpuSwapchain, m_GpuAllocationCallbacks);
				vkDestroyCommandPool(m_GpuDevice, m_GpuGraphicsCommandPool, m_GpuAllocationCallbacks);
				vkDestroyCommandPool(m_GpuDevice, m_GpuTransferCommandPool, m_GpuAllocationCallbacks);
				m_StagingBufferDestroyList.Terminate();
				for (Thread& thread : m_ThreadList) {
					thread.Terminate();
				}
				vkDestroyDevice(m_GpuDevice, m_GpuAllocationCallbacks);
				m_GpuDevice = VK_NULL_HANDLE;
			}
			if (m_GpuInstance) {
				vkDestroySurfaceKHR(m_GpuInstance, m_GpuSurface, m_GpuAllocationCallbacks);
				m_GpuSurface = VK_NULL_HANDLE;
				vkDestroyInstance(m_GpuInstance, m_GpuAllocationCallbacks);
				m_GpuInstance = VK_NULL_HANDLE;
			}
		}

		template<Queue queue_T>
		VkCommandPool GetThisThreadCommandPool() const {
			static_assert(queue_T != Queue::Present, "invalid queue for function Renderer::GetThisThreadCommandPool");
			std::thread::id threadID = std::this_thread::get_id();
			if (threadID == m_MainThreadID) {
				if constexpr (queue_T == Queue::Graphics) {
					return m_GpuGraphicsCommandPool;
				}
				else if (queue_T == Queue::Transfer){
					return m_GpuTransferCommandPool;
				}
			}
			else {
				Thread* thread = m_ThreadList.Find(std::this_thread::get_id());
				if (thread) {
					if constexpr (queue_T == Queue::Graphics) {
						return thread->m_GpuGraphicsCommandPool;
					}
					else if (queue_T == Queue::Transfer) {
						return thread->m_GpuTransferCommandPool;
					}
				}
			}
			m_ErrorCallback(this, ErrorOrigin::Threading, 
				"attempting to get command pool from a thread that hasn't been created yet!", VK_SUCCESS);
			return VK_NULL_HANDLE;
		}

		void CreateSwapchain() {

			int framebufferWidth, framebufferHeight;
			glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);

			if (framebufferWidth == 0 || framebufferHeight == 0) {
				m_GpuSwapchainExtent = { 0, 0 };
				return;
			}

			static constexpr auto clamp = [](uint32_t val, uint32_t min, uint32_t max) -> uint32_t { 
				val = val > min ? val : min;
				return val < max ? val : max;
			};

			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Gpu, m_GpuSurface, &surfaceCapabilities);

			uint32_t surfaceFormatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_Gpu, m_GpuSurface, &surfaceFormatCount, nullptr);
			Stack::Array<VkSurfaceFormatKHR> surfaceFormats = m_SingleThreadStack.Allocate<VkSurfaceFormatKHR>(surfaceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_Gpu, m_GpuSurface, &surfaceFormatCount, surfaceFormats.m_Data);

			if (!surfaceFormatCount) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "vulkan surface format count was 0 (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}

			m_GpuSwapchainSurfaceFormat = surfaceFormats[0];
			for (VkSurfaceFormatKHR format : surfaceFormats) {
				if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
					m_GpuSwapchainSurfaceFormat = format;
					break;
				}
			}
			m_SingleThreadStack.Deallocate<VkSurfaceFormatKHR>(surfaceFormats.m_Size);

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_Gpu, m_GpuSurface, &presentModeCount, nullptr);
			Stack::Array<VkPresentModeKHR> presentModes = m_SingleThreadStack.Allocate<VkPresentModeKHR>(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_Gpu, m_GpuSurface, &presentModeCount, presentModes.m_Data);

			m_GpuPresentMode = VK_PRESENT_MODE_FIFO_KHR;
			for (VkPresentModeKHR presentMode : presentModes) {
				if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					m_GpuPresentMode = presentMode;
					break;
				}
			}
			m_SingleThreadStack.Deallocate<VkPresentModeKHR>(presentModes.m_Size);

			if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
				m_GpuSwapchainExtent = surfaceCapabilities.currentExtent;
			}
			else {
				VkExtent2D actualExtent {
					(uint32_t)framebufferWidth,
					(uint32_t)framebufferHeight,
				};
				actualExtent.width = clamp(
					actualExtent.width, 
					surfaceCapabilities.minImageExtent.width,
					surfaceCapabilities.maxImageExtent.width);
				actualExtent.height = clamp(
					actualExtent.height,
					surfaceCapabilities.minImageExtent.height,
					surfaceCapabilities.maxImageExtent.height);
				m_GpuSwapchainExtent = actualExtent;
			}

			m_FramesInFlight = clamp(desired_frames_in_flight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

			uint32_t queueFamilyIndices[2] {
				m_GpuGraphicsQueueFamilyIndex,
				m_GpuPresentQueueFamilyIndex,
			};

			VkSwapchainCreateInfoKHR swapchainInfo {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.pNext = nullptr,
				.flags = 0,
				.surface = m_GpuSurface,
				.minImageCount = m_FramesInFlight,
				.imageFormat = m_GpuSwapchainSurfaceFormat.format,
				.imageColorSpace = m_GpuSwapchainSurfaceFormat.colorSpace,
				.imageExtent = m_GpuSwapchainExtent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.imageSharingMode = VK_SHARING_MODE_CONCURRENT,
				.queueFamilyIndexCount = 2,
				.pQueueFamilyIndices = queueFamilyIndices,
				.preTransform = surfaceCapabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = m_GpuPresentMode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE,
			};

			if (!VkAssert(vkCreateSwapchainKHR(m_GpuDevice, &swapchainInfo, m_GpuAllocationCallbacks, &m_GpuSwapchain), 
					"failed to create vulkan swapchain (function vkCreateSwapchainKHR in function CreateSwapchain)!")) {
				return;
			}

			vkQueueWaitIdle(m_GpuGraphicsQueue);
			vkQueueWaitIdle(m_GpuTransferQueue);

			for (size_t i = 0; i < m_GpuSwapchainImageViews.m_Size; i++) {
				vkDestroyImageView(m_GpuDevice, m_GpuSwapchainImageViews[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, m_GpuRenderFinishedSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, m_GpuRenderWaitSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroyFence(m_GpuDevice, m_GpuInFlightGraphicsFences[i].fence, m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, m_GpuTransferFinishedSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, m_GpuTransferWaitSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroyFence(m_GpuDevice, m_GpuInFlightTransferFences[i].fence, m_GpuAllocationCallbacks);
			}

			m_InFlightRenderStack.Clear();

			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, nullptr);
			if (!m_InFlightRenderStack.Allocate<VkImage>(m_FramesInFlight, &m_GpuSwapchainImages)) {
				m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, m_GpuSwapchainImages.m_Data);

			if (!m_InFlightRenderStack.Allocate<VkImageView>(m_FramesInFlight, &m_GpuSwapchainImageViews) ||
				!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderFinishedSemaphores) ||
				!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderWaitSemaphores) ||
				!m_InFlightRenderStack.Allocate<Fence>(m_FramesInFlight, &m_GpuInFlightGraphicsFences) ||
				!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuTransferFinishedSemaphores) ||
				!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuTransferWaitSemaphores) ||
				!m_InFlightRenderStack.Allocate<Fence>(m_FramesInFlight, &m_GpuInFlightTransferFences) ||
				!m_InFlightRenderStack.Allocate<VkCommandBuffer>(m_FramesInFlight, &m_GpuRenderCommandBuffers)) {
				m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
					"in flight stack was out of memory (in function CreateSwapchain)!", 0);
			}

			VkCommandBufferAllocateInfo renderCommandBufferAllocInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_GpuGraphicsCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = m_FramesInFlight,
			};

			if(!VkAssert(vkAllocateCommandBuffers(m_GpuDevice, &renderCommandBufferAllocInfo, m_GpuRenderCommandBuffers.m_Data), 
					"failed to allocate render command buffers (function vkAllocateCommandBuffers in function CreateSwapchain!)")) {
				return;
			}

			/*

			VkSemaphoreTypeCreateInfo timelineSemaphoreTypeInfo {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
				.pNext = nullptr,
				.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
				.initialValue = 0,
			};

			VkSemaphoreCreateInfo timelineSemaphoreInfo {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = &timelineSemaphoreTypeInfo,
				.flags = 0,
			};

			*/

			VkSemaphoreCreateInfo semaphoreInfo {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
			};

			VkFenceCreateInfo fenceInfo {
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
			};

			for (size_t i = 0; i < m_FramesInFlight; i++) {
				VkImageViewCreateInfo imageViewInfo {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.image = m_GpuSwapchainImages[i],
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = m_GpuSwapchainSurfaceFormat.format,
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
				VkAssert(vkCreateImageView(m_GpuDevice, &imageViewInfo, m_GpuAllocationCallbacks, &m_GpuSwapchainImageViews[i]), 
						"failed to create swapchain image view (function vkCreateImageView in function CreateSwapchain)!");
				VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuRenderFinishedSemaphores[i]), 
						"failed to create render finished semaphore (function vkCreateSemaphore in function CreateSwapchain)");
				VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuRenderWaitSemaphores[i]), 
						"failed to create render wait semaphore (function vkCreateSemaphore in function CreateSwapchain)");
				VkAssert(vkCreateFence(m_GpuDevice, &fenceInfo, m_GpuAllocationCallbacks, &m_GpuInFlightGraphicsFences[i].fence), 
						"failed to create in flight graphis fence (function vkCreateFence in function CreateSwapchain)");
				VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuTransferFinishedSemaphores[i]), 
						"failed to create render finished semaphore (function vkCreateSemaphore in function CreateSwapchain)");
				VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuTransferWaitSemaphores[i]), 
						"failed to create render wait semaphore (function vkCreateSemaphore in function CreateSwapchain)");
				VkAssert(vkCreateFence(m_GpuDevice, &fenceInfo, m_GpuAllocationCallbacks, &m_GpuInFlightTransferFences[i].fence), 
						"failed to create in flight transfer fence (function vkCreateFence in function CreateSwapchain)");
				VkSubmitInfo dummySubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 0,
					.pCommandBuffers = nullptr,
				};
				vkQueueSubmit(m_GpuGraphicsQueue, 1, &dummySubmitInfo, m_GpuInFlightGraphicsFences[i].fence);
				vkQueueSubmit(m_GpuGraphicsQueue, 1, &dummySubmitInfo, m_GpuInFlightTransferFences[i].fence);
				m_GpuInFlightGraphicsFences[i].state = Fence::State::Resettable;
				m_GpuInFlightTransferFences[i].state = Fence::State::Resettable;
			}
			m_CurrentFrame = 0;
			m_SwapchainCreateCallback(this, m_GpuSwapchainExtent, m_FramesInFlight, m_GpuSwapchainImageViews.m_Data);

			m_GraphicsCommandBufferQueue.Lock();
			CommandBuffer<Queue::Graphics>& transitionCommandBuffer = m_GraphicsCommandBufferQueue.New();

			VkCommandBufferAllocateInfo transitionCommandBuffersAllocInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_GpuGraphicsCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			VkAssert(vkAllocateCommandBuffers(m_GpuDevice, &transitionCommandBuffersAllocInfo, &transitionCommandBuffer.m_GpuCommandBuffer),
				"failed to allocate command buffer for swapchain image view layout transition (function vkAllocateCommandBuffers in function CreateSwapchain)!");

			VkCommandBufferBeginInfo transitionBeginInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr,
			};

			VkAssert(vkBeginCommandBuffer(transitionCommandBuffer.m_GpuCommandBuffer, &transitionBeginInfo),
				"failed to begin swapchain image view layout transition command buffer (function vkBeginCommandBuffer in function CreateSwapchain)");

			Stack::Array<VkImageMemoryBarrier> imageMemoryBarriers = m_SingleThreadStack.Allocate<VkImageMemoryBarrier>(m_FramesInFlight);

			if (!imageMemoryBarriers.m_Data) {
				m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
					"single thread stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
			}

			for (size_t i = 0; i < m_FramesInFlight; i++) {
				imageMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarriers[i].pNext = nullptr;
				imageMemoryBarriers[i].srcAccessMask = 0;
				imageMemoryBarriers[i].dstAccessMask = 0;
				imageMemoryBarriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarriers[i].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				imageMemoryBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarriers[i].image = m_GpuSwapchainImages[i];
				imageMemoryBarriers[i].subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				};
			}

			vkCmdPipelineBarrier(transitionCommandBuffer.m_GpuCommandBuffer,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 
				imageMemoryBarriers.m_Size, imageMemoryBarriers.m_Data);

			VkAssert(vkEndCommandBuffer(transitionCommandBuffer.m_GpuCommandBuffer),
				"failed to end swapchain image view layout transition command buffer (function vkEndCommandBuffer in function CreateSwapchain)");

			transitionCommandBuffer.m_Flags = (uint32_t)CommandBufferUsage::Free;

			m_GraphicsCommandBufferQueue.Unlock();

			m_SingleThreadStack.Clear();
		}

		void RecreateSwapchain() {
			vkDestroySwapchainKHR(m_GpuDevice, m_GpuSwapchain, m_GpuAllocationCallbacks);
			LockGuard threadsLockGuard(m_ThreadStackMutex);
			CreateSwapchain();
			for (Thread& thread : m_ThreadList) {
				thread.m_GraphicsCommandBufferFreeList.Reallocate();
				thread.m_TransferCommandBufferFreeList.Reallocate();
			}
			m_GraphicsCommandBufferFreeList.Reallocate();
			m_TransferCommandBufferFreeList.Reallocate();
			m_StagingBufferDestroyList.Reallocate();
		}

		VkDescriptorSetLayout CreateDescriptorSetLayout(uint32_t bindingCount, VkDescriptorSetLayoutBinding* pBindings) const {
			VkDescriptorSetLayoutCreateInfo createInfo {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = bindingCount,
				.pBindings = pBindings,
			};
			VkDescriptorSetLayout res;
			if (!VkCheck(vkCreateDescriptorSetLayout(m_GpuDevice, &createInfo, m_GpuAllocationCallbacks, &res), 
				"failed to create descriptor set layout (function vkCreateDescriptorSetLayout in function CreateDescriptorSetLayout)!")) {
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool AllocateDescriptorSets(const DescriptorPool& descriptorPool, uint32_t setCount, 
			VkDescriptorSetLayout* pLayouts, VkDescriptorSet outSets[]) {
			if (descriptorPool.m_DescriptorPool == VK_NULL_HANDLE) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, "attempting to allocate descriptor sets with a descriptor pool that's null!", VK_SUCCESS);
				return false;
			}
			VkDescriptorSetAllocateInfo allocInfo {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = descriptorPool.m_DescriptorPool,
				.descriptorSetCount = setCount,
				.pSetLayouts = pLayouts,
			};
			VkResult vkRes = vkAllocateDescriptorSets(m_GpuDevice, &allocInfo, outSets);
			if (vkRes == VK_ERROR_OUT_OF_POOL_MEMORY) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, 
					"failed to allocate descriptor sets (function vkAllocateDescriptorSets in function AllocateDescriptorSets) because descriptor pool is out of memory!", 
					vkRes);
				return false;
			}
			else if (!VkCheck(vkRes,"failed to allocate descriptor sets (in function AllocateDescriptorSets)!")) {
				return false;
			}
			return true;
		}

		VkPipelineLayout CreatePipelineLayout(uint32_t setLayoutCount, VkDescriptorSetLayout* pSetLayouts, 
				uint32_t pushConstantRangeCount, VkPushConstantRange* pPushConstantRanges) {
			VkPipelineLayoutCreateInfo createInfo {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.setLayoutCount = setLayoutCount,
				.pSetLayouts = pSetLayouts,
				.pushConstantRangeCount = pushConstantRangeCount,
				.pPushConstantRanges = pPushConstantRanges,
			};
			VkPipelineLayout res;
			if (!VkCheck(vkCreatePipelineLayout(m_GpuDevice, &createInfo, m_GpuAllocationCallbacks, &res), 
				"failed to create pipeline layout (function vkCreatePipelineLayout in function CreatePipelineLayout)!")) {
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool CreateGraphicsPipelines(uint32_t pipelineCount, VkGraphicsPipelineCreateInfo pipelineCreateInfos[], VkPipeline outPipelines[]) {
			if (!VkCheck(vkCreateGraphicsPipelines(m_GpuDevice, VK_NULL_HANDLE, pipelineCount, pipelineCreateInfos, 
					m_GpuAllocationCallbacks, outPipelines), 
					"failed to create graphics pipelines (function vkCreateGraphicsPipelines in function CreateGraphicsPipelines)!")) {
				return false;
			}
			return true;
		}

		bool FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& outIndex) const {
			VkPhysicalDeviceMemoryProperties memProperties{};
			vkGetPhysicalDeviceMemoryProperties(m_Gpu, &memProperties);
			for (size_t i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					outIndex = (uint32_t)i;
					return true;
				}
			}
			return false;
		}

		struct DrawData {
			VkCommandBuffer commandBuffer;
			VkImageView swapchainImageView;
		};

		bool BeginFrame(DrawData& outDrawData) {

			static constexpr uint64_t frame_timeout = 2000000000;
			if (m_GpuSwapchain == VK_NULL_HANDLE || m_GpuSwapchainExtent.width == 0 || m_GpuSwapchainExtent.height == 0) {
				return false;
			}
			if (m_CurrentFrame >= m_FramesInFlight) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, 
					"current frame was larger than frames in flights (in function BeginFrame)", VK_SUCCESS);
				return false;
			}

			VkFence fences[2] { m_GpuInFlightGraphicsFences[m_CurrentFrame].fence, m_GpuInFlightTransferFences[m_CurrentFrame].fence };

			if (m_GpuInFlightGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable &&
				m_GpuInFlightTransferFences[m_CurrentFrame].state == Fence::State::Resettable) {
				if (!VkCheck(vkWaitForFences(m_GpuDevice, 2, fences, VK_TRUE, frame_timeout), 
						"failed to wait for in flight fences (function vkWaitForFences in function BeginFrame)!")) {
					return false;
				}
				vkResetFences(m_GpuDevice, 2, fences);
			}	

			else if (m_GpuInFlightGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable) {
				if (!VkCheck(vkWaitForFences(m_GpuDevice, 1, &fences[0], VK_TRUE, frame_timeout), 
						"failed to wait for in flight fences (function vkWaitForFences in function BeginFrame)!")) {
					return false;
				}
				vkResetFences(m_GpuDevice, 1, &fences[0]);
			}
			else if (m_GpuInFlightGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable) {
				if (!VkCheck(vkWaitForFences(m_GpuDevice, 1, &fences[1], VK_TRUE, frame_timeout), 
						"failed to wait for in flight fences (function vkWaitForFences in function BeginFrame)!")) {
					return false;
				}
				vkResetFences(m_GpuDevice, 1, &fences[1]);
			}

			m_GpuInFlightGraphicsFences[m_CurrentFrame].state = Fence::State::None;
			m_GpuInFlightTransferFences[m_CurrentFrame].state = Fence::State::None;

			m_GraphicsCommandBufferFreeList.Free(m_CurrentFrame);
			m_TransferCommandBufferFreeList.Free(m_CurrentFrame);
			m_ThreadStackMutex.lock();
			for (Thread& thread : m_ThreadList) {
				thread.m_GraphicsCommandBufferFreeList.Free(m_CurrentFrame);
				thread.m_TransferCommandBufferFreeList.Free(m_CurrentFrame);
			}
			m_ThreadStackMutex.unlock();

			m_StagingBufferDestroyList.Destroy(m_CurrentFrame);

			m_TransferCommandBufferQueue.Lock();

			if (m_TransferCommandBufferQueue.m_Size) {

				Stack::Array<VkCommandBuffer> transferCommandBuffers 
					= m_SingleThreadStack.Allocate<VkCommandBuffer>(m_TransferCommandBufferQueue.m_Size);

				if (!transferCommandBuffers.m_Data) {
					m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
						"single thread stack was out of memory (in function EndFrame)!", VK_SUCCESS);
				}

				for (size_t i = 0; i < m_TransferCommandBufferQueue.m_Size; i++) {
					CommandBuffer<Queue::Transfer>& commandBuffer = m_TransferCommandBufferQueue.m_Data[i];
					transferCommandBuffers[i] = commandBuffer.m_GpuCommandBuffer;
					if (commandBuffer.m_Flags & (uint32_t)CommandBufferUsage::Free) {
						if (commandBuffer.m_Thread) {
							commandBuffer.m_Thread->m_TransferCommandBufferFreeList.Push(commandBuffer.m_GpuCommandBuffer, m_CurrentFrame);
						}
						else {
							m_TransferCommandBufferFreeList.Push(commandBuffer.m_GpuCommandBuffer, m_CurrentFrame);
						}
					}
					if (commandBuffer.m_Flags & (uint32_t)CommandBufferUsage::DestroyStagingBuffer) {
						m_StagingBufferDestroyList.Push(m_CurrentFrame, commandBuffer.m_GpuBuffer, commandBuffer.m_GpuDeviceMemory);
					}
				}

				VkSubmitInfo transferSubmit {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = 0,
					.commandBufferCount = (uint32_t)transferCommandBuffers.m_Size,
					.pCommandBuffers = transferCommandBuffers.m_Data,
				};

				if (VkCheck(vkQueueSubmit(m_GpuTransferQueue, 1, &transferSubmit, m_GpuInFlightTransferFences[m_CurrentFrame].fence), 
						"failed to submit to transfer queue (function vkQueueSubmit in function EndFrame)!")) {
					m_GpuInFlightTransferFences[m_CurrentFrame].state = Fence::State::Resettable;
				}

				m_TransferCommandBufferQueue.Clear();
				m_SingleThreadStack.Clear();
			}

			m_TransferCommandBufferQueue.Unlock();

			uint32_t imageIndex;
			VkResult result = vkAcquireNextImageKHR(m_GpuDevice, m_GpuSwapchain, frame_timeout, 
					m_GpuRenderWaitSemaphores[m_CurrentFrame], VK_NULL_HANDLE , &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				RecreateSwapchain();
				return false;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, "failed to acquire next swapchain image (in function BeginFrame)!", result);
				return false;
			}
			if (imageIndex != m_CurrentFrame) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "image index didn't match current frame (in function BeginFrame)!", VK_SUCCESS);
				return false;
			}
			outDrawData.swapchainImageView = m_GpuSwapchainImageViews[m_CurrentFrame];
			outDrawData.commandBuffer = m_GpuRenderCommandBuffers[m_CurrentFrame];
			vkResetCommandBuffer(outDrawData.commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr,
			};
			if (!VkCheck(vkBeginCommandBuffer(outDrawData.commandBuffer, &commandBufferBeginInfo), 
				"failed to begin render command buffer (function vkBeginCommandBuffer in function BeginFrame)")) {
				return false;
			}
			return true;
		}

		void EndFrame() {

			vkEndCommandBuffer(m_GpuRenderCommandBuffers[m_CurrentFrame]);

			Stack::Array<VkCommandBuffer> graphicsCommandBuffers { 0, nullptr };

			m_GraphicsCommandBufferQueue.Lock();

			if (m_GraphicsCommandBufferQueue.m_Size) {

				m_SingleThreadStack.Allocate<VkCommandBuffer>(m_GraphicsCommandBufferQueue.m_Size, &graphicsCommandBuffers);

				if (!graphicsCommandBuffers.m_Data) {
					m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
						"single thread stack was out of memory (in function EndFrame)!", VK_SUCCESS);
				}

				for (size_t i = 0; i < m_GraphicsCommandBufferQueue.m_Size; i++) {
					CommandBuffer<Queue::Graphics>& commandBuffer = m_GraphicsCommandBufferQueue.m_Data[i];
					graphicsCommandBuffers[i] = commandBuffer.m_GpuCommandBuffer;
					if (commandBuffer.m_Flags & (uint32_t)CommandBufferUsage::Free) {
						if (commandBuffer.m_Thread) {
							commandBuffer.m_Thread->m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_GpuCommandBuffer, m_CurrentFrame);
						}
						else {
							m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_GpuCommandBuffer, m_CurrentFrame);
						}
					}
				}
				m_GraphicsCommandBufferQueue.Clear();
			}


			m_GraphicsCommandBufferQueue.Unlock();

			VkPipelineStageFlags graphicsWaitStages[1] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkSubmitInfo graphicsSubmits[2] {
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &m_GpuRenderWaitSemaphores[m_CurrentFrame],
					.pWaitDstStageMask = graphicsWaitStages,
					.commandBufferCount = 1,
					.pCommandBuffers = &m_GpuRenderCommandBuffers[m_CurrentFrame],
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &m_GpuRenderFinishedSemaphores[m_CurrentFrame],
				},
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = (uint32_t)graphicsCommandBuffers.m_Size,
					.pCommandBuffers = graphicsCommandBuffers.m_Data,
				}
			};

			m_SingleThreadStack.Clear();

			if (!VkCheck(vkQueueSubmit(m_GpuGraphicsQueue, graphicsCommandBuffers.m_Size ? 2 : 1, graphicsSubmits, 
					m_GpuInFlightGraphicsFences[m_CurrentFrame].fence), 
					"failed to submit to graphics queue (function vkQueueSubmit in function EndFrame)!")) {
				return;
			}

			m_GpuInFlightGraphicsFences[m_CurrentFrame].state = Fence::State::Resettable;	

			VkPresentInfoKHR presentInfo {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = nullptr,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &m_GpuRenderFinishedSemaphores[m_CurrentFrame],
				.swapchainCount = 1,
				.pSwapchains = &m_GpuSwapchain,
				.pImageIndices = &m_CurrentFrame,
				.pResults = nullptr,
			};

			VkResult vkRes = vkQueuePresentKHR(m_GpuGraphicsQueue, &presentInfo);

			m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;

			if (vkRes == VK_ERROR_OUT_OF_DATE_KHR || vkRes == VK_SUBOPTIMAL_KHR) {
				RecreateSwapchain();
			}
			else {
				VkCheck(vkRes, "failed to present image (function vkQueuePresentKHR in function EndFrame)!");
			}
		}	
	};
}
