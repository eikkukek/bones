#pragma once

#include "math.hpp"
#include "vulkan/vulkan.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glslang_c_interface.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "fmt/printf.h"
#include "fmt/color.h"
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>
#include <mutex>
#include <utility>

namespace engine {

#ifndef BOOL_RENDERER_DEBUG 
#define BOOL_RENDERER_DEBUG true 
#endif

	static_assert(sizeof(size_t) >= 4, "size of size_t isn't big enough!");

	inline constexpr uint32_t PackColorRBGA(const Vec4& color) {
		return ((uint32_t)(color.w * 255) << 24) 
			+ ((uint32_t)(color.z * 255) << 16)
			+ ((uint32_t)(color.y * 255) << 8)
			+ (uint32_t)(color.x * 255);
	}

	class Renderer {
	public:

		typedef void (*SwapchainCreateCallback)(const Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews);
		typedef std::lock_guard<std::mutex> LockGuard;
		typedef uint32_t Bool32;

		enum class ErrorOrigin {
			Uncategorized = 0,
			InitializationFailed = 1,
			Vulkan = 2,
			OutOfMemory = 3,
			NullDereference = 4,
			IndexOutOfBounds = 5,
			Shader = 6,
			Buffer = 7,
			Image = 8,
			Threading = 9,
			MaxEnum,
		};

		static const char* ErrorOriginString(ErrorOrigin origin) {
			static constexpr const char* strings[(size_t)ErrorOrigin::MaxEnum] {
				"Uncategorized",
				"InitializationFailed",
				"Vulkan",
				"OutOfMemory",
				"NullDereference",
				"IndexOutOfBounds",
				"Shader",
				"Buffer",
				"Image",
				"Threading",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		typedef void (*CriticalErrorCallback)(const Renderer* renderer, ErrorOrigin origin, const char* err, VkResult vkErr);

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
						PrintError(ErrorOrigin::IndexOutOfBounds, "index out of bounds (engine::Renderer::Stack::Array::operator[])!");
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
					PrintError(ErrorOrigin::OutOfMemory, "stack out of memory (function engine::Renderer::Stack::Allocate)!");
					return nullptr;
				}
				return new(res) T(std::forward<Args>(args)...);
			}

			template<typename T>
			Array<T> Allocate(size_t count) {
				T* res = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintError(ErrorOrigin::OutOfMemory, "stack out of memory (function engine::Renderer::Stack::Allocate)!");
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
					PrintError(ErrorOrigin::OutOfMemory, "stack out of memory (function engine::Renderer::Stack::Allocate)!");
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
					PrintError(ErrorOrigin::NullDereference, "pointer passed to stack was null (function Stack::Allocate)!");
					return false;
				}
				*out = m_Data + m_UsedSize;
				if ((m_UsedSize += size) > m_MaxSize) {
					PrintError(ErrorOrigin::OutOfMemory, "stack out of memory (function Stack::Allocate)!");
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

		template<typename T, uint32_t max_count_T>
		struct OneTypeStack {

			typedef T* Iterator;
			typedef T* const ConstItererator;

			T* const m_Data;
			uint32_t m_Count;

			OneTypeStack() noexcept : m_Count(0), m_Data((T*)malloc(max_count_T * sizeof(T))) {}

			~OneTypeStack() {
				free(m_Data);
			}

			template<typename... Args>
			T* New(Args&&... args) {
				if (m_Count >= max_count_T) {
					PrintError(ErrorOrigin::OutOfMemory, "one type stack was out of memory (function OneTypeStack::New)!");
					return nullptr;
				}
				return new(&m_Data[m_Count++]) T(std::forward<Args>(args)...);
			}

			void Clear() {
				m_Count = 0;
			}

			Iterator begin() const {
				return m_Data;
			}

			ConstItererator end() const {
				return &m_Data[m_Count];
			}
		};

		struct DescriptorPool {

			const Renderer& m_Renderer;
			const uint32_t m_MaxSets;
			VkDescriptorPool m_DescriptorPool;

			DescriptorPool(Renderer& renderer, uint32_t maxSets) noexcept 
				: m_Renderer(renderer), m_MaxSets(maxSets), m_DescriptorPool(VK_NULL_HANDLE) {}

			~DescriptorPool() {
				Terminate();
			}

			void Terminate() {
				if (m_DescriptorPool != VK_NULL_HANDLE) {
					vkDestroyDescriptorPool(m_Renderer.m_VulkanDevice, m_DescriptorPool, m_Renderer.m_VulkanAllocationCallbacks);
					m_DescriptorPool = VK_NULL_HANDLE;
				}
			}

			bool Create(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes) {
				if (m_DescriptorPool != VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::Uncategorized, 
						"attempting to create descriptor pool (in function DescriptorPool::CreatePool) that has already been created!");
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
				VkResult vkRes = vkCreateDescriptorPool(m_Renderer.m_VulkanDevice, &createInfo, 
					m_Renderer.m_VulkanAllocationCallbacks, &m_DescriptorPool);
				if (vkRes != VK_SUCCESS) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to create descriptor pool (function vkCreateDescriptorPool in function DescriptorPool::CreatePool)!",
						vkRes);
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

			static void PrintShaderMessage(const char* log, const char* dLog) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, "{}\n{}", log, dLog);
			}

			bool NotCompiled() const {
				return !m_GlslangShader || !m_GlslangProgram;
			}

