#pragma once

#include "vulkan/vulkan.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
			MaxEnum,
		};

		typedef void (*ErrorCallback)(Renderer* renderer, ErrorOrigin origin, const char* err, VkFlags vkErr);
		typedef void (*SwapchainCreateCallback)(Renderer* renderer, VkExtent2D extent, uint32_t imageCount, VkImageView* imageViews);

		static const char* ErrorOriginStr(ErrorOrigin origin) {
			static constexpr const char* strings[static_cast<size_t>(ErrorOrigin::MaxEnum)] {
				"Uncategorized",
				"InitializationFailed",
				"Vulkan",
				"StackOutOfMemory",
				"NullDereference",
				"IndexOutOfBounds",
			};
			if (origin == ErrorOrigin::MaxEnum) {
				return strings[0];
			}
			return strings[(size_t)origin];
		}

		static void PrintMsg(const char* msg);
		static void PrintWarn(const char* warn);
		static void PrintErr(const char* err, ErrorOrigin origin);

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
						PrintErr("index out of bounds (engine::Renderer::Stack::Array::operator[])!", ErrorOrigin::IndexOutOfBounds);
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
					PrintWarn("data was null (engine::Renderer::Stack constructor)!");
				}
			}

			template<typename T>
			Array<T> Allocate(size_t count) {
				T* ret = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					PrintErr("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::StackOutOfMemory);
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
					PrintErr("stack out of memory (function engine::Renderer::Stack::Allocate)!", ErrorOrigin::StackOutOfMemory);
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

		static constexpr uint32_t cexpr_desired_frames_in_flight = 2;
		static constexpr const char* cexpr_gpu_validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* cexpr_gpu_dynamic_rendering_extension_name = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
		static constexpr const char* cexpr_gpu_swapchain_extension_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		static constexpr size_t cexpr_in_flight_render_stack_size = 512;
		static constexpr size_t cexpr_single_thread_stack_size = 524288;
		static constexpr size_t cexpr_graphics_commands_stack_size = 32768;

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

		bool VkAssert(VkResult result, const char* err) {
			if (result != VK_SUCCESS) {
				m_CriticalErrorCallback(this, ErrorOrigin::Vulkan, err, result);
				return false;
			}
			return true;
		}

		bool VkCheck(VkResult result, const char* err) {
			if (result != VK_SUCCESS) {
				m_ErrorCallback(this, ErrorOrigin::Vulkan, err, result);
				return false;
			}
			return true;
		}

		template<typename T>
		T* PtrAssert(T* ptr, const char* err) {
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
					PrintWarn("Vulkan Khronos validation not supported (in Renderer constructor)!");
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
