#pragma once

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

namespace engine {

#ifndef BOOL_RENDERER_DEBUG 
#define BOOL_RENDERER_DEBUG true 
#endif

	class Renderer {
	public:

		enum class ErrorOrigin {
			Uncategorized = 0,
			InitializationFailed = 1,
			Vulkan = 2,
			StackOutOfMemory = 3,
			NullDereference = 4,
			IndexOutOfBounds = 5,
			Shader = 6,
			MaxEnum,
		};

		typedef void (*ErrorCallback)(const Renderer* renderer, ErrorOrigin origin, const char* err, VkFlags vkErr);
		typedef void (*SwapchainCreateCallback)(const Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews);

		static const char* ErrorOriginStr(ErrorOrigin origin) {
			static constexpr const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"InitializationFailed",
				"Vulkan",
				"StackOutOfMemory",
				"NullDereference",
				"IndexOutOfBounds",
				"Shader",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		static void PrintMessage(const char* msg) {
			printf("renderer message: %s\n", msg);
		}

		static void PrintWarning(const char* warn) {
			printf("renderer warning: %s\n", warn);
		}

		static void PrintError(const char* err, ErrorOrigin origin) {
			printf("renderer error: %s error origin: %s\n", err, Renderer::ErrorOriginStr(origin));
		}

		struct Stack {

			template<typename T>
			struct Array {

				T* const m_Data;
				const size_t m_Size;

				Array(T* data, size_t size) : m_Data(data), m_Size(size) {}
	
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

				T* operator[](size_t index) {
					if (index >= m_Size) {
						PrintError("index out of bounds (engine::Renderer::Stack::Array::operator[])!", ErrorOrigin::IndexOutOfBounds);
						return nullptr;
					}
					return &m_Data[index];
				}
			};

			uint8_t* const m_Data;
			const size_t m_MaxSize;
			size_t m_UsedSize;

			Stack(uint8_t* data, size_t maxSize)
				: m_Data(data), m_MaxSize(maxSize), m_UsedSize(0) {
				if (!m_Data) {
					PrintWarning("data was null (engine::Renderer::Stack constructor)!");
				}
			}

			template<typename T>
			Array<T> Allocate(size_t count) {
				T* ret = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintError("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::StackOutOfMemory);
					return Array<T>(nullptr, 0);
				}
				T* iter = ret;
				T* end = &ret[count];
				for (; iter != end; iter++) {
					new(iter) T();
				}
				return Array<T>(ret, count);
			}

			template<typename T>
			bool Allocate(size_t count, Array<T>* out) {
				T* ret = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintError("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::StackOutOfMemory);
					return false;
				}
				T* iter = ret;
				T* end = &ret[count];
				for (; iter != end; iter++) {
					new(iter) T();
				}
				new(out) Array<T>(ret, count);
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

			bool NotCompiled() const {
				return !m_GlslangShader || !m_GlslangProgram;
			}

			size_t GetBinarySize() const noexcept {
				if (NotCompiled()) {
					return 0;
				}
				return glslang_program_SPIRV_get_size(m_GlslangProgram);
			}

			const unsigned int* GetBinary() const noexcept {
				if (NotCompiled()) {
					return nullptr;
				}
				return glslang_program_SPIRV_get_ptr(m_GlslangProgram);
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

		static constexpr uint32_t cexpr_desired_frames_in_flight = 2;
		static constexpr const char* cexpr_gpu_validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* cexpr_gpu_dynamic_rendering_extension_name = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
		static constexpr const char* cexpr_gpu_swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		static constexpr size_t cexpr_in_flight_render_stack_size = 512;
		static constexpr size_t cexpr_single_thread_stack_size = 524288;
		static constexpr size_t cexpr_graphics_commands_stack_size = 32768;

		static constexpr size_t max_model_descriptor_sets = 250000;

		static bool GpuSucceeded(VkResult result) {
			return result == VK_SUCCESS;
		}

		Stack m_SingleThreadStack;
		uint8_t m_InFlightRenderStackData[cexpr_in_flight_render_stack_size];
		Stack m_InFlightRenderStack;
		Stack m_GraphicsCommandsStack;

		VkDevice m_GpuDevice = VK_NULL_HANDLE;
		VkAllocationCallbacks* m_GpuAllocationCallbacks = nullptr;
		VkPhysicalDevice m_Gpu = VK_NULL_HANDLE;
		VkSampleCountFlags m_GpuColorMsaaSamples = 1;
		VkSampleCountFlags m_GpuDepthMsaaSamples = 1;
		VkSurfaceKHR m_GpuSurface = VK_NULL_HANDLE;
		uint32_t m_GpuMaxFragmentOutputAttachments;

		VkDescriptorSetLayout m_ModelDescriptorSetLayout;
		DescriptorPool m_ModelDescriptorPool { *this, max_model_descriptor_sets };

		Stack::Array<VkCommandBuffer> m_GpuRenderCommandBuffers { 0, 0 };
		Stack::Array<VkSemaphore> m_GpuRenderFinishedSemaphores { 0, 0 };
		Stack::Array<VkSemaphore> m_GpuRenderWaitSemaphores { 0, 0 };
		Stack::Array<VkFence> m_GpuInFlightFences { 0, 0 };
		Stack::Array<VkImageView> m_GpuSwapchainImageViews { 0, 0 };
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
		VkCommandPool m_MainThreadGpuGraphicsCommandPool = VK_NULL_HANDLE;
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

		Renderer(const char* appName, uint32_t appVersion, GLFWwindow* window, ErrorCallback criticalErrorCallback, ErrorCallback errorCallback, SwapchainCreateCallback swapchainCreateCallback)
			: m_SingleThreadStack((uint8_t*)malloc(cexpr_single_thread_stack_size), cexpr_single_thread_stack_size), 
				m_InFlightRenderStack(m_InFlightRenderStackData, cexpr_in_flight_render_stack_size), m_Window(window),
				m_GraphicsCommandsStack((uint8_t*)malloc(cexpr_graphics_commands_stack_size), cexpr_graphics_commands_stack_size),
				m_CriticalErrorCallback(criticalErrorCallback), m_ErrorCallback(errorCallback), m_SwapchainCreateCallback(swapchainCreateCallback) {

			assert(m_CriticalErrorCallback && "critical error callback was null (renderer)!");
			assert(m_ErrorCallback && "error callback was null (renderer)!");
			assert(m_SwapchainCreateCallback && "swapchain create callback was null (renderer)!");

			uint32_t instanceExtensionCount;
			const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionCount);

			uint32_t instanceExtensionsNotFoundCount = instanceExtensionCount;

			uint32_t availableInstanceExtensionCount;
			vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, nullptr);
			Stack::Array<VkExtensionProperties> availableGpuInstanceExtensions = m_SingleThreadStack.Allocate<VkExtensionProperties>(availableInstanceExtensionCount);
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

			if (instanceExtensionsNotFoundCount) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, "couldn't find all the required vulkan instance extensions (in Renderer constructor)!", VK_SUCCESS);
			}

			bool includeGpuValidationLayer = false;
			if constexpr (BOOL_RENDERER_DEBUG) {
				uint32_t availableInstanceLayerCount;
				vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount, nullptr);
				Stack::Array<VkLayerProperties> availableGpuInstanceLayers = m_SingleThreadStack.Allocate<VkLayerProperties>(availableInstanceLayerCount);
				vkEnumerateInstanceLayerProperties(&availableInstanceLayerCount, availableGpuInstanceLayers.m_Data);
				for (VkLayerProperties& layer : availableGpuInstanceLayers) {
					if (!strcmp(layer.layerName, cexpr_gpu_validation_layer_name)) {
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
				.ppEnabledLayerNames = includeGpuValidationLayer ? &cexpr_gpu_validation_layer_name : nullptr,
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
				for (VkExtensionProperties& extension : deviceExtensions) {
					if (!strcmp(cexpr_gpu_dynamic_rendering_extension_name, extension.extensionName)) {
						dynamicRenderingExtensionFound = true;
						break;
					}
				}
				m_SingleThreadStack.Deallocate<VkExtensionProperties>(deviceExtensions.m_Size);
				if (!dynamicRenderingExtensionFound) {
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

			VkPhysicalDeviceVulkan13Features gpuFeaturesVulkan13 {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = nullptr,
				.dynamicRendering = VK_TRUE,
			};

			VkDeviceCreateInfo deviceInfo {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &gpuFeaturesVulkan13,
				.queueCreateInfoCount = 3,
				.pQueueCreateInfos = deviceQueueInfos,
				.enabledExtensionCount = 1,
				.ppEnabledExtensionNames = &cexpr_gpu_swapchain_extension_name,
				.pEnabledFeatures = &gpuFeatures,
			};

			if (!VkAssert(vkCreateDevice(m_Gpu, &deviceInfo, m_GpuAllocationCallbacks, &m_GpuDevice), "failed to create vulkan device (function vkCreateDevice in Renderer constructor)!")) {
				return;
			}

			vkGetDeviceQueue(m_GpuDevice, m_GpuGraphicsQueueFamilyIndex, 0, &m_GpuGraphicsQueue);
			vkGetDeviceQueue(m_GpuDevice, m_GpuTransferQueueFamilyIndex, 0, &m_GpuTransferQueue);
			vkGetDeviceQueue(m_GpuDevice, m_GpuPresentQueueFamilyIndex, 0, &m_GpuPresentQueue);	

			m_SingleThreadStack.Clear();

			VkCommandPoolCreateInfo renderCommandPoolInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = m_GpuGraphicsQueueFamilyIndex,
			};

			if (!VkAssert(vkCreateCommandPool(m_GpuDevice, &renderCommandPoolInfo, m_GpuAllocationCallbacks, &m_MainThreadGpuGraphicsCommandPool), 
					"failed to create render command pool (function vkCreateCommandPool in Renderer constructor)!")) {
				return;
			}

			CreateSwapchain();

			VkDescriptorPoolSize modelPoolSize {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2,
			};

			m_ModelDescriptorPool.CreatePool(1, &modelPoolSize);
		}	

		~Renderer() {
			Terminate();
			free(m_SingleThreadStack.m_Data);
			free(m_GraphicsCommandsStack.m_Data);
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

			m_GpuSwapchainSurfaceFormat = *surfaceFormats[0];
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

			m_FramesInFlight = clamp(cexpr_desired_frames_in_flight, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

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

			size_t oldFramesInFlight = m_GpuSwapchainImageViews.m_Size;

			vkQueueWaitIdle(m_GpuGraphicsQueue);

			for (size_t i = 0; i < oldFramesInFlight; i++) {
				vkDestroyImageView(m_GpuDevice, *m_GpuSwapchainImageViews[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, *m_GpuRenderFinishedSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, *m_GpuRenderWaitSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroyFence(m_GpuDevice, *m_GpuInFlightFences[i], m_GpuAllocationCallbacks);
			}

			m_InFlightRenderStack.Clear();

			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, nullptr);
			if (!m_InFlightRenderStack.Allocate<VkImage>(m_FramesInFlight, &m_GpuSwapchainImages)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, m_GpuSwapchainImages.m_Data);

			if (!m_InFlightRenderStack.Allocate<VkImageView>(m_FramesInFlight, &m_GpuSwapchainImageViews)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderFinishedSemaphores)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderWaitSemaphores)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkFence>(m_FramesInFlight, &m_GpuInFlightFences)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkCommandBuffer>(m_FramesInFlight, &m_GpuRenderCommandBuffers)) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", 0);
				return;
			}

			VkCommandBufferAllocateInfo renderCommandBufferAllocInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_MainThreadGpuGraphicsCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = m_FramesInFlight,
			};

			if(!VkAssert(vkAllocateCommandBuffers(m_GpuDevice, &renderCommandBufferAllocInfo, m_GpuRenderCommandBuffers.m_Data), 
					"failed to allocate render command buffers (function vkAllocateCommandBuffers in function CreateSwapchain!)")) {
				return;
			}

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
					.image = *m_GpuSwapchainImages[i],
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
				if (!VkAssert(vkCreateImageView(m_GpuDevice, &imageViewInfo, m_GpuAllocationCallbacks, m_GpuSwapchainImageViews[i]), 
						"failed to create swapchain image view (function vkCreateImageView in function CreateSwapchain)!")) {
					return;
				}
				if (!VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, m_GpuRenderFinishedSemaphores[i]), 
						"failed to create render finished semaphore (function vkCreateSemaphore in function CreateSwapchain)")) {
					return;
				}
				if (!VkAssert(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, m_GpuRenderWaitSemaphores[i]), 
						"failed to create render finished semaphore (function vkCreateSemaphore in function CreateSwapchain)")) {
					return;
				}
				if (!VkAssert(vkCreateFence(m_GpuDevice, &fenceInfo, m_GpuAllocationCallbacks, m_GpuInFlightFences[i]), 
						"failed to create in flight fences (function vkCreateFence in function CreateSwapchain)")) {
					return;
				}
				VkSubmitInfo dummySubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 0,
					.pCommandBuffers = nullptr,
				};
				vkQueueSubmit(m_GpuGraphicsQueue, 1, &dummySubmitInfo, *m_GpuInFlightFences[i]);
			}
			m_CurrentFrame = 0;
			m_SwapchainCreateCallback(this, m_GpuSwapchainExtent, m_FramesInFlight, m_GpuSwapchainImageViews.m_Data);

			Stack::Array<VkCommandBuffer> transitionCommandBuffers = m_GraphicsCommandsStack.Allocate<VkCommandBuffer>(1);
			if (!transitionCommandBuffers.m_Data) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "graphics commands stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}

			VkCommandBufferAllocateInfo transitionCommandBuffersAllocInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_MainThreadGpuGraphicsCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = (uint32_t)transitionCommandBuffers.m_Size,
			};

			if (!VkAssert(vkAllocateCommandBuffers(m_GpuDevice, &transitionCommandBuffersAllocInfo, transitionCommandBuffers.m_Data),
					"failed to allocate command buffers for swapchain image view layout transition (function vkAllocateCommandBuffers in function CreateSwapchain)!")) {
				return;
			}

			VkCommandBufferBeginInfo transitionBeginInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pInheritanceInfo = nullptr,
			};

			if (!VkAssert(vkBeginCommandBuffer(*transitionCommandBuffers[0], &transitionBeginInfo), 
					"failed to begin swapchain image view layout transition command buffer (function vkBeginCommandBuffer in function CreateSwapchain)")) {
				return;
			}

			Stack::Array<VkImageMemoryBarrier> imageMemoryBarriers = m_SingleThreadStack.Allocate<VkImageMemoryBarrier>(m_FramesInFlight);

			if (!imageMemoryBarriers.m_Data) {
				m_CriticalErrorCallback(this, ErrorOrigin::StackOutOfMemory, "single thread stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}

			for (size_t i = 0; i < m_FramesInFlight; i++) {
				imageMemoryBarriers[i]->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarriers[i]->pNext = nullptr;
				imageMemoryBarriers[i]->srcAccessMask = 0;
				imageMemoryBarriers[i]->dstAccessMask = 0;
				imageMemoryBarriers[i]->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarriers[i]->newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				imageMemoryBarriers[i]->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarriers[i]->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarriers[i]->image = *m_GpuSwapchainImages[i];
				imageMemoryBarriers[i]->subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				};
			}

			vkCmdPipelineBarrier(*transitionCommandBuffers[0],
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 
				imageMemoryBarriers.m_Size, imageMemoryBarriers.m_Data);
			if (!VkAssert(vkEndCommandBuffer(*transitionCommandBuffers[0]), 
					"failed to end swapchain image view layout transition command buffer (function vkEndCommandBuffer in function CreateSwapchain)")) {
				return;
			}
		}

		void Terminate() {
			if (m_GpuDevice) {
				m_ModelDescriptorPool.Terminate();
				vkDeviceWaitIdle(m_GpuDevice);
				for (size_t i = 0; i < m_FramesInFlight; i++) {
					vkDestroyImageView(m_GpuDevice, m_GpuSwapchainImageViews[i] ? *m_GpuSwapchainImageViews[i] : nullptr, m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuRenderFinishedSemaphores[i] ? *m_GpuRenderFinishedSemaphores[i] : nullptr, m_GpuAllocationCallbacks);
					vkDestroySemaphore(m_GpuDevice, m_GpuRenderWaitSemaphores[i] ? *m_GpuRenderWaitSemaphores[i] : nullptr, m_GpuAllocationCallbacks);
					vkDestroyFence(m_GpuDevice, m_GpuInFlightFences[i] ? *m_GpuInFlightFences[i] : nullptr, m_GpuAllocationCallbacks);
				}
				vkDestroySwapchainKHR(m_GpuDevice, m_GpuSwapchain, m_GpuAllocationCallbacks);
				vkDestroyCommandPool(m_GpuDevice, m_MainThreadGpuGraphicsCommandPool, m_GpuAllocationCallbacks);
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

		void RecreateSwapchain() {
			vkDestroySwapchainKHR(m_GpuDevice, m_GpuSwapchain, m_GpuAllocationCallbacks);
			CreateSwapchain();
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
			VkResult vkRes = vkCreateDescriptorSetLayout(m_GpuDevice, &createInfo, m_GpuAllocationCallbacks, &res);
			if (vkRes != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, "failed to create descriptor set layout (function vkCreateDescriptorSetLayout in function CreateDescriptorSetLayout)!", vkRes);
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool AllocateDescriptorSets(const DescriptorPool& descriptorPool, uint32_t setCount, VkDescriptorSetLayout* pLayouts, VkDescriptorSet outSets[]) {
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
					"failed to allocate descriptor sets (function vkAllocateDescriptorSets in function AllocateDescriptorSets) because descriptor pool is out of memory!", vkRes);
				return false;
			}
			else if (vkRes != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, 
					"failed to allocate descriptor sets (in function AllocateDescriptorSets)!", vkRes);
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
			VkResult vkRes = vkCreatePipelineLayout(m_GpuDevice, &createInfo, m_GpuAllocationCallbacks, &res);
			if (vkRes != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan,
					"failed to create pipeline layout (function vkCreatePipelineLayout in function CreatePipelineLayout)!", vkRes);
				return VK_NULL_HANDLE;
			}
			return res;
		}

		bool CreateGraphicsPipelines(uint32_t pipelineCount, VkGraphicsPipelineCreateInfo pipelineCreateInfos[], VkPipeline outPipelines[]) {
			VkResult vkRes = vkCreateGraphicsPipelines(m_GpuDevice, VK_NULL_HANDLE, pipelineCount, pipelineCreateInfos, m_GpuAllocationCallbacks, outPipelines);
			if (vkRes != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, 
					"failed to create graphics pipelines (function vkCreateGraphicsPipelines in function CreateGraphicsPipelines)!", vkRes);
				return false;
			}
			return true;
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
			if (!VkCheck(vkWaitForFences(m_GpuDevice, 1, m_GpuInFlightFences[m_CurrentFrame], VK_TRUE, frame_timeout), "failed to wait for in flight fence (in function BeginFrame)!")) {
				return false;
			}
			uint32_t imageIndex;
			VkResult result = vkAcquireNextImageKHR(m_GpuDevice, m_GpuSwapchain, frame_timeout, 
					*m_GpuRenderWaitSemaphores[m_CurrentFrame], nullptr, &imageIndex);
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
			outDrawData.swapchainImageView = *m_GpuSwapchainImageViews[m_CurrentFrame];
			vkResetFences(m_GpuDevice, 1, m_GpuInFlightFences[m_CurrentFrame]);
			outDrawData.commandBuffer = *m_GpuRenderCommandBuffers[m_CurrentFrame];
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

			vkEndCommandBuffer(*m_GpuRenderCommandBuffers[m_CurrentFrame]);

			VkPipelineStageFlags waitStages[1] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkSubmitInfo submits[2]{
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = m_GpuRenderWaitSemaphores[m_CurrentFrame],
					.pWaitDstStageMask = waitStages,
					.commandBufferCount = 1,
					.pCommandBuffers = m_GpuRenderCommandBuffers[m_CurrentFrame],
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = m_GpuRenderFinishedSemaphores[m_CurrentFrame],
				},
				{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = (uint32_t)m_GraphicsCommandsStack.m_UsedSize / sizeof(VkCommandBuffer),
					.pCommandBuffers = (VkCommandBuffer*)m_GraphicsCommandsStack.m_Data,
				},
			};

			if (!VkCheck(vkQueueSubmit(m_GpuGraphicsQueue, 2, submits, *m_GpuInFlightFences[m_CurrentFrame]), 
				"failed to submit render to queue (function vkQueueSubmit in function EndFrame)!")) {
				return;
			}

			m_GraphicsCommandsStack.Clear();

			VkPresentInfoKHR presentInfo {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = m_GpuRenderFinishedSemaphores[m_CurrentFrame],
				.swapchainCount = 1,
				.pSwapchains = &m_GpuSwapchain,
				.pImageIndices = &m_CurrentFrame,
				.pResults = nullptr,
			};

			VkResult result = vkQueuePresentKHR(m_GpuGraphicsQueue, &presentInfo);

			m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				RecreateSwapchain();
			}
			else if (result != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, "failed to present image (function vkQueuePresentKHR in function EndFrame)!", result);
			}
		}	
	};
}