			bool Compile(const char* shaderCode, VkShaderStageFlagBits shaderStage) {

				const glslang_resource_t resource {
					.max_draw_buffers = (int)m_Renderer.m_MaxFragmentOutPutAttachments,
					.limits {
						.general_uniform_indexing = true,
						.general_attribute_matrix_vector_indexing = true,
						.general_varying_indexing = true,
						.general_sampler_indexing = true,
						.general_variable_indexing = true,
						.general_constant_matrix_vector_indexing = true,
					},
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
					PrintError(ErrorOrigin::Shader, "failed to initialize glslang process (in Shader constructor)!");
					return false;
				}

				m_GlslangShader = glslang_shader_create(&input);

				if (!glslang_shader_preprocess(m_GlslangShader, &input)) {
					const char* log = glslang_shader_get_info_log(m_GlslangShader);
					const char* dLog = glslang_shader_get_info_debug_log(m_GlslangShader);
					PrintShaderMessage(log, dLog);
					glslang_shader_delete(m_GlslangShader);
					m_GlslangShader = nullptr;
					return false;
				}

				if (!glslang_shader_parse(m_GlslangShader, &input)) {
					const char* log = glslang_shader_get_info_log(m_GlslangShader);
					const char* dLog = glslang_shader_get_info_debug_log(m_GlslangShader);
					PrintShaderMessage(log, dLog);
					glslang_shader_delete(m_GlslangShader);
					m_GlslangShader = nullptr;
					return false;
				}

				m_GlslangProgram = glslang_program_create();
				glslang_program_add_shader(m_GlslangProgram, m_GlslangShader);

				if (!glslang_program_link(m_GlslangProgram, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
					const char* log = glslang_program_get_info_log(m_GlslangProgram);
					const char* dLog = glslang_program_get_info_debug_log(m_GlslangProgram);
					PrintShaderMessage(log, dLog);
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
				return true;
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
					PrintError(ErrorOrigin::Shader, 
						"attempting to create shader module with a shader (in function CreateShaderModule) with a shader that hasn't been compiled");
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
				if (!m_Renderer.VkCheck(vkCreateShaderModule(m_Renderer.m_VulkanDevice, &createInfo, m_Renderer.m_VulkanAllocationCallbacks, &res), 
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
			VkCommandPool m_CommandPool;
			uint32_t m_FramesInFlight; 
			VkCommandBuffer(*m_Data)[max_command_buffers_per_frame];
			uint32_t* m_Counts;

			CommandBufferFreeList(Renderer& renderer)
				: m_Renderer(renderer), m_CommandPool(VK_NULL_HANDLE), m_FramesInFlight(0), m_Data(nullptr), m_Counts(nullptr) {}

			~CommandBufferFreeList() {
				free(m_Data);
				delete[] m_Counts;
			}

			void Initialize(VkCommandPool commandPool) {
				if (m_FramesInFlight != 0) {
					PrintError(ErrorOrigin::InitializationFailed, 
						"attempting to initialize command buffer free list more than once (function CommandBufferFreeList::Initialize)!");
					return;
				}
				m_CommandPool = commandPool;
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

			bool Push(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
				if (currentFrame >= m_FramesInFlight) {
					PrintError(ErrorOrigin::IndexOutOfBounds, 
						"current frame goes out of bounds of command buffer free list frames in flight (function CommandBufferFreeList::Push)!");
					return false;
				}
				uint32_t& count = m_Counts[currentFrame];
				if (count >= max_command_buffers_per_frame) {
					PrintError(ErrorOrigin::OutOfMemory, "command buffer free list was out of memory (function CommandBufferFreeList::Push)!");
					return false;
				}
				m_Data[currentFrame][count++] = commandBuffer;
				return true;
			}

			void Free(uint32_t currentFrame) {
				if (currentFrame >= m_FramesInFlight) {
					PrintError(ErrorOrigin::IndexOutOfBounds, 
						"given frame goes out of bounds of command buffer free list frames in flight (function CommandBufferFreeList::Free)!");
					return;
				}
				uint32_t& count = m_Counts[currentFrame];
				if (count) {
					vkFreeCommandBuffers(m_Renderer.m_VulkanDevice, m_CommandPool, 
						count, m_Data[currentFrame]);
					count = 0;
				}
			}

			void FreeAll() {
				for (uint32_t i = 0; i < m_FramesInFlight; i++) {
					uint32_t& count = m_Counts[i];
					if (count) {
						vkFreeCommandBuffers(m_Renderer.m_VulkanDevice, m_CommandPool, 
							count, m_Data[i]);
						count = 0;
					}
				}
			}
		};

		struct Thread {

			enum class State {
				Inactive = 0,
				Active = 1,
			};

			struct Guard {
				Thread& m_Thread;
				Guard(Thread& thread) noexcept : m_Thread(thread) {
					if (thread.m_GraphicsCommandPool == VK_NULL_HANDLE) {
						thread.m_State = State::Inactive;
						return;
					}
					m_Thread.m_State = State::Active;
				}
				~Guard() noexcept {
					m_Thread.m_State = State::Inactive;
				}
			};

			static constexpr size_t in_flight_render_stack_size = sizeof(VkCommandPool) * 5 * 5;

			Renderer& m_Renderer;
			const std::thread::id m_ThreadID;
			std::atomic<State> m_State;
			VkCommandPool m_GraphicsCommandPool;
			VkCommandPool m_TransferCommandPool;
			CommandBufferFreeList m_TransferCommandBufferFreeList;
			CommandBufferFreeList m_GraphicsCommandBufferFreeList;

			Thread(Renderer& renderer, std::thread::id threadID) noexcept : m_Renderer(renderer), m_ThreadID(threadID),
				m_State(State::Inactive),
				m_GraphicsCommandPool(VK_NULL_HANDLE), m_TransferCommandPool(VK_NULL_HANDLE), 
				m_GraphicsCommandBufferFreeList(m_Renderer), m_TransferCommandBufferFreeList(m_Renderer) {
				VkCommandPoolCreateInfo graphicsPoolInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.queueFamilyIndex = m_Renderer.m_GraphicsQueueFamilyIndex,
				};
				m_Renderer.VkAssert(vkCreateCommandPool(m_Renderer.m_VulkanDevice, 
					&graphicsPoolInfo, m_Renderer.m_VulkanAllocationCallbacks, &m_GraphicsCommandPool),
					"failed to create graphics command pool for thread (function vkCreateCommandPool in Thread constructor)!");
				VkCommandPoolCreateInfo transferPoolInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.queueFamilyIndex = m_Renderer.m_TransferQueueFamilyIndex,
				};
				m_Renderer.VkAssert(vkCreateCommandPool(m_Renderer.m_VulkanDevice, 
					&transferPoolInfo, m_Renderer.m_VulkanAllocationCallbacks, &m_TransferCommandPool),
					"failed to create transfer command pool for thread (function vkCreateCommandPool in Thread constructor)!");
				m_TransferCommandBufferFreeList.Initialize(m_TransferCommandPool);
				m_GraphicsCommandBufferFreeList.Initialize(m_GraphicsCommandPool);
			}

			void Terminate() {
				m_GraphicsCommandBufferFreeList.FreeAll();
				vkDestroyCommandPool(m_Renderer.m_VulkanDevice, m_GraphicsCommandPool, m_Renderer.m_VulkanAllocationCallbacks);
				m_GraphicsCommandPool = VK_NULL_HANDLE;
				m_TransferCommandBufferFreeList.FreeAll();
				vkDestroyCommandPool(m_Renderer.m_VulkanDevice, m_TransferCommandPool, m_Renderer.m_VulkanAllocationCallbacks);
				m_TransferCommandPool = VK_NULL_HANDLE;
			}
		};

		typedef uint32_t CommandBufferFlags;

		struct CommandBufferSubmitCallback {

			struct BufferData {
				VkBuffer m_Buffer{};
				VkDeviceMemory m_VulkanDeviceMemory{};
			};

			union Data {
				BufferData u_BufferData{};
			};

			void (*m_Callback)(const Renderer& renderer, const CommandBufferSubmitCallback&){};
			Data m_Data{};

			void Callback(const Renderer& renderer) const {
				if (m_Callback) {
					m_Callback(renderer, *this);
				}
			}
		};

		enum CommandBufferFlag {
			CommandBufferFlag_SubmitCallback = 1,
			CommandBufferFlag_FreeAfterSubmit = 2,
		};

		template<Queue queue_T>
		struct CommandBuffer {
			CommandBuffer(std::thread::id threadID = std::this_thread::get_id()) : m_ThreadID(threadID) {}
			const std::thread::id m_ThreadID;
			CommandBufferFlags m_Flags{};
			VkCommandBuffer m_CommandBuffer{};
			CommandBufferSubmitCallback m_SubmitCallback{};
		};

		struct Buffer {

			Renderer& m_Renderer;
			VkBuffer m_Buffer;
			VkDeviceMemory m_VulkanDeviceMemory;
			VkDeviceSize m_BufferSize;	

			Buffer(Renderer& renderer) noexcept 
				: m_Renderer(renderer), m_Buffer(VK_NULL_HANDLE), m_VulkanDeviceMemory(VK_NULL_HANDLE), m_BufferSize(0) {}

			Buffer(const Buffer&) = delete;

			Buffer(Buffer&& other) noexcept
				: m_Renderer(other.m_Renderer), m_Buffer(other.m_Buffer), m_VulkanDeviceMemory(other.m_VulkanDeviceMemory),
					m_BufferSize(other.m_BufferSize) {
				other.m_Buffer = VK_NULL_HANDLE;
				other.m_VulkanDeviceMemory = VK_NULL_HANDLE;
				other.m_BufferSize = 0;
			}

			~Buffer() {
				Terminate();
			}

			void Terminate() {
				if (m_Renderer.m_VulkanDevice == VK_NULL_HANDLE) {
					return;
				}
				vkFreeMemory(m_Renderer.m_VulkanDevice, m_VulkanDeviceMemory, m_Renderer.m_VulkanAllocationCallbacks);
				m_VulkanDeviceMemory = VK_NULL_HANDLE;
				vkDestroyBuffer(m_Renderer.m_VulkanDevice, m_Buffer, m_Renderer.m_VulkanAllocationCallbacks);
				m_Buffer = VK_NULL_HANDLE;
				m_BufferSize = 0;
			}

			bool Create(VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, 
				VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queueFamilyIndexCount = 0, 
				const uint32_t* pQueueFamilyIndices = nullptr) {
				if (m_Buffer != VK_NULL_HANDLE || m_VulkanDeviceMemory != VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::Uncategorized, 
						"attempting to create buffer (in function Buffer::Create) when the buffer has already been created!");
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
				if (!m_Renderer.VkCheck(vkCreateBuffer(m_Renderer.m_VulkanDevice, &bufferInfo, 
						m_Renderer.m_VulkanAllocationCallbacks, &m_Buffer),
						"failed to create buffer (function vkCreateBuffer in function Buffer::Create)!")) {
					m_Buffer = VK_NULL_HANDLE;
					return false;
				}
				VkMemoryRequirements memRequirements{};
				vkGetBufferMemoryRequirements(m_Renderer.m_VulkanDevice, m_Buffer, &memRequirements);
				VkMemoryAllocateInfo allocInfo {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.pNext = nullptr,
					.allocationSize = memRequirements.size,
				};
				if (!m_Renderer.FindMemoryTypeIndex(memRequirements.memoryTypeBits, bufferProperties, allocInfo.memoryTypeIndex)) {
					PrintError(ErrorOrigin::Vulkan, 
						"failed to find memory type index when creating buffer (function FindMemoryTypeIndex in function Buffer::Create)!");
					Terminate();
					return false;
				}
				if (!m_Renderer.VkCheck(vkAllocateMemory(m_Renderer.m_VulkanDevice, &allocInfo, 
						m_Renderer.m_VulkanAllocationCallbacks, &m_VulkanDeviceMemory),
						"failed to allocate memory for buffer (function vkAllocateMemory in function Buffer::Create)!")) {
					Terminate();
					return false;
				}
				if (!m_Renderer.VkCheck(vkBindBufferMemory(m_Renderer.m_VulkanDevice, m_Buffer, m_VulkanDeviceMemory, 0),
						"failed to bind buffer memory (function vkBindBufferMemory in function Buffer::Create)!")) {
					Terminate();
					return false;
				}
				m_BufferSize = size;
				return true;
			}

			bool CreateWithData(VkDeviceSize size, const void* data,
				VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties,
				VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queueFamilyIndexCount = 0, 
				const uint32_t* pQueueFamilyIndices = nullptr) {

				Buffer stagingBuffer(m_Renderer);
				if (!stagingBuffer.Create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					PrintError(ErrorOrigin::Buffer, 
						"failed to create buffer (function Buffer::Create in function Buffer::CreateWithData)!");
				}

				void* stagingMap;
				vkMapMemory(m_Renderer.m_VulkanDevice, stagingBuffer.m_VulkanDeviceMemory, 0, size, 0, &stagingMap);
				memcpy(stagingMap, data, size);
				vkUnmapMemory(m_Renderer.m_VulkanDevice, stagingBuffer.m_VulkanDeviceMemory);

				if (!Create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage, 
						bufferProperties, sharingMode, queueFamilyIndexCount, pQueueFamilyIndices)) {
					PrintError(ErrorOrigin::Buffer, 
						"failed to create buffer (function Buffer::Create in function Buffer::CreateWithData)!");
					return false;
				}

				VkCommandBufferAllocateInfo commandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = m_Renderer.GetCommandPool<Queue::Transfer>(),
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};

				LockGuard lockGuard(m_Renderer.m_TransferCommandBufferQueueMutex);

				CommandBuffer<Queue::Transfer>* commandBuffer 
					= m_Renderer.m_TransferCommandBufferQueue.New(std::this_thread::get_id());
				if (!commandBuffer) {
					PrintError(ErrorOrigin::OutOfMemory, 
						"transfer command buffer queue was out of memory (function OneTypeStack::New in function Buffer::CopyBuffer)!");
					Terminate();
					return false;
				}
				if (!m_Renderer.VkCheck(vkAllocateCommandBuffers(m_Renderer.m_VulkanDevice, 
						&commandBufferAllocInfo, &commandBuffer->m_CommandBuffer),
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
				if (!m_Renderer.VkCheck(vkBeginCommandBuffer(commandBuffer->m_CommandBuffer, &commandBufferBeginInfo), 
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
				vkCmdCopyBuffer(commandBuffer->m_CommandBuffer, stagingBuffer.m_Buffer, m_Buffer, 1, &copyRegion);
				if (!m_Renderer.VkCheck(vkEndCommandBuffer(commandBuffer->m_CommandBuffer),
						"failed to end command buffer (function vkEndCommandBuffer in function Buffer::CreateWithData)")) {
					return false;
				}
				commandBuffer->m_Flags = CommandBufferFlag_FreeAfterSubmit | CommandBufferFlag_SubmitCallback;
				commandBuffer->m_SubmitCallback= {
					.m_Callback = [](const Renderer& renderer, const CommandBufferSubmitCallback& callback) {
						vkDestroyBuffer(renderer.m_VulkanDevice, callback.m_Data.u_BufferData.m_Buffer, 
							renderer.m_VulkanAllocationCallbacks);
						vkFreeMemory(renderer.m_VulkanDevice, callback.m_Data.u_BufferData.m_VulkanDeviceMemory, 
							renderer.m_VulkanAllocationCallbacks);
					},
					.m_Data {
						.u_BufferData {
							.m_Buffer = stagingBuffer.m_Buffer,
							.m_VulkanDeviceMemory = stagingBuffer.m_VulkanDeviceMemory,
						}
					}
				};
				stagingBuffer.m_Buffer = VK_NULL_HANDLE;
				stagingBuffer.m_VulkanDeviceMemory = VK_NULL_HANDLE;
				return true;
			}

			bool CopyBuffer(Buffer& dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size) const {
				if (m_Buffer == VK_NULL_HANDLE || dst.m_Buffer == VK_NULL_HANDLE) {
					PrintError(ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + srcOffset is larger than source size (in function Buffer::CopyBuffer)!");
				}
				if (m_BufferSize < size + srcOffset) {
					PrintError(ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + srcOffset is larger than source size (in function Buffer::CopyBuffer)!");
					return false;
				}
				if (dst.m_BufferSize < size + dstOffset) {
					PrintError(ErrorOrigin::Buffer, 
						"attempting to copy buffer when the size + dstOffset is larger than destination size (in function Buffer::CopyBuffer)!");
					return false;
				}
				LockGuard lockGuard(m_Renderer.m_TransferCommandBufferQueueMutex);
				CommandBuffer<Queue::Transfer>* commandBuffer = m_Renderer.m_TransferCommandBufferQueue.New();
				if (!commandBuffer) {
					PrintError(ErrorOrigin::OutOfMemory, 
						"transfer command buffer queue was out of memory (function OneTypeStack::New in function Buffer::CopyBuffer)!");
					return false;
				}
				VkCommandBufferAllocateInfo commandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = m_Renderer.GetCommandPool<Queue::Transfer>(),
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1,
				};
				if (!m_Renderer.VkCheck(vkAllocateCommandBuffers(m_Renderer.m_VulkanDevice, &commandBufferAllocInfo, 
						&commandBuffer->m_CommandBuffer), 
						"failed to allocate command buffer (function vkAllocateCommandBuffers in function Buffer::CopyBuffer)!")) {
					return false;
				}
				VkCommandBufferBeginInfo commandBufferBeginInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext = nullptr,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
					.pInheritanceInfo = nullptr,
				};
				if (!m_Renderer.VkCheck(vkBeginCommandBuffer(commandBuffer->m_CommandBuffer, &commandBufferBeginInfo), 
						"failed to begin command buffer (function vkBeginCommandBuffer in function Buffer::CopyBuffer)!")) {
					return false;
				}
				VkBufferCopy copyRegion {
					.srcOffset = srcOffset,
					.dstOffset = dstOffset,
					.size = size,
				};
				vkCmdCopyBuffer(commandBuffer->m_CommandBuffer, m_Buffer, dst.m_Buffer, 1, &copyRegion);
				if (!m_Renderer.VkCheck(vkEndCommandBuffer(commandBuffer->m_CommandBuffer),
						"failed to end command buffer (function vkEndCommandBuffer in function Buffer::CopyBuffer)")) {
					return false;
				}
				return true;
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
		static constexpr size_t max_thread_count = 256;
		static constexpr size_t max_pending_graphics_command_buffer_count = 250000;
		static constexpr size_t max_pending_transfer_command_buffer_count = 250000;
		static constexpr size_t max_command_buffer_submit_callbacks 
			= max_pending_graphics_command_buffer_count + max_pending_transfer_command_buffer_count;
		static constexpr size_t in_flight_render_stack_size = 1024;

		static constexpr size_t max_model_descriptor_sets = 250000;	

		static void PrintMessage(const char* msg) {
			fmt::print(fmt::emphasis::bold, "Renderer message: {}\n", msg);
		}

		static void PrintWarning(const char* warn) {
			fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold, "Renderer warning: %s\n", warn);
		}

		static void PrintError(ErrorOrigin origin, const char* err, VkResult vkErr = VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
				"Renderer called an error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
			if (vkErr != VK_SUCCESS) {
				fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
					"Vulkan error code: {}\n", (int)vkErr); 
			}
		}

		Stack m_SingleThreadStack;
		Stack m_InFlightRenderStack;

		VkDevice m_VulkanDevice = VK_NULL_HANDLE;
		VkAllocationCallbacks* m_VulkanAllocationCallbacks = nullptr;
		VkPhysicalDevice m_Gpu = VK_NULL_HANDLE;
		const std::thread::id m_MainThreadID;
		VkCommandPool m_GraphicsCommandPool = VK_NULL_HANDLE;
		VkCommandPool m_TransferCommandPool = VK_NULL_HANDLE;
		OneTypeStack<Thread, max_thread_count> m_Threads;
		std::mutex m_ThreadsMutex;

		OneTypeStack<CommandBuffer<Queue::Graphics>, max_pending_graphics_command_buffer_count / 2> m_GraphicsCommandBufferQueue;
		std::mutex m_GraphicsCommandBufferQueueMutex{};
		OneTypeStack<CommandBuffer<Queue::Graphics>, max_pending_graphics_command_buffer_count / 2> m_EarlyGraphicsCommandBufferQueue;
		std::mutex m_EarlyGraphicsCommandBufferQueueMutex{};
		OneTypeStack<CommandBuffer<Queue::Transfer>, max_pending_graphics_command_buffer_count> m_TransferCommandBufferQueue;
		std::mutex m_TransferCommandBufferQueueMutex{};
		Stack::Array<OneTypeStack<CommandBufferSubmitCallback, max_command_buffer_submit_callbacks>> 
			m_CommandBufferSubmitCallbacks { 0, nullptr };

		CommandBufferFreeList m_TransferCommandBufferFreeList;
		CommandBufferFreeList m_GraphicsCommandBufferFreeList;

		VkSampleCountFlags m_ColorMsaaSamples = 1;
		VkSampleCountFlags m_DepthMsaaSamples = 1;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		uint32_t m_MaxFragmentOutPutAttachments;

		VkDescriptorSetLayout m_ModelDescriptorSetLayout;
		DescriptorPool m_ModelDescriptorPool { *this, max_model_descriptor_sets };

		Stack::Array<Fence> m_InFlightEarlyGraphicsFences { 0, nullptr };
		Stack::Array<Fence> m_InFlightTransferFences { 0, nullptr };
		Stack::Array<Fence> m_InFlightGraphicsFences { 0, nullptr };
		Stack::Array<VkSemaphore> m_EarlyGraphicsSignalSemaphores { 0, nullptr };
		Stack::Array<VkCommandBuffer> m_RenderCommandBuffers { 0, nullptr };
		Stack::Array<VkSemaphore> m_RenderFinishedSemaphores { 0, nullptr };
		Stack::Array<VkSemaphore> m_RenderWaitSemaphores { 0, nullptr };

		Stack::Array<VkImageView> m_SwapchainImageViews { 0, nullptr };
		std::mutex m_GraphicsQueueMutex{};
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		std::mutex m_TransferQueueMutex{};
		VkQueue m_TransferQueue = VK_NULL_HANDLE;

		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkExtent2D m_SwapchainExtent = { 0, 0 };
		uint32_t m_FramesInFlight = 0;
		uint32_t m_CurrentFrame = 0;

		uint32_t m_GraphicsQueueFamilyIndex = 0;
		uint32_t m_TransferQueueFamilyIndex = 0;
		uint32_t m_PresentQueueFamilyIndex = 0;

		ImGuiContext* m_ImGuiContext = nullptr;
		VkDescriptorPool m_ImGuiGpuDecriptorPool = VK_NULL_HANDLE;

		GLFWwindow* m_Window = nullptr;
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		Stack::Array<VkImage> m_SwapchainImages { 0, 0 };
		VkSurfaceFormatKHR m_SwapchainSurfaceFormat{};
		VkPresentModeKHR m_PresentMode{};
		SwapchainCreateCallback m_SwapchainCreateCallback;

		VkInstance m_VulkanInstance = VK_NULL_HANDLE;

		const CriticalErrorCallback m_CriticalErrorCallback;

		void Assert(bool expression, ErrorOrigin origin, const char* err) const {
			if (!expression) {
				m_CriticalErrorCallback(this, origin, err, VK_SUCCESS);
			}
		}

		void VkAssert(VkResult result, const char* err) const {
			if (result != VK_SUCCESS) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, err, result);
			}
		}

		bool VkCheck(VkResult result, const char* err) const {
			if (result != VK_SUCCESS) {
				PrintError(ErrorOrigin::Vulkan, err, result);
				return false;
			}
			return true;
		}

		Renderer(const char* appName, uint32_t appVersion, GLFWwindow* window, bool includeImGui, 
			CriticalErrorCallback criticalErrorCallback, SwapchainCreateCallback swapchainCreateCallback)
			: m_SingleThreadStack(single_thread_stack_size, (uint8_t*)malloc(single_thread_stack_size)), 
				m_InFlightRenderStack(in_flight_render_stack_size, (uint8_t*)malloc(in_flight_render_stack_size)), m_Window(window),
				m_GraphicsCommandBufferQueue(),
				m_TransferCommandBufferQueue(),
				m_GraphicsCommandBufferFreeList(*this),
				m_TransferCommandBufferFreeList(*this),
				m_MainThreadID(std::this_thread::get_id()),
				m_CriticalErrorCallback(criticalErrorCallback), m_SwapchainCreateCallback(swapchainCreateCallback) {

			assert(m_CriticalErrorCallback && "critical error callback was null (renderer)!");
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

			VkAssert(vkCreateInstance(&instanceCreateInfo, m_VulkanAllocationCallbacks, &m_VulkanInstance), 
					"failed to create vulkan instance (function vkCreateInstance in Renderer constructor)!");

			VkAssert(glfwCreateWindowSurface(m_VulkanInstance, m_Window, m_VulkanAllocationCallbacks, &m_Surface), 
					"failed to create window surface (function glfwCreateWindowSurface in Renderer constructor)!");

			uint32_t gpuCount;
			vkEnumeratePhysicalDevices(m_VulkanInstance, &gpuCount, nullptr);
			Stack::Array<VkPhysicalDevice> gpus = m_SingleThreadStack.Allocate<VkPhysicalDevice>(gpuCount);
			vkEnumeratePhysicalDevices(m_VulkanInstance, &gpuCount, gpus.m_Data);

			int bestGpuScore = 0;
			VkPhysicalDevice bestGpu = VK_NULL_HANDLE;
			uint32_t bestGpuQueueFamilyIndices[3];
			VkSampleCountFlags bestGpuColorSamples = 1;
			VkSampleCountFlags bestGpuDepthSamples = 1;

			for (VkPhysicalDevice gpu : gpus) {
				uint32_t surfaceFormatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_Surface, &surfaceFormatCount, nullptr);
				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_Surface, &presentModeCount, nullptr);
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
						vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queueFamilyIndex, m_Surface, &presentSupported);
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
					m_MaxFragmentOutPutAttachments = properties.limits.maxFragmentOutputAttachments;
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
			m_ColorMsaaSamples = bestGpuColorSamples;
			m_DepthMsaaSamples = bestGpuDepthSamples;
			m_GraphicsQueueFamilyIndex = bestGpuQueueFamilyIndices[0];
			m_TransferQueueFamilyIndex = bestGpuQueueFamilyIndices[1];
			m_PresentQueueFamilyIndex = bestGpuQueueFamilyIndices[2];

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

			VkAssert(vkCreateDevice(m_Gpu, &deviceInfo, m_VulkanAllocationCallbacks, &m_VulkanDevice), 
					"failed to create vulkan device (function vkCreateDevice in Renderer constructor)!");

			vkGetDeviceQueue(m_VulkanDevice, m_GraphicsQueueFamilyIndex, 0, &m_GraphicsQueue);
			vkGetDeviceQueue(m_VulkanDevice, m_TransferQueueFamilyIndex, 0, &m_TransferQueue);
			vkGetDeviceQueue(m_VulkanDevice, m_PresentQueueFamilyIndex, 0, &m_PresentQueue);	

			m_SingleThreadStack.Clear();

			VkCommandPoolCreateInfo graphicsCommandPoolInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_GraphicsQueueFamilyIndex,
			};

			VkAssert(vkCreateCommandPool(m_VulkanDevice, &graphicsCommandPoolInfo, m_VulkanAllocationCallbacks, &m_GraphicsCommandPool),
				"failed to create transfer command pool (function vkCreateCommandPool in Renderer constructor)!");

			VkCommandPoolCreateInfo transferCommandPoolInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_TransferQueueFamilyIndex,
			};

