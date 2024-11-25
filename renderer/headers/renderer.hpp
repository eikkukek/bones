#pragma once

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"
#include "vulkan/vulkan_core.h"
#include "assert.h"
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

		static constexpr uint32_t cexpr_frames_in_flight = 2;
		static constexpr const char* cexpr_gpu_validation_layer_name = "VK_LAYER_KHRONOS_validation";
		static constexpr const char* cexpr_gpu_dynamic_rendering_extension_name = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;

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
			};

			uint8_t* m_Data;
			const size_t m_MaxSize;
			size_t m_UsedSize;

			Stack(size_t maxSize) 
				: m_Data((uint8_t*)malloc(maxSize)), m_MaxSize(m_Data ? maxSize : 0), m_UsedSize(0) {
				if (!m_Data) {
					printf("failed to allocate stack (game::Renderer::Stack constructor)!");
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
			void Deallocate(size_t count) {
				size_t size = sizeof(T) * count;
				m_UsedSize -= size < m_UsedSize ? size : m_UsedSize;
			}

			void Clear() {
				m_UsedSize = 0;
			}

			~Stack() {
				free(m_Data);
			}
		};

		static bool GpuSucceeded(VkResult result) {
			return result == VK_SUCCESS;
		}

		VkDevice m_GpuDevice = VK_NULL_HANDLE;
		VkAllocationCallbacks* m_GpuAllocationCallbacks = nullptr;
		VkPhysicalDevice m_Gpu = VK_NULL_HANDLE;
		VkSampleCountFlags m_GpuColorMsaaSamples = 1;
		VkSampleCountFlags m_GpuDepthMsaaSamples = 1;

		VkCommandBuffer m_GpuRenderCommandBuffers[cexpr_frames_in_flight];
		VkSemaphore m_GpuRenderFinishedSemaphores[cexpr_frames_in_flight];
		VkSemaphore m_GpuRenderWaitSemaphores[cexpr_frames_in_flight];
		VkFence m_GpuInFlightFences[cexpr_frames_in_flight];
		VkImageView m_GpuSwapchainImageViews[cexpr_frames_in_flight];
		VkQueue m_GpuGraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_GpuTransferQueue = VK_NULL_HANDLE;
		VkQueue m_GpuPresentQueue = VK_NULL_HANDLE;
		VkExtent2D m_GpuSwapchainExtent = { 0, 0 };
		uint32_t m_current_frame = 0;

		uint32_t m_GpuGraphicsQueueFamilyIndex = 0;
		uint32_t m_GpuTransferQueueFamilyIndex = 0;
		uint32_t m_GpuPresentQueueFamilyIndex = 0;

		VkCommandPool m_GpuRenderCommandPool{};
		VkSwapchainKHR m_GpuSwapchain = VK_NULL_HANDLE;

		VkInstance m_GpuInstance = VK_NULL_HANDLE;
		VkSurfaceKHR m_GpuSurface = VK_NULL_HANDLE;

		const size_t c_SingleThreadStackSize = 65536;
		Stack m_SingleThreadStack;

		Renderer(const char* appName, uint32_t appVersion, GLFWwindow* window) : m_SingleThreadStack(c_SingleThreadStackSize) {

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

			assert(!instanceExtensionsNotFoundCount && "couldn't find all required gpu instance extensions!");

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
					printf("Vulkan Khronos validation not supported (game::Renderer constructor)!");
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

			assert(GpuSucceeded(vkCreateInstance(&instanceCreateInfo, m_GpuAllocationCallbacks, &m_GpuInstance)) && "failed to create gpu instance!");
			assert(GpuSucceeded(glfwCreateWindowSurface(m_GpuInstance, window, m_GpuAllocationCallbacks, &m_GpuSurface)) && "failed to create window surface!");

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

			assert(bestGpu != VK_NULL_HANDLE && "couldn't find suitable gpu!");

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

			assert(GpuSucceeded(vkCreateDevice(m_Gpu, &deviceInfo, m_GpuAllocationCallbacks, &m_GpuDevice)) && "failed to create gpu device!");

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

			assert(GpuSucceeded(vkCreateCommandPool(m_GpuDevice, &renderCommandPoolInfo, m_GpuAllocationCallbacks, &m_GpuRenderCommandPool)) && "failed to create render command pool!");

			VkCommandBufferAllocateInfo renderCommandBuffersAllocInfo {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = m_GpuRenderCommandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = cexpr_frames_in_flight,
			};

			assert(GpuSucceeded(vkAllocateCommandBuffers(m_GpuDevice, &renderCommandBuffersAllocInfo, m_GpuRenderCommandBuffers)) && "failed to create render command buffers!");

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

			for (size_t i = 0; i < cexpr_frames_in_flight; i++) {
				assert(GpuSucceeded(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuRenderFinishedSemaphores[i])) && "failed to create render finished semaphore!");
				assert(GpuSucceeded(vkCreateSemaphore(m_GpuDevice, &semaphoreInfo, m_GpuAllocationCallbacks, &m_GpuRenderWaitSemaphores[i])) && "failed to crate render wait semaphore!");
				assert(GpuSucceeded(vkCreateFence(m_GpuDevice, &fenceInfo, m_GpuAllocationCallbacks, &m_GpuInFlightFences[i])) && "failed to create in flight fence!");
			}
		}

		void BeginFrame() {
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
		}
	};
}
