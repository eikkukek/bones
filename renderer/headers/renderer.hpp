#pragma once

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace game {

#ifndef BOOL_RENDERER_DEBUG 
#define BOOL_RENDERER_DEBUG true 
#endif


	class Renderer {

		static constexpr uint32_t cexpr_desired_frames_in_flight = 2;
		static constexpr const char* cexpr_gpu_validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* cexpr_gpu_dynamic_rendering_extension_name = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
		static constexpr size_t cexpr_in_flight_render_stack_size = 512;

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
						printf("index out of bounds (game::Renderer::Stack::Array::operator[])!");
						return nullptr;
					}
					return &m_Data[index];
				}
			};

			uint8_t* m_Data;
			const size_t m_MaxSize;
			size_t m_UsedSize;

			Stack(uint8_t* data, size_t maxSize)
				: m_Data(data), m_MaxSize(maxSize), m_UsedSize(0) {
				if (!m_Data) {
					printf("data was null (game::Renderer::Stack constructor)!");
				}
			}

			template<typename T>
			Array<T> Allocate(size_t count) {
				T* ret = (T*)(m_Data + m_UsedSize);
				if ((m_UsedSize += sizeof(T) * count) > m_MaxSize) {
					printf("stack out of memory (function game::Renderer::Stack::Allocate)!");
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
					printf("stack out of memory (function game::Renderer::Stack::Allocate)!");
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

		static bool GpuSucceeded(VkResult result) {
			return result == VK_SUCCESS;
		}

		enum class CriticalErrorOrigin {
			Vulkan = 1,
			StackOutOfMemory = 2,
			NullDereference = 3,
		};

		const size_t c_SingleThreadStackSize = 65536;
		uint8_t* m_SingleThreadStackData;
		Stack m_SingleThreadStack;

		uint8_t m_InFlightRenderStackData[cexpr_in_flight_render_stack_size];
		Stack m_InFlightRenderStack;

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
		uint32_t m_current_frame = 0;

		uint32_t m_GpuGraphicsQueueFamilyIndex = 0;
		uint32_t m_GpuTransferQueueFamilyIndex = 0;
		uint32_t m_GpuPresentQueueFamilyIndex = 0;

		GLFWwindow* m_Window = nullptr;
		VkCommandPool m_GpuRenderCommandPool = VK_NULL_HANDLE;
		VkSwapchainKHR m_GpuSwapchain = VK_NULL_HANDLE;
		Stack::Array<VkImage> m_GpuSwapchainImages { 0, 0 };
		VkSurfaceFormatKHR m_GpuSwapchainSurfaceFormat{};
		VkPresentModeKHR m_GpuPresentMode{};

		VkInstance m_GpuInstance = VK_NULL_HANDLE;

		void (*m_CriticalErrorCallback)(Renderer* rend, CriticalErrorOrigin origin, const char* err, VkResult vkErr) = nullptr;

		bool VkAssert(VkResult res, const char* err) {
			if (res != VK_SUCCESS) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::Vulkan, err, res);
				return false;
			}
			return true;
		}

		template<typename T>
		T* PtrAssert(T* ptr, const char* err) {
			if (!ptr) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::NullDereference, err, VK_SUCCESS);
			}
			return ptr;
		}

		Renderer(const char* appName, uint32_t appVersion, GLFWwindow* window, void(*criticalErrorCallback)(Renderer*, CriticalErrorOrigin, const char*, VkResult)) 
			: m_SingleThreadStack((uint8_t*)malloc(c_SingleThreadStackSize), c_SingleThreadStackSize), 
				m_InFlightRenderStack(m_InFlightRenderStackData, cexpr_in_flight_render_stack_size), m_Window(window),
				m_CriticalErrorCallback(criticalErrorCallback) {

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
				m_CriticalErrorCallback(this, CriticalErrorOrigin::Vulkan, "couldn't find all the required vulkan instance extensions (in constructor)!", VK_SUCCESS);
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
					printf("Vulkan Khronos validation not supported (in constructor)!");
				}
			}

			VkApplicationInfo appInfo {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pNext = nullptr,
				.pApplicationName = appName,
				.applicationVersion = appVersion,
				.pEngineName = "bones engine",
				.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
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
					"failed to create vulkan instance (function vkCreateInstance)!")) {
				return;
			}

			if (!VkAssert(glfwCreateWindowSurface(m_GpuInstance, m_Window, m_GpuAllocationCallbacks, &m_GpuSurface), 
					"failed to create window surface (function glfwCreateWindowSurface)!")) {
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
				if (!surfaceFormatCount || !presentModeCount || !features.samplerAnisotropy || features.fillModeNonSolid) {
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

			if (bestGpu != VK_NULL_HANDLE) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::Vulkan, "failed to find suitable gpu (in constructor)!", VK_SUCCESS);
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
				.ppEnabledExtensionNames = &cexpr_gpu_dynamic_rendering_extension_name,
				.pEnabledFeatures = &gpuFeatures,
			};

			if (!VkAssert(vkCreateDevice(m_Gpu, &deviceInfo, m_GpuAllocationCallbacks, &m_GpuDevice), "failed to create vulkan device (function vkCreateDevice in constructor)!")) {
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

			if (!VkAssert(vkCreateCommandPool(m_GpuDevice, &renderCommandPoolInfo, m_GpuAllocationCallbacks, &m_GpuRenderCommandPool), 
					"failed to create render command pool (function vkCreateCommandPool in constructor)!")) {
				return;
			}
		}

		void CreateSwapchain() {

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
				m_CriticalErrorCallback(this, CriticalErrorOrigin::Vulkan, "vulkan surface format count was 0 (in function CreateSwapchain)!", VK_SUCCESS);
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
				int width, height;
				glfwGetFramebufferSize(m_Window, &width, &height);
				VkExtent2D actualExtent {
					(uint32_t)width,
					(uint32_t)height
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
					"failed to create vulkan swapchain (function vkCreateSwapchainKHR)!")) {
				return;
			}

			size_t oldFramesInFlight = m_GpuSwapchainImageViews.m_Size;

			for (size_t i = 0; i < oldFramesInFlight; i++) {
				vkDestroyImageView(m_GpuDevice, *m_GpuSwapchainImageViews[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, *m_GpuRenderFinishedSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroySemaphore(m_GpuDevice, *m_GpuRenderWaitSemaphores[i], m_GpuAllocationCallbacks);
				vkDestroyFence(m_GpuDevice, *m_GpuInFlightFences[i], m_GpuAllocationCallbacks);
			}

			m_InFlightRenderStack.Clear();

			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, nullptr);
			if (!m_InFlightRenderStack.Allocate<VkImage>(m_FramesInFlight, &m_GpuSwapchainImages)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}
			vkGetSwapchainImagesKHR(m_GpuDevice, m_GpuSwapchain, &m_FramesInFlight, m_GpuSwapchainImages.m_Data);

			if (!m_InFlightRenderStack.Allocate<VkImageView>(m_FramesInFlight, &m_GpuSwapchainImageViews)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderFinishedSemaphores)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkSemaphore>(m_FramesInFlight, &m_GpuRenderWaitSemaphores)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkFence>(m_FramesInFlight, &m_GpuInFlightFences)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}
			if (!m_InFlightRenderStack.Allocate<VkCommandBuffer>(m_FramesInFlight, &m_GpuRenderCommandBuffers)) {
				m_CriticalErrorCallback(this, CriticalErrorOrigin::StackOutOfMemory, "in flight stack was out of memory (in function CreateSwapchain)!", VK_SUCCESS);
				return;
			}

			VkCommandBufferAllocateInfo renderCommandBufferAllocInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_GpuRenderCommandPool,
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
			}
		}

		struct DrawData {
			VkCommandBuffer commandBuffer;
			VkImageView swapchainImageView;
		};

		bool BeginFrame(DrawData& outDrawData) {
			if (m_GpuSwapchainExtent.width == 0 || m_GpuSwapchainExtent.height == 0) {
				return false;
			}
			return true;
		}

		void EndFrame() {
		}

		void Terminate() {
			vkDestroyCommandPool(m_GpuDevice, m_GpuRenderCommandPool, m_GpuAllocationCallbacks);
			vkDestroyDevice(m_GpuDevice, m_GpuAllocationCallbacks);
			m_GpuDevice = VK_NULL_HANDLE;
			vkDestroySurfaceKHR(m_GpuInstance, m_GpuSurface, m_GpuAllocationCallbacks);
			m_GpuSurface = VK_NULL_HANDLE;
			vkDestroyInstance(m_GpuInstance, m_GpuAllocationCallbacks);
			m_GpuInstance = VK_NULL_HANDLE;
		}

		~Renderer() {
			if (m_GpuInstance != VK_NULL_HANDLE) {
				Terminate();
			}
			free(m_SingleThreadStackData);
		}
	};
}