			VkAssert(vkCreateCommandPool(m_VulkanDevice, &transferCommandPoolInfo, m_VulkanAllocationCallbacks, &m_TransferCommandPool),
				"failed to create transfer command pool (function vkCreateCommandPool in Renderer constructor)!");

			CreateSwapchain();

			m_GraphicsCommandBufferFreeList.Initialize(m_GraphicsCommandPool);
			m_TransferCommandBufferFreeList.Initialize(m_TransferCommandPool);

			VkDescriptorPoolSize modelPoolSize {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2,
			};

			m_ModelDescriptorPool.Create(1, &modelPoolSize);

			if (includeImGui) {
				IMGUI_CHECKVERSION();
				m_ImGuiContext = ImGui::CreateContext();

				ImGuiIO imguiIO = ImGui::GetIO();
				imguiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

				VkDescriptorPoolSize imguiPoolSize {
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
				};

				VkDescriptorPoolCreateInfo imguiPoolInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
					.maxSets = 1,
					.poolSizeCount = 1,
					.pPoolSizes = &imguiPoolSize,
				};

				VkAssert(vkCreateDescriptorPool(m_VulkanDevice, &imguiPoolInfo, m_VulkanAllocationCallbacks, &m_ImGuiGpuDecriptorPool), 
					"failed to create descriptor pool for ImGui (function vkCreateDescriptorPool in Renderer constructor)!");

				ImGui_ImplVulkan_InitInfo imguiVulkanInfo {
					.Instance = m_VulkanInstance,
					.PhysicalDevice = m_Gpu,
					.Device = m_VulkanDevice,
					.QueueFamily = m_GraphicsQueueFamilyIndex,
					.Queue = m_GraphicsQueue,
					.DescriptorPool = m_ImGuiGpuDecriptorPool,
					.RenderPass = VK_NULL_HANDLE,
					.MinImageCount = m_FramesInFlight,
					.ImageCount = m_FramesInFlight,
					.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
					.PipelineCache = VK_NULL_HANDLE,
					.Subpass = 0,
					.UseDynamicRendering = true,
					.PipelineRenderingCreateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
						.pNext = nullptr,
						.viewMask = 0,
						.colorAttachmentCount = 1,
						.pColorAttachmentFormats = &m_SwapchainSurfaceFormat.format,
						.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
						.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
					},
				};
			}
		}	

		~Renderer() {
			Terminate();
			free(m_SingleThreadStack.m_Data);
			free(m_InFlightRenderStack.m_Data);
		}

		void Terminate() {
			if (m_VulkanDevice) {
				m_ModelDescriptorPool.Terminate();
				vkDeviceWaitIdle(m_VulkanDevice);
				for (size_t i = 0; i < m_FramesInFlight; i++) {
					vkDestroyImageView(m_VulkanDevice, m_SwapchainImageViews[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_EarlyGraphicsSignalSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_RenderFinishedSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_RenderWaitSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightEarlyGraphicsFences[i].fence, m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightTransferFences[i].fence, m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightGraphicsFences[i].fence, m_VulkanAllocationCallbacks);
				}
				vkDestroySwapchainKHR(m_VulkanDevice, m_Swapchain, m_VulkanAllocationCallbacks);
				vkDestroyCommandPool(m_VulkanDevice, m_GraphicsCommandPool, m_VulkanAllocationCallbacks);
				vkDestroyCommandPool(m_VulkanDevice, m_TransferCommandPool, m_VulkanAllocationCallbacks);
				for (Thread& thread : m_Threads) {
					vkDestroyCommandPool(m_VulkanDevice, thread.m_GraphicsCommandPool, m_VulkanAllocationCallbacks);
					vkDestroyCommandPool(m_VulkanDevice, thread.m_TransferCommandPool, m_VulkanAllocationCallbacks);
				}
				vkDestroyDevice(m_VulkanDevice, m_VulkanAllocationCallbacks);
				m_VulkanDevice = VK_NULL_HANDLE;
			}
			if (m_VulkanInstance) {
				vkDestroySurfaceKHR(m_VulkanInstance, m_Surface, m_VulkanAllocationCallbacks);
				m_Surface = VK_NULL_HANDLE;
				vkDestroyInstance(m_VulkanInstance, m_VulkanAllocationCallbacks);
				m_VulkanInstance = VK_NULL_HANDLE;
			}
		}

		void CreateSwapchain() {

			int framebufferWidth, framebufferHeight;
			glfwGetFramebufferSize(m_Window, &framebufferWidth, &framebufferHeight);

			if (framebufferWidth == 0 || framebufferHeight == 0) {
				m_SwapchainExtent = { 0, 0 };
				return;
			}

			static constexpr auto clamp = [](uint32_t val, uint32_t min, uint32_t max) -> uint32_t { 
				val = val > min ? val : min;
				return val < max ? val : max;
			};

			VkSurfaceCapabilitiesKHR surfaceCapabilities;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Gpu, m_Surface, &surfaceCapabilities);

			uint32_t surfaceFormatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_Gpu, m_Surface, &surfaceFormatCount, nullptr);
			Stack::Array<VkSurfaceFormatKHR> surfaceFormats = m_SingleThreadStack.Allocate<VkSurfaceFormatKHR>(surfaceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(m_Gpu, m_Surface, &surfaceFormatCount, surfaceFormats.m_Data);

			if (!surfaceFormatCount) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "vulkan surface format count was 0 (in function CreateSwapchain)!", 
					VK_SUCCESS);
				return;
			}

			m_SwapchainSurfaceFormat = surfaceFormats[0];
			for (VkSurfaceFormatKHR format : surfaceFormats) {
				if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
					m_SwapchainSurfaceFormat = format;
					break;
				}
			}
			m_SingleThreadStack.Deallocate<VkSurfaceFormatKHR>(surfaceFormats.m_Size);

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_Gpu, m_Surface, &presentModeCount, nullptr);
			Stack::Array<VkPresentModeKHR> presentModes = m_SingleThreadStack.Allocate<VkPresentModeKHR>(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_Gpu, m_Surface, &presentModeCount, presentModes.m_Data);

			m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
			for (VkPresentModeKHR presentMode : presentModes) {
				if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					m_PresentMode = presentMode;
					break;
				}
			}
			m_SingleThreadStack.Deallocate<VkPresentModeKHR>(presentModes.m_Size);

			if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
				m_SwapchainExtent = surfaceCapabilities.currentExtent;
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
				m_SwapchainExtent = actualExtent;
			}

			uint32_t oldFramesInFlight = m_FramesInFlight;

			m_FramesInFlight = clamp(desired_frames_in_flight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

			uint32_t queueFamilyIndices[2] {
				m_GraphicsQueueFamilyIndex,
				m_PresentQueueFamilyIndex,
			};

			VkSwapchainCreateInfoKHR swapchainInfo {
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.pNext = nullptr,
				.flags = 0,
				.surface = m_Surface,
				.minImageCount = m_FramesInFlight,
				.imageFormat = m_SwapchainSurfaceFormat.format,
				.imageColorSpace = m_SwapchainSurfaceFormat.colorSpace,
				.imageExtent = m_SwapchainExtent,
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				.imageSharingMode = VK_SHARING_MODE_CONCURRENT,
				.queueFamilyIndexCount = 2,
				.pQueueFamilyIndices = queueFamilyIndices,
				.preTransform = surfaceCapabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = m_PresentMode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE,
			};

			VkAssert(vkCreateSwapchainKHR(m_VulkanDevice, &swapchainInfo, m_VulkanAllocationCallbacks, &m_Swapchain), 
					"failed to create vulkan swapchain (function vkCreateSwapchainKHR in function CreateSwapchain)!");

			vkGetSwapchainImagesKHR(m_VulkanDevice, m_Swapchain, &m_FramesInFlight, nullptr);

			vkQueueWaitIdle(m_GraphicsQueue);
			vkQueueWaitIdle(m_TransferQueue);

			if (m_FramesInFlight != oldFramesInFlight) {

				for (size_t i = 0; i < oldFramesInFlight; i++) {
					vkDestroyImageView(m_VulkanDevice, m_SwapchainImageViews[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_EarlyGraphicsSignalSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_RenderFinishedSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroySemaphore(m_VulkanDevice, m_RenderWaitSemaphores[i], m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightEarlyGraphicsFences[i].fence, m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightTransferFences[i].fence, m_VulkanAllocationCallbacks);
					vkDestroyFence(m_VulkanDevice, m_InFlightGraphicsFences[i].fence, m_VulkanAllocationCallbacks);
					for (const CommandBufferSubmitCallback& callback : m_CommandBufferSubmitCallbacks[i]) {
						callback.Callback(*this);
					}
					(&m_CommandBufferSubmitCallbacks[i])->~OneTypeStack();
				}

				if (!m_InFlightRenderStack.Allocate<VkImageView>(m_FramesInFlight, &m_SwapchainImageViews) ||
					!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_EarlyGraphicsSignalSemaphores) ||
					!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_RenderFinishedSemaphores) ||
					!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_RenderWaitSemaphores) ||
					!m_InFlightRenderStack.Allocate<Fence>(m_FramesInFlight, &m_InFlightEarlyGraphicsFences) ||
					!m_InFlightRenderStack.Allocate<Fence>(m_FramesInFlight, &m_InFlightTransferFences) ||
					!m_InFlightRenderStack.Allocate<Fence>(m_FramesInFlight, &m_InFlightGraphicsFences) ||
					!m_InFlightRenderStack.Allocate<VkCommandBuffer>(m_FramesInFlight, &m_RenderCommandBuffers) ||
					!m_InFlightRenderStack.Allocate<OneTypeStack<CommandBufferSubmitCallback, max_command_buffer_submit_callbacks>>(
						m_FramesInFlight, &m_CommandBufferSubmitCallbacks) ||
					!m_InFlightRenderStack.Allocate<VkImage>(m_FramesInFlight, &m_SwapchainImages))
				{
					m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
						"in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				}

				vkGetSwapchainImagesKHR(m_VulkanDevice, m_Swapchain, &m_FramesInFlight, m_SwapchainImages.m_Data);

				VkCommandBufferAllocateInfo renderCommandBufferAllocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.pNext = nullptr,
					.commandPool = m_GraphicsCommandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = m_FramesInFlight,
				};

				VkAssert(vkAllocateCommandBuffers(m_VulkanDevice, &renderCommandBufferAllocInfo, m_RenderCommandBuffers.m_Data), 
						"failed to allocate render command buffers (function vkAllocateCommandBuffers in function CreateSwapchain!)");

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
						.image = m_SwapchainImages[i],
						.viewType = VK_IMAGE_VIEW_TYPE_2D,
						.format = m_SwapchainSurfaceFormat.format,
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
					VkAssert(vkCreateImageView(m_VulkanDevice, &imageViewInfo, m_VulkanAllocationCallbacks, &m_SwapchainImageViews[i]), 
						"failed to create swapchain image view (function vkCreateImageView in function CreateSwapchain)!");
					VkAssert(vkCreateSemaphore(m_VulkanDevice, &semaphoreInfo, m_VulkanAllocationCallbacks, 
						&m_EarlyGraphicsSignalSemaphores[i]),
						"failed to create early graphics semaphores (function vkCreateSemaphore in function CreateSwapchain)!");
					VkAssert(vkCreateSemaphore(m_VulkanDevice, &semaphoreInfo, m_VulkanAllocationCallbacks, 
						&m_RenderFinishedSemaphores[i]), 
						"failed to create render finished semaphore (function vkCreateSemaphore in function CreateSwapchain)!");
					VkAssert(vkCreateSemaphore(m_VulkanDevice, &semaphoreInfo, m_VulkanAllocationCallbacks, &m_RenderWaitSemaphores[i]), 
						"failed to create render wait semaphore (function vkCreateSemaphore in function CreateSwapchain)!");
					VkAssert(vkCreateFence(m_VulkanDevice, &fenceInfo, m_VulkanAllocationCallbacks, 
						&m_InFlightEarlyGraphicsFences[i].fence),
						"failed to create in flight early graphics fence (function vkCreateFence in function CreateSwapchain)!");
					VkAssert(vkCreateFence(m_VulkanDevice, &fenceInfo, m_VulkanAllocationCallbacks, &m_InFlightTransferFences[i].fence), 
						"failed to create in flight transfer fence (function vkCreateFence in function CreateSwapchain)!");
					VkAssert(vkCreateFence(m_VulkanDevice, &fenceInfo, m_VulkanAllocationCallbacks, &m_InFlightGraphicsFences[i].fence), 
						"failed to create in flight graphis fence (function vkCreateFence in function CreateSwapchain)!");
					VkSubmitInfo dummySubmitInfo {
						.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
						.commandBufferCount = 0,
						.pCommandBuffers = nullptr,
					};
					vkQueueSubmit(m_GraphicsQueue, 1, &dummySubmitInfo, m_InFlightEarlyGraphicsFences[i].fence);
					vkQueueSubmit(m_GraphicsQueue, 1, &dummySubmitInfo, m_InFlightTransferFences[i].fence);
					vkQueueSubmit(m_GraphicsQueue, 1, &dummySubmitInfo, m_InFlightGraphicsFences[i].fence);
					m_InFlightEarlyGraphicsFences[i].state = Fence::State::Resettable;
					m_InFlightTransferFences[i].state = Fence::State::Resettable;
					m_InFlightGraphicsFences[i].state = Fence::State::Resettable;
				}
				m_CurrentFrame = 0;
			}
			else {

				vkGetSwapchainImagesKHR(m_VulkanDevice, m_Swapchain, &m_FramesInFlight, m_SwapchainImages.m_Data);

				Stack::Array<VkFence> resetFences = m_SingleThreadStack.Allocate<VkFence>(m_FramesInFlight * 3);
				uint32_t resetFenceCount = 0;

				for (size_t i = 0; i < m_FramesInFlight; i++) {
					vkDestroyImageView(m_VulkanDevice, m_SwapchainImageViews[i], m_VulkanAllocationCallbacks);
					VkImageViewCreateInfo imageViewInfo{
						.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.image = m_SwapchainImages[i],
						.viewType = VK_IMAGE_VIEW_TYPE_2D,
						.format = m_SwapchainSurfaceFormat.format,
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
					VkAssert(vkCreateImageView(m_VulkanDevice, &imageViewInfo, m_VulkanAllocationCallbacks, &m_SwapchainImageViews[i]), 
							"failed to create swapchain image view (function vkCreateImageView in function CreateSwapchain)!");
					if (m_InFlightGraphicsFences[i].state == Fence::State::Resettable) {
						resetFences[resetFenceCount++] = m_InFlightGraphicsFences[i].fence;
						m_InFlightGraphicsFences[i].state = Fence::State::None;
					}
					if (m_InFlightTransferFences[i].state == Fence::State::Resettable) {
						resetFences[resetFenceCount++] = m_InFlightTransferFences[i].fence;
						m_InFlightTransferFences[i].state = Fence::State::None;
					}
					if (m_InFlightEarlyGraphicsFences[i].state == Fence::State::Resettable) {
						resetFences[resetFenceCount++] = m_InFlightEarlyGraphicsFences[i].fence;
						m_InFlightEarlyGraphicsFences[i].state = Fence::State::None;
					}
				}
				vkResetFences(m_VulkanDevice, resetFenceCount, resetFences.m_Data);
			}

			m_SwapchainCreateCallback(this, m_SwapchainExtent, m_FramesInFlight, m_SwapchainImageViews.m_Data);

			m_GraphicsCommandBufferQueueMutex.lock();
			CommandBuffer<Queue::Graphics>* transitionCommandBuffer = m_GraphicsCommandBufferQueue.New();

			Assert(transitionCommandBuffer, ErrorOrigin::OutOfMemory, 
				"failed to allocate graphics command buffer (function OneTypeStack::New in function CreateSwapchain)!");

			VkCommandBufferAllocateInfo transitionCommandBuffersAllocInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_GraphicsCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			VkAssert(vkAllocateCommandBuffers(m_VulkanDevice, &transitionCommandBuffersAllocInfo, 
				&transitionCommandBuffer->m_CommandBuffer),
				"failed to allocate command buffer for swapchain image view layout transition (function vkAllocateCommandBuffers in function CreateSwapchain)!");

			VkCommandBufferBeginInfo transitionBeginInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr,
			};

			VkAssert(vkBeginCommandBuffer(transitionCommandBuffer->m_CommandBuffer, &transitionBeginInfo),
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
				imageMemoryBarriers[i].image = m_SwapchainImages[i];
				imageMemoryBarriers[i].subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				};
			}

			vkCmdPipelineBarrier(transitionCommandBuffer->m_CommandBuffer,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 
				imageMemoryBarriers.m_Size, imageMemoryBarriers.m_Data);

			VkAssert(vkEndCommandBuffer(transitionCommandBuffer->m_CommandBuffer),
				"failed to end swapchain image view layout transition command buffer (function vkEndCommandBuffer in function CreateSwapchain)");

			transitionCommandBuffer->m_Flags = CommandBufferFlag_FreeAfterSubmit;

			m_GraphicsCommandBufferQueueMutex.unlock();

			m_SingleThreadStack.Clear();
			m_CurrentFrame = 0;
		}

		void RecreateSwapchain() {
			vkDestroySwapchainKHR(m_VulkanDevice, m_Swapchain, m_VulkanAllocationCallbacks);
			CreateSwapchain();
			m_GraphicsCommandBufferFreeList.Reallocate();
			m_TransferCommandBufferFreeList.Reallocate();
			for (Thread& thread : m_Threads) {
				thread.m_GraphicsCommandBufferFreeList.Reallocate();
				thread.m_TransferCommandBufferFreeList.Reallocate();
			}
		}

		template<Queue queue_T>
		VkCommandPool GetCommandPool(std::thread::id threadID = std::this_thread::get_id()) {
			static_assert(queue_T != Queue::Present, "invalid queue in function Renderer::GetCommandBuffer!");
			if (threadID == m_MainThreadID) {
				if constexpr (queue_T == Queue::Graphics) {
					return m_GraphicsCommandPool;
				}
				else {
					return m_TransferCommandPool;
				}
			}
			else {
				LockGuard lockGuard(m_ThreadsMutex);
				Thread* thread = m_Threads.begin();
				Thread* end = m_Threads.end();
				for (; thread != end; thread++) {
					if (thread->m_ThreadID == threadID) {
						break;
					}
				}
				if (thread == end) {
					thread = m_Threads.New(*this, threadID);
				}
				if constexpr (queue_T == Queue::Graphics) {
					return thread->m_GraphicsCommandPool;
				}
				else {
					return thread->m_TransferCommandPool;
				}
			}
			return VK_NULL_HANDLE;
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
			if (!VkCheck(vkCreateDescriptorSetLayout(m_VulkanDevice, &createInfo, m_VulkanAllocationCallbacks, &res), 
				"failed to create descriptor set layout (function vkCreateDescriptorSetLayout in function CreateDescriptorSetLayout)!")) {
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool AllocateDescriptorSets(const DescriptorPool& descriptorPool, uint32_t setCount, 
			VkDescriptorSetLayout* pLayouts, VkDescriptorSet outSets[]) {
			if (descriptorPool.m_DescriptorPool == VK_NULL_HANDLE) {
				PrintError(ErrorOrigin::Vulkan, 
					"attempting to allocate descriptor sets with a descriptor pool that's null!");
				return false;
			}
			VkDescriptorSetAllocateInfo allocInfo {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = descriptorPool.m_DescriptorPool,
				.descriptorSetCount = setCount,
				.pSetLayouts = pLayouts,
			};
			VkResult vkRes = vkAllocateDescriptorSets(m_VulkanDevice, &allocInfo, outSets);
			if (vkRes == VK_ERROR_OUT_OF_POOL_MEMORY) {
				PrintError(ErrorOrigin::Vulkan, 
					"failed to allocate descriptor sets (function vkAllocateDescriptorSets in function AllocateDescriptorSets) because descriptor pool is out of memory!", 
					vkRes);
				return false;
			}
			else if (!VkCheck(vkRes,"failed to allocate descriptor sets (in function AllocateDescriptorSets)!")) {
				return false;
			}
			return true;
		}

		void UpdateDescriptorSets(uint32_t writeCount, const VkWriteDescriptorSet* writes) {
			vkUpdateDescriptorSets(m_VulkanDevice, writeCount, writes, 0, nullptr);
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
			if (!VkCheck(vkCreatePipelineLayout(m_VulkanDevice, &createInfo, m_VulkanAllocationCallbacks, &res), 
				"failed to create pipeline layout (function vkCreatePipelineLayout in function CreatePipelineLayout)!")) {
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool CreateGraphicsPipelines(uint32_t pipelineCount, VkGraphicsPipelineCreateInfo pipelineCreateInfos[], VkPipeline outPipelines[]) {
			if (!VkCheck(vkCreateGraphicsPipelines(m_VulkanDevice, VK_NULL_HANDLE, pipelineCount, pipelineCreateInfos, 
					m_VulkanAllocationCallbacks, outPipelines), 
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
			if (m_Swapchain == VK_NULL_HANDLE || m_SwapchainExtent.width == 0 || m_SwapchainExtent.height == 0) {
				return false;
			}
			if (m_CurrentFrame >= m_FramesInFlight) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, 
					"current frame was larger than frames in flights (in function BeginFrame)", VK_SUCCESS);
				return false;
			}

			VkFence waitFences[3]{};
			uint32_t waitFenceCount{};

			if (m_InFlightEarlyGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable) {
				waitFences[waitFenceCount++] = m_InFlightEarlyGraphicsFences[m_CurrentFrame].fence;
			}

			if (m_InFlightTransferFences[m_CurrentFrame].state == Fence::State::Resettable) {
				waitFences[waitFenceCount++] = m_InFlightTransferFences[m_CurrentFrame].fence;
			}

			if (m_InFlightGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable) {
				waitFences[waitFenceCount++] = m_InFlightGraphicsFences[m_CurrentFrame].fence;
			}

			if (waitFenceCount) {
				if (!VkCheck(vkWaitForFences(m_VulkanDevice, waitFenceCount, waitFences, VK_TRUE, frame_timeout), 
						"failed to wait for in flight fences (function vkWaitForFences in function BeginFrame)!")) {
					return false;
				}
				vkResetFences(m_VulkanDevice, waitFenceCount, waitFences);
			}

			m_InFlightEarlyGraphicsFences[m_CurrentFrame].state = Fence::State::None;
			m_InFlightTransferFences[m_CurrentFrame].state = Fence::State::None;
			m_InFlightGraphicsFences[m_CurrentFrame].state = Fence::State::None;

			for (const CommandBufferSubmitCallback& callback : m_CommandBufferSubmitCallbacks[m_CurrentFrame]) {
				callback.Callback(*this);
			}

			m_CommandBufferSubmitCallbacks[m_CurrentFrame].Clear();

			m_GraphicsCommandBufferFreeList.Free(m_CurrentFrame);
			m_TransferCommandBufferFreeList.Free(m_CurrentFrame);

			m_ThreadsMutex.lock();
			for (Thread& thread : m_Threads) {
				thread.m_GraphicsCommandBufferFreeList.Free(m_CurrentFrame);
				thread.m_TransferCommandBufferFreeList.Free(m_CurrentFrame);
			}
			m_ThreadsMutex.unlock();


			m_EarlyGraphicsCommandBufferQueueMutex.lock();

			if (m_EarlyGraphicsCommandBufferQueue.m_Count) {

				Stack::Array<VkCommandBuffer> commandBuffers
				 	= m_SingleThreadStack.Allocate<VkCommandBuffer>(m_EarlyGraphicsCommandBufferQueue.m_Count);

				 if (!commandBuffers.m_Data) {
					 m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory,
					 	"single thread stack was out of memory (function Stack::Allocate in function BeginFrame)!",
						VK_SUCCESS);
				 }

				 for (size_t i = 0; i < m_EarlyGraphicsCommandBufferQueue.m_Count; i++) {
					 CommandBuffer<Queue::Graphics> commandBuffer = m_EarlyGraphicsCommandBufferQueue.m_Data[i];
					 commandBuffers[i] = commandBuffer.m_CommandBuffer;
					 if (commandBuffer.m_Flags & CommandBufferFlag_SubmitCallback) {
						 Assert(m_CommandBufferSubmitCallbacks[m_CurrentFrame].New(commandBuffer.m_SubmitCallback), 
						 	ErrorOrigin::OutOfMemory, 
							"command buffer submit callbacks was out of memory (function OneTypeStack::New in function BeginFrame)!");
					 }
					 if (commandBuffer.m_Flags & CommandBufferFlag_FreeAfterSubmit) {
						if (commandBuffer.m_ThreadID == m_MainThreadID) {
							m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
						}
						else {
							bool threadFound = false;
							for (Thread& thread : m_Threads) {
								if (thread.m_ThreadID == commandBuffer.m_ThreadID) {
									thread.m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
									threadFound = true;
									break;
								}
							}
							if (!threadFound) {
								PrintError(ErrorOrigin::Threading, 
									"failed to find transfer command buffer thread (in function BeginFrame)!");
							}
						}
					 }
				 }

				VkSubmitInfo submitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.commandBufferCount = (uint32_t)commandBuffers.m_Size,
					.pCommandBuffers = commandBuffers.m_Data,
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &m_EarlyGraphicsSignalSemaphores[m_CurrentFrame],
				};

				if (VkCheck(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightEarlyGraphicsFences[m_CurrentFrame].fence), 
						"failed to submit to early graphics queue (function vkQueueSubmit in function BeginFrame)!")) {
					m_InFlightEarlyGraphicsFences[m_CurrentFrame].state = Fence::State::Resettable;
				}

				m_EarlyGraphicsCommandBufferQueue.Clear();
				m_SingleThreadStack.Clear();
			}

			m_EarlyGraphicsCommandBufferQueueMutex.unlock();

			m_TransferCommandBufferQueueMutex.lock();

			if (m_TransferCommandBufferQueue.m_Count) {

				Stack::Array<VkCommandBuffer> commandBuffers 
					= m_SingleThreadStack.Allocate<VkCommandBuffer>(m_TransferCommandBufferQueue.m_Count);

				if (!commandBuffers.m_Data) {
					m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
						"single thread stack was out of memory (function Stack::Allocate in function BeginFrame)!", 
						VK_SUCCESS);
				}

				for (size_t i = 0; i < m_TransferCommandBufferQueue.m_Count; i++) {
					CommandBuffer<Queue::Transfer>& commandBuffer = m_TransferCommandBufferQueue.m_Data[i];
					commandBuffers[i] = commandBuffer.m_CommandBuffer;
					if (commandBuffer.m_Flags & CommandBufferFlag_SubmitCallback) {
						 Assert(m_CommandBufferSubmitCallbacks[m_CurrentFrame].New(commandBuffer.m_SubmitCallback), 
						 	ErrorOrigin::OutOfMemory, 
							"command buffer submit callbacks was out of memory (function OneTypeStack::New in function BeginFrame)!");
					}
					if (commandBuffer.m_Flags & CommandBufferFlag_FreeAfterSubmit) {
						if (commandBuffer.m_ThreadID == m_MainThreadID) {
							m_TransferCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
						}
						else {
							bool threadFound = false;
							for (Thread& thread : m_Threads) {
								if (thread.m_ThreadID == commandBuffer.m_ThreadID) {
									thread.m_TransferCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
									threadFound = true;
									break;
								}
							}
							if (!threadFound) {
								PrintError(ErrorOrigin::Threading, 
									"failed to find transfer command buffer thread (in function BeginFrame)!");
							}
						}
					}
				}

				VkPipelineStageFlags stageFlag = VK_PIPELINE_STAGE_TRANSFER_BIT;

				VkSubmitInfo submitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount 
						= m_InFlightEarlyGraphicsFences[m_CurrentFrame].state == Fence::State::Resettable ? 1U : 0U,
					.pWaitSemaphores = &m_EarlyGraphicsSignalSemaphores[m_CurrentFrame],
					.pWaitDstStageMask = &stageFlag,
					.commandBufferCount = (uint32_t)commandBuffers.m_Size,
					.pCommandBuffers = commandBuffers.m_Data,
				};

				if (VkCheck(vkQueueSubmit(m_TransferQueue, 1, &submitInfo, m_InFlightTransferFences[m_CurrentFrame].fence), 
						"failed to submit to transfer queue (function vkQueueSubmit in function BeginFrame)!")) {
					m_InFlightTransferFences[m_CurrentFrame].state = Fence::State::Resettable;
				}

				m_TransferCommandBufferQueue.Clear();
				m_SingleThreadStack.Clear();
			}

			m_TransferCommandBufferQueueMutex.unlock();

			uint32_t imageIndex;
			VkResult result = vkAcquireNextImageKHR(m_VulkanDevice, m_Swapchain, frame_timeout, 
					m_RenderWaitSemaphores[m_CurrentFrame], VK_NULL_HANDLE , &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				RecreateSwapchain();
				return false;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				PrintError(ErrorOrigin::Vulkan, "failed to acquire next swapchain image (in function BeginFrame)!", result);
				return false;
			}
			if (imageIndex != m_CurrentFrame) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "image index didn't match current frame (in function BeginFrame)!", 
					VK_SUCCESS);
				return false;
			}
			outDrawData.swapchainImageView = m_SwapchainImageViews[m_CurrentFrame];
			outDrawData.commandBuffer = m_RenderCommandBuffers[m_CurrentFrame];
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

			vkEndCommandBuffer(m_RenderCommandBuffers[m_CurrentFrame]);

			Stack::Array<VkCommandBuffer> graphicsCommandBuffers { 0, nullptr };

			m_GraphicsCommandBufferQueueMutex.lock();

			if (m_GraphicsCommandBufferQueue.m_Count) {

				m_SingleThreadStack.Allocate<VkCommandBuffer>(m_GraphicsCommandBufferQueue.m_Count, &graphicsCommandBuffers);

				if (!graphicsCommandBuffers.m_Data) {
					m_CriticalErrorCallback(this, ErrorOrigin::OutOfMemory, 
						"single thread stack was out of memory (in function EndFrame)!", VK_SUCCESS);
				}

				for (size_t i = 0; i < m_GraphicsCommandBufferQueue.m_Count; i++) {
					CommandBuffer<Queue::Graphics>& commandBuffer = m_GraphicsCommandBufferQueue.m_Data[i];
					graphicsCommandBuffers[i] = commandBuffer.m_CommandBuffer;
					if (commandBuffer.m_Flags & CommandBufferFlag_FreeAfterSubmit) {
						if (commandBuffer.m_Flags & CommandBufferFlag_FreeAfterSubmit) {
							if (commandBuffer.m_ThreadID == m_MainThreadID) {
								m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
							}
							else {
								bool threadFound = false;
								for (Thread& thread : m_Threads) {
									if (thread.m_ThreadID == commandBuffer.m_ThreadID) {
										thread.m_GraphicsCommandBufferFreeList.Push(commandBuffer.m_CommandBuffer, m_CurrentFrame);
										threadFound = true;
										break;
									}
								}
								if (!threadFound) {
									PrintError(ErrorOrigin::Threading, 
										"failed to find graphis command buffer thread (in function EndFrame)!");
								}
							}
						}
						if (commandBuffer.m_Flags & CommandBufferFlag_SubmitCallback) {
							m_CommandBufferSubmitCallbacks[m_CurrentFrame].New(commandBuffer.m_SubmitCallback);
						}
					}
				}
				m_GraphicsCommandBufferQueue.Clear();
			}


			m_GraphicsCommandBufferQueueMutex.unlock();

			VkPipelineStageFlags graphicsWaitStages[1] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkSubmitInfo graphicsSubmits[2] {
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &m_RenderWaitSemaphores[m_CurrentFrame],
					.pWaitDstStageMask = graphicsWaitStages,
					.commandBufferCount = 1,
					.pCommandBuffers = &m_RenderCommandBuffers[m_CurrentFrame],
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame],
				},
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.pNext = nullptr,
					.commandBufferCount = (uint32_t)graphicsCommandBuffers.m_Size,
					.pCommandBuffers = graphicsCommandBuffers.m_Data,
				}
			};

			m_SingleThreadStack.Clear();

			if (!VkCheck(vkQueueSubmit(m_GraphicsQueue, graphicsCommandBuffers.m_Size ? 2 : 1, graphicsSubmits, 
					m_InFlightGraphicsFences[m_CurrentFrame].fence), 
					"failed to submit to graphics queue (function vkQueueSubmit in function EndFrame)!")) {
				return;
			}

			m_InFlightGraphicsFences[m_CurrentFrame].state = Fence::State::Resettable;	

			VkPresentInfoKHR presentInfo {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = nullptr,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame],
				.swapchainCount = 1,
				.pSwapchains = &m_Swapchain,
				.pImageIndices = &m_CurrentFrame,
				.pResults = nullptr,
			};

			VkResult vkRes = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

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
