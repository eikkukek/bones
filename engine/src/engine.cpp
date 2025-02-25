#include "engine.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

namespace engine {


	void CriticalError(ErrorOrigin origin, const char *err, VkResult vkErr, const char* libErr) {
		fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
			"Engine called a critical error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
		if (vkErr != VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"Vulkan error code: {}\n", (int)vkErr);
		}
		if (libErr) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"Library error message: {}\n", libErr);
		}
		Engine::s_engine_instance->~Engine();
		Engine::s_engine_instance = nullptr;
		fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
		glfwTerminate();
#ifdef DEBUG
		assert(false);
#endif
		exit(EXIT_FAILURE);
	}

	bool LoadImage(const char* fileName, uint32_t components, uint8_t*& outImage, Vec2_T<uint32_t>& outExtent) {
		int x, y, texChannels;
		outImage = stbi_load(fileName, &x, &y, &texChannels, components);
		if (!outImage) {
			PrintError(ErrorOrigin::Stb, 
				"failed to load image (function stbi_load in function LoadImage)!");
			return false;
		}
		outExtent = { (uint32_t)x, (uint32_t)y };
		return true;
	}

	bool Body::UpdateRenderTransform(RenderID ID) {
		Mat4* transform = m_RenderDataTransforms.Find(ID);
		if (!transform) {
			return false;
		}
		WorldRenderData* data = m_Area.m_World.GetRenderData(ID);
		assert(data);
		data->m_Transform = m_Transform * *transform;
		return true;
	}

	void Body::UpdateTransforms() {
		m_Transform = m_Rotation.AsMat4();
		m_Transform[3] = Vec4(m_Position, 1.0f);
		m_Transform[3].x *= -1;
		m_Transform[3].z *= -1;
		uint32_t transformCount = m_RenderDataTransforms.Size();
		uint64_t* keyIter = m_RenderDataTransforms.KeysBegin();
		Mat4* valueIter = m_RenderDataTransforms.ValuesBegin();
		for (uint32_t i = 0; i < transformCount; i++) {
			WorldRenderData* data = m_Area.m_World.GetRenderData(keyIter[i]);
			assert(data);
			data->m_Transform = m_Transform * valueIter[i];
		}
	}

	void PhysicsManager::JoltDebugRendererImpl::DrawGeometry(JPH::RMat44Arg modelMat, const JPH::AABox& worldSpaceBounds, float LODScale,
		JPH::ColorArg modelColor, const GeometryRef& geometry, ECullMode cullMode, ECastShadow castShadow, EDrawMode drawMode) {
		const BatchImpl* batch = static_cast<const BatchImpl*>(geometry->GetLOD(m_World.GetCameraPosition(), worldSpaceBounds, LODScale).mTriangleBatch.GetPtr());
		StaticMesh* mesh = m_Meshes.Find(batch->m_ObjectID);
		if (!mesh) {
			PrintError(ErrorOrigin::Jolt,
				"couldn't find mesh to render (in function PhysicsManager::JoltDebugRenderer::DrawGeometry)!");
			assert(false);
			return;
		}
		if (drawMode == EDrawMode::Wireframe) {
			m_World.RenderWireMesh(mesh->GetMeshData(), modelMat, Vec4(modelColor.r, modelColor.g, modelColor.b, modelColor.a) / 255.0f);
		}
	}

	ObjectID Area::AddBody(const char* name, const Vec3& position, const Quaternion& rotation, PhysicsLayer physicsLayer,
			const Body::ColliderCreateInfo& colliderInfo, const JPH::PhysicsMaterial* physicsMaterial) {
		Body* body = m_Bodies.Emplace(m_World.m_NextObjectID, *this, m_World.m_PhysicsManager, name, position, rotation);
		assert(body);
		body->PhysInitialize(physicsLayer, colliderInfo, physicsMaterial);
		return m_World.m_NextObjectID++;
	}

	bool Area::RemoveBody(ObjectID ID) {
		Body* body = m_Bodies.Find(ID);
		if (body) {
			body->Terminate();
		}
		return m_Bodies.Erase(ID);
	}

	UnidirectionalLight::UnidirectionalLight(World& world, uint64_t objectID, Type type, Vec2_T<uint32_t> shadowMapResolution) 
		: m_World(world), m_ObjectID(objectID), m_ShadowMapResolution(shadowMapResolution), m_Type(type),
			m_FragmentBuffer(world.m_Renderer) {}

	void UnidirectionalLight::Initialize(const Mat4& projection, const Mat4& view, const Vec3& color) {

		assert(m_Type == Type::Directional);

		Renderer& renderer = m_World.m_Renderer;

		uint32_t framesInFlight = renderer.m_FramesInFlight;	

		if (m_DepthImages.Size() != framesInFlight) {
			LockGuard graphicsQueueLockGuard(renderer.m_EarlyGraphicsCommandBufferQueueMutex);
			Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
				= renderer.m_EarlyGraphicsCommandBufferQueue.New();
			if (!commandBuffer) {
				CriticalError(ErrorOrigin::Renderer,
					"renderer graphics command buffer was out of memory (in function UnidirectionalLight::Initialize)!");
			}
			if (!renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(
					renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
					&commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function UnidirectionalLight::Initialize)");
			}
			if (!renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to begin command buffer (function Renderer::BeginCommandBuffer in function UnidirectionalLight::Initialize)");
			}
			SwapchainCreateCallback(framesInFlight, commandBuffer->m_CommandBuffer);
			VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
			if (vkRes != VK_SUCCESS) {
				CriticalError(ErrorOrigin::Vulkan,
					"failed to end command buffer (function vkEndCommandBuffer in function UnidirectionalLight::Initialize)!",
				vkRes);
			}
			commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;
		}

		m_ViewMatrices.m_Projection = projection;
		m_ViewMatrices.m_View = view;

		*(FragmentBufferDirectional*)m_FragmentMap = {
			.m_ViewMatrix = m_ViewMatrices.GetLightViewMatrix(),
			.m_Direction = m_ViewMatrices.GetDirection(),
			.m_Color = color,
		};
	}

	void UnidirectionalLight::Terminate() {
		Renderer& renderer = m_World.m_Renderer;
		for (uint32_t i = 0; i < m_DepthImages.Size(); i++) {
			renderer.DestroyImageView(m_DepthImageViews[i]);
			renderer.DestroyImage(m_DepthImages[i]);
			renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
		}
		renderer.DestroyDescriptorPool(m_ShadowMapDescriptorPool);
		renderer.DestroySampler(m_ShadowMapSampler);
		m_FragmentBuffer.Terminate();
	}

	void UnidirectionalLight::SwapchainCreateCallback(uint32_t imageCount, VkCommandBuffer commandBuffer) {

		Renderer& renderer = m_World.m_Renderer;

		if (m_FragmentBuffer.IsNull()) {
			if (!m_FragmentBuffer.Create(GetFragmentBufferSize(),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create buffer for directional light (function Renderer::Buffer::Create in function UnidirectionalLight::SwapchainCreateCallback)!");
			}
			if (!m_FragmentBuffer.MapMemory(0, m_FragmentBuffer.m_BufferSize, (void**)&m_FragmentMap)) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to map buffer memory for directional light (function Renderer::Buffer::MapMemory in function UnidirectionalLight::SwapchainCreateCallback)!");
			}
		}

		if (m_DepthImageViews.Size() != imageCount) {
			if (m_DepthImages.Size() < imageCount) {

				size_t oldImageCount = m_DepthImages.Size();

				m_DepthImages.Resize(imageCount);
				m_DepthImagesMemory.Resize(imageCount);
				m_DepthImageViews.Resize(imageCount);

				VkExtent3D extent = { m_ShadowMapResolution.x, m_ShadowMapResolution.y, 1 };

				for (size_t i = oldImageCount; i < imageCount; i++) {
					VkImage& image = m_DepthImages[i];
					VkDeviceMemory& memory = m_DepthImagesMemory[i];
					VkImageView& imageView = m_DepthImageViews[i];
					image = renderer.CreateImage(VK_IMAGE_TYPE_2D, renderer.m_DepthOnlyFormat, 
						extent, 1, 1, VK_SAMPLE_COUNT_1_BIT, 
						VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
						VK_SHARING_MODE_EXCLUSIVE, 1, &renderer.m_GraphicsQueueFamilyIndex);
					if (image == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create depth image for directional light (function Renderer::CreateImage in function UnidirectionalLight::SwapchainCreateCallback)!");
					}
					memory = renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					if (memory == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to allocate depth image memory for directional light (function Renderer::AllocateImageMemory in function UnidirectionalLight::SwapchainCreateCallback)!");
					}
					imageView = renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, renderer.m_DepthOnlyFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
					if (imageView == VK_NULL_HANDLE) {
						CriticalError(ErrorOrigin::Renderer, 
							"failed to create depth image view for directional light (function Renderer::AllocateImageMemory in function UnidirectionalLight::SwapchainCreateCallback)!");
					}

					VkImageMemoryBarrier memoryBarrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.pNext = nullptr,
						.srcAccessMask = 0,
						.dstAccessMask = 0,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
						.image = image,
						.subresourceRange {
							.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1,
						},
					};

					vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
				}	
			}
			else {
				for (size_t i = imageCount; i < m_DepthImages.Size(); i++) {
					renderer.DestroyImageView(m_DepthImageViews[i]);
					renderer.DestroyImage(m_DepthImages[i]);
					renderer.FreeVulkanDeviceMemory(m_DepthImagesMemory[i]);
				}
				m_DepthImages.Resize(imageCount);
				m_DepthImagesMemory.Resize(imageCount);
				m_DepthImageViews.Resize(imageCount);
			}
			if (m_ShadowMapSampler == VK_NULL_HANDLE) {
				VkSamplerCreateInfo samplerInfo = Renderer::GetDefaultSamplerInfo();
				samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
				m_ShadowMapSampler = renderer.CreateSampler(samplerInfo);
				if (m_ShadowMapSampler == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create shadow map sampler for directional light (function Renderer::CreateSampler in function UnidirectionalLight::SwapchainCreateCallback)!");
				}
			}
			if (m_ShadowMapDescriptorPool != VK_NULL_HANDLE) {
				renderer.DestroyDescriptorPool(m_ShadowMapDescriptorPool);
			}
			DynamicArray<VkDescriptorPoolSize> poolSizes(2 * imageCount);
			for (uint32_t i = 0; i < poolSizes.Size();) {
				poolSizes[i++] = {
					.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
				};
				poolSizes[i++] = {
					.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
				};
			}
			m_ShadowMapDescriptorPool = renderer.CreateDescriptorPool(0, imageCount, poolSizes.Size(), poolSizes.Data());
			if (m_ShadowMapDescriptorPool == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create descriptor pool for directional light (function Renderer::CreateDescriptorPool in function UnidirectionalLight::SwapchainCreateCallback)!");
			}
			m_ShadowMapDescriptorSets.Resize(imageCount);
			DynamicArray<VkDescriptorSetLayout> setLayouts(imageCount);
			for (VkDescriptorSetLayout& layout : setLayouts) {
				layout = m_World.m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout;
			}
			if (!renderer.AllocateDescriptorSets(nullptr, m_ShadowMapDescriptorPool, imageCount, 
					setLayouts.Data(), m_ShadowMapDescriptorSets.Data())) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to allocate descriptor sets for directional light (function Renderer::AllocateDescriptorSets in function UnidirectionalLight::SwapchainCreateCallback)!");
			}
			VkDescriptorBufferInfo descriptorBufferInfo {
				.buffer = m_FragmentBuffer.m_Buffer,
				.offset = 0,
				.range = GetFragmentBufferSize(),
			};
			for (uint32_t i = 0; i < imageCount; i++) {
				VkDescriptorImageInfo imageInfo {
					.sampler = m_ShadowMapSampler,
					.imageView = m_DepthImageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				};
				VkWriteDescriptorSet descriptorWrites[2] {
					Renderer::GetDescriptorWrite(nullptr, 0, m_ShadowMapDescriptorSets[i], 
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, nullptr),
					Renderer::GetDescriptorWrite(nullptr, 1, m_ShadowMapDescriptorSets[i], 
						VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &descriptorBufferInfo),
				};
				renderer.UpdateDescriptorSets(2, descriptorWrites);
			}
		}
	}

	void UnidirectionalLight::DepthDraw(const Renderer::DrawData& drawData) const {

		VkImageMemoryBarrier memoryBarrier1 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = 0,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = m_DepthImages[drawData.m_CurrentFrame],
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
			0, 0, nullptr, 0, nullptr, 1, &memoryBarrier1);

		VkExtent2D extent { m_ShadowMapResolution.x, m_ShadowMapResolution.y };

		VkRect2D scissor {
			.offset = { 0, 0 },
			.extent = extent,
		};

		VkViewport viewport {
			.x = 0,
			.y = 0,
			.width = (float)extent.width,
			.height = (float)extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
		vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);

		VkRenderingAttachmentInfo depthAttachment {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = m_DepthImageViews[drawData.m_CurrentFrame],
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue { .depthStencil { .depth = 1.0f, .stencil = 0 } },
		};

		VkRenderingInfo renderingInfo {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = { .offset { 0, 0 }, .extent { extent }, },
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

		const pipelines::World& pipelines = m_World.m_Pipelines;

		vkCmdBeginRendering(drawData.m_CommandBuffer, &renderingInfo);
		vkCmdBindPipeline(drawData.m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.m_DrawPipelineUD);
		Mat4 matrices[2] {
			m_ViewMatrices.GetLightViewMatrix(),
			{},
		};
		for (const WorldRenderData& renderData : m_World.m_RenderDatas) {
			matrices[1] = renderData.m_Transform;
			matrices[1][3].y *= -1;
			vkCmdPushConstants(drawData.m_CommandBuffer, pipelines.m_DrawPipelineLayoutUD, VK_SHADER_STAGE_VERTEX_BIT, 0, 128, matrices);
			vkCmdBindVertexBuffers(drawData.m_CommandBuffer, 0, 1, renderData.m_MeshData.m_VertexBuffers, renderData.m_MeshData.m_VertexBufferOffsets);
			vkCmdBindIndexBuffer(drawData.m_CommandBuffer, renderData.m_MeshData.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawData.m_CommandBuffer, renderData.m_MeshData.m_IndexCount, 1, 0, 0, 0);
		}	
		vkCmdEndRendering(drawData.m_CommandBuffer);

		scissor.extent = drawData.m_SwapchainExtent;
		viewport.width = (float)drawData.m_SwapchainExtent.width;
		viewport.height = (float)drawData.m_SwapchainExtent.height;
		vkCmdSetScissor(drawData.m_CommandBuffer, 0, 1, &scissor);
		vkCmdSetViewport(drawData.m_CommandBuffer, 0, 1, &viewport);

		VkImageMemoryBarrier memoryBarrier2 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = 0,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = m_DepthImages[drawData.m_CurrentFrame],
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		vkCmdPipelineBarrier(drawData.m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
			0, 0, nullptr, 0, nullptr, 1, &memoryBarrier2);
	}

	void World::Initialize(const StaticMesh& quadMesh2D) {

		m_StaticQuadMeshDataPBR = quadMesh2D.GetMeshData();

		m_Pipelines.Initialize(m_Renderer, m_ColorImageResourcesFormat);

		if (!m_CameraMatricesBuffer.Create(sizeof(CameraMatricesBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create camera matrices buffer (function Renderer::Buffer::Create in function World::Initialize)!");
		}

		VkResult vkRes = vkMapMemory(m_Renderer.m_VulkanDevice, m_CameraMatricesBuffer.m_VulkanDeviceMemory, 0, 
			sizeof(CameraMatricesBuffer), 0, (void**)&m_CameraMatricesMap);
		if (vkRes != VK_SUCCESS) {
			CriticalError(ErrorOrigin::Vulkan, 
				"failed to map camera matrices buffer (function vkMapMemory in function World::Initialize)!");
		}

		m_EditorCamera.m_Projection = Mat4::Projection(default_camera_fov,
			(float)m_Renderer.m_SwapchainExtent.width / m_Renderer.m_SwapchainExtent.height, default_camera_near, default_camera_far);
		m_EditorCamera.m_View = Mat4(1);

		m_GameCamera = m_EditorCamera;

		VkDescriptorPoolSize camPoolSize {
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
		};

		m_CameraMatricesDescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, 1, &camPoolSize);

		if (m_CameraMatricesDescriptorPool == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create camera matrices descriptor pool (function Renderer::CreateDescriptorPool in function World::Initialize)");
		}

		if (!m_Renderer.AllocateDescriptorSets(nullptr, m_CameraMatricesDescriptorPool, 1, 
				&m_Pipelines.m_CameraDescriptorSetLayout, &m_CameraMatricesDescriptorSet)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to allocate camera matrices descriptor set (function Renderer::AllocateDescriptorSets in function World::Initialize)!");
		}

		VkDescriptorBufferInfo cameraDecriptorBufferInfo {
			.buffer = m_CameraMatricesBuffer.m_Buffer,
			.offset = 0,
			.range = sizeof(CameraMatricesBuffer),
		};

		VkWriteDescriptorSet cameraDescriptorSetWrite = Renderer::GetDescriptorWrite(nullptr, 0, m_CameraMatricesDescriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &cameraDecriptorBufferInfo);

		m_Renderer.UpdateDescriptorSets(1, &cameraDescriptorSetWrite);

		m_DirectionalLight.Initialize(
			Mat4::Orthogonal(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f),
			Mat4::LookAt(Vec3(10.0f, 10.0f, 2.0f), Vec3::Up(), Vec3(0.0f, 0.0f, 0.0f)),
			Vec3(201.0f / 255.0f, 226.0f / 255.0f, 255.0f / 255.0f));

		static constexpr uint32_t default_texture_count = 1;

		static constexpr VkDescriptorPoolSize default_textures_pool_sizes[default_texture_count] {
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
			},
		};

		m_DefaultTextureDescriptorPool = m_Renderer.CreateDescriptorPool(0, 1, default_texture_count, default_textures_pool_sizes);

		if (!m_Renderer.AllocateDescriptorSets(nullptr, m_DefaultTextureDescriptorPool,
				1, &m_Pipelines.m_TextureDescriptorSetLayoutPBR, &m_DefaultAlbedoDescriptorSet)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to allocate default texture descriptor sets for world (function Renderer::AllocateDescriptorSets in function World::Initialize)!");
		}

		if (m_DefaultTextureDescriptorPool == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create default texture descriptor pool for world (function Renderer::CreateDescriptorPool in function World::Initialize)!");
		}

		static constexpr uint32_t default_albedo_pixel = PackColorRBGA({ 242.0f / 255.0f, 15.0f / 255.0f, 204.0f / 255.0f, 1.0f });
		static constexpr Vec2_T<uint32_t> default_albedo_extent = { 64U, 64U };
		static constexpr size_t default_albedo_pixel_count = default_albedo_extent.x * default_albedo_extent.y;

		uint32_t* const defaultAlbedoImage = (uint32_t*)malloc(sizeof(uint32_t) * default_albedo_pixel_count);

		assert(defaultAlbedoImage);

		for (size_t i = 0; i < default_albedo_pixel_count; i++) {
			defaultAlbedoImage[i] = default_albedo_pixel;
		}

		if (!m_DefaultAlbedoTexture.Create(VK_FORMAT_R8G8B8A8_SRGB, default_albedo_extent, defaultAlbedoImage)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create default albedo texture for world(function Texture::Create in function World::Initialize)!");
		}

		free(defaultAlbedoImage);

		m_DefaultAlbedoImageView = m_DefaultAlbedoTexture.CreateImageView();

		if (m_DefaultAlbedoImageView == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create default albedo image view for world (function Texture::CreateImageView in function World::Initialize)");
		}

		const VkDescriptorImageInfo defaultAlbedoImageInfo {
			.sampler = m_ColorResourceImageSampler,
			.imageView = m_DefaultAlbedoImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		const VkWriteDescriptorSet defaultAlbedoDescriptorWrite = Renderer::GetDescriptorWrite(nullptr, 0, m_DefaultAlbedoDescriptorSet, 
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &defaultAlbedoImageInfo, nullptr);

		m_Renderer.UpdateDescriptorSets(1, &defaultAlbedoDescriptorWrite);
	}

	void World::SwapchainCreateCallback(VkExtent2D swapchainExtent, Vec2_T<uint32_t> renderResolution, float aspectRatio, uint32_t imageCount) {

		m_RenderResolution = renderResolution;

		m_EditorCamera.m_Projection
				= Mat4::Projection(default_camera_fov, aspectRatio, default_camera_near, default_camera_far);
		m_GameCamera = m_EditorCamera;

		if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
			VkFormat colorImageResourcesFormatCandidates[2] = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_B8G8R8A8_SRGB };
			m_ColorImageResourcesFormat = m_Renderer.FindSupportedFormat(1, colorImageResourcesFormatCandidates, 
				VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
			if (m_ColorImageResourcesFormat == VK_FORMAT_UNDEFINED) {
				CriticalError(ErrorOrigin::Renderer, 
					"couldn't find suitable format for color image resources (function Renderer::FindSupportedFormat in function World::SwapchainCreateCallback)!");
			}
		}

		static constexpr uint32_t descriptor_count = 3;

		if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {

			VkDescriptorSetLayoutBinding imageSamplerDescriptorSetBinding {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			};

			VkDescriptorSetLayoutBinding pbrRenderPipelineDescriptorSetBindings[descriptor_count] {
				imageSamplerDescriptorSetBinding,
				imageSamplerDescriptorSetBinding,
				imageSamplerDescriptorSetBinding,
			};

			for (uint32_t i = 1; i < descriptor_count; i++) {
				pbrRenderPipelineDescriptorSetBindings[i].binding = i;
			}

			m_Pipelines.m_RenderPBRImagesDescriptorSetLayout 
				= m_Renderer.CreateDescriptorSetLayout(nullptr, descriptor_count, pbrRenderPipelineDescriptorSetBindings);

			if (m_Pipelines.m_RenderPBRImagesDescriptorSetLayout == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create pbr render pipeline samplers descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::SwapchainCreateCallback)!");
			}
		}

		if (m_ColorResourceImageSampler == VK_NULL_HANDLE) {
			m_ColorResourceImageSampler = m_Renderer.CreateSampler(m_Renderer.GetDefaultSamplerInfo());
			if (m_ColorResourceImageSampler == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create color resource image sampler for world (function Renderer::CreateSampler in function World::SwapchainCreateCallback)!");
			}
		}

		if (m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout == VK_NULL_HANDLE) {
			
			static constexpr VkDescriptorSetLayoutBinding dir_light_shadow_map_descriptor_set_layout_bindings[2] {
				Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
				Renderer::GetDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
			};

			m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout 
				= m_Renderer.CreateDescriptorSetLayout(nullptr, 2, dir_light_shadow_map_descriptor_set_layout_bindings);

			if (m_Pipelines.m_DirectionalLightShadowMapDescriptorSetLayout == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create directional light descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function World::Initialize)!");
			}
		}

		m_Renderer.DestroyDescriptorPool(m_RenderPBRImagesDescriptorPool);
		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
		};
		DynamicArray<VkDescriptorPoolSize> poolSizes(descriptor_count * imageCount);
		for (VkDescriptorPoolSize& size : poolSizes) {
			size = poolSize;
		}
		m_RenderPBRImagesDescriptorPool = m_Renderer.CreateDescriptorPool(0, imageCount, poolSizes.Size(), poolSizes.Data());

		if (m_RenderPBRImagesDescriptorPool == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create pbr render pipeline image descriptor pool (function Renderer::CreateDescriptorPool in function World::SwapchainCreateCallback)!");
		}

		m_RenderPBRImagesDescriptorSets.Resize(imageCount);

		DynamicArray<VkDescriptorSetLayout> setLayouts(imageCount);

		for (VkDescriptorSetLayout& set : setLayouts) {
			set = m_Pipelines.m_RenderPBRImagesDescriptorSetLayout;
		}

		if (!m_Renderer.AllocateDescriptorSets(nullptr, m_RenderPBRImagesDescriptorPool, imageCount, 
				setLayouts.Data(), m_RenderPBRImagesDescriptorSets.Data())) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to allocate pbr rendering pipeline image descriptor sets (function Renderer::AllocateDescriptorSets in function World::SwapchainCreateCallback)!");
		}

		DestroyImageResources();
		m_DiffuseImageViews.Resize(imageCount);
		m_PositionAndMetallicImageViews.Resize(imageCount);
		m_NormalAndRougnessImageViews.Resize(imageCount);
		m_DepthImageViews.Resize(imageCount);
		m_DiffuseImages.Resize(imageCount);
		m_PositionAndMetallicImages.Resize(imageCount);
		m_NormalAndRougnessImages.Resize(imageCount);
		m_DepthImages.Resize(imageCount);
		m_DiffuseImagesMemory.Resize(imageCount);
		m_PositionAndMetallicImagesMemory.Resize(imageCount);
		m_NormalAndRougnessImagesMemory.Resize(imageCount);
		m_DepthImagesMemory.Resize(imageCount);

		LockGuard graphicsQueueLockGuard(m_Renderer.m_EarlyGraphicsCommandBufferQueueMutex);
		Renderer::CommandBuffer<Renderer::Queue::Graphics>* commandBuffer
			= m_Renderer.m_EarlyGraphicsCommandBufferQueue.New();
		if (!commandBuffer) {
			CriticalError(ErrorOrigin::Renderer,
				"renderer graphics command buffer was out of memory (in function World::SwapchainCreateCallback)!");
		}
		if (!m_Renderer.AllocateCommandBuffers(Renderer::GetDefaultCommandBufferAllocateInfo(
				m_Renderer.GetCommandPool<Renderer::Queue::Graphics>(), 1), 
				&commandBuffer->m_CommandBuffer)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to allocate command buffer (function Renderer::AllocateCommandBuffers in function World::SwapchainCreateCallback)");
		}
		if (!m_Renderer.BeginCommandBuffer(commandBuffer->m_CommandBuffer)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to begin command buffer (function Renderer::BeginCommandBuffer in function World::SwapchainCreateCallback)");
		}

		VkFormat depthFormat = m_Renderer.m_DepthOnlyFormat;
		VkExtent3D imageExtent {
			.width = m_RenderResolution.x,
			.height = m_RenderResolution.y,
			.depth = 1,
		};
		uint32_t colorImageQueueFamilies[1] { m_Renderer.m_GraphicsQueueFamilyIndex, };
		uint32_t colorImageQueueFamilyCount = 1;
		VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; 
		VkSharingMode colorImageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		for (uint32_t i = 0; i < imageCount; i++) {
			{
				VkImage& image = m_DiffuseImages[i];
				VkDeviceMemory& imageMemory = m_DiffuseImagesMemory[i];
				VkImageView& imageView = m_DiffuseImageViews[i];
				image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
					VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
					colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
				if (image == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world diffuse image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
				}
				imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				if (imageMemory == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate world diffuse image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
				}
				imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				if (imageView == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world diffuse image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
				}
			}
			{
				VkImage& image = m_PositionAndMetallicImages[i];
				VkDeviceMemory& imageMemory = m_PositionAndMetallicImagesMemory[i];
				VkImageView& imageView = m_PositionAndMetallicImageViews[i];
				image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
					VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
					colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
				if (image == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create position/metallic image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
				}
				imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				if (imageMemory == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate position/metallic image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)");
				}
				imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				if (imageView == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world position/metallic image view (function Renderer::CreateImageView in function World::Initialize)");
				}
			}
			{
				VkImage& image = m_NormalAndRougnessImages[i];
				VkDeviceMemory& imageMemory = m_NormalAndRougnessImagesMemory[i];
				VkImageView& imageView = m_NormalAndRougnessImageViews[i];
				image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, m_ColorImageResourcesFormat, imageExtent, 1, 1, 
					VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, colorImageUsage, 
					colorImageSharingMode, colorImageQueueFamilyCount, colorImageQueueFamilies);
				if (image == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create normal/roughness image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
				}
				imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				if (imageMemory == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate normal/roughness image memory (function Renderer::AllocateImageMemory in function World::Initialize)");
				}
				imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, m_ColorImageResourcesFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				if (imageView == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world normal/roughness image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)");
				}
			}
			{
				VkImage& image = m_DepthImages[i];
				VkDeviceMemory& imageMemory = m_DepthImagesMemory[i];
				VkImageView& imageView = m_DepthImageViews[i];
				image = m_Renderer.CreateImage(VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1, 
					VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE, 1, &m_Renderer.m_GraphicsQueueFamilyIndex);
				if (image == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world depth image (function Renderer::CreateImage in function World::SwapchainCreateCallback)!");
				}
				imageMemory = m_Renderer.AllocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				if (imageMemory == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to allocate world depth image memory (function Renderer::AllocateImageMemory in function World::SwapchainCreateCallback)!");
				}
				imageView = m_Renderer.CreateImageView(image, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
				if (imageView == VK_NULL_HANDLE) {
					CriticalError(ErrorOrigin::Renderer, 
						"failed to create world depth image view (function Renderer::CreateImageView in function World::SwapchainCreateCallback)!");
				}
			}

			VkDescriptorImageInfo descriptorImageInfos[descriptor_count] {
				{
					.sampler = m_ColorResourceImageSampler,
					.imageView = m_DiffuseImageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				{
					.sampler = m_ColorResourceImageSampler,
					.imageView = m_PositionAndMetallicImageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				{
					.sampler = m_ColorResourceImageSampler,
					.imageView = m_NormalAndRougnessImageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
			};

			VkWriteDescriptorSet descriptorWrites[descriptor_count];
			for (uint32_t j = 0; j < descriptor_count; j++) {
				descriptorWrites[j] 
					= Renderer::GetDescriptorWrite(nullptr, j, m_RenderPBRImagesDescriptorSets[i], 
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfos[j], nullptr);
			}

			m_Renderer.UpdateDescriptorSets(descriptor_count, descriptorWrites);

			static constexpr uint32_t image_count = descriptor_count;

			VkImage colorImages[image_count] {
				m_DiffuseImages[i],
				m_PositionAndMetallicImages[i],
				m_NormalAndRougnessImages[i],	
			};	

			VkImageMemoryBarrier memoryBarriers[image_count];

			for (size_t j = 0; j < image_count; j++) {
				memoryBarriers[j] = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.pNext = nullptr,
					.srcAccessMask = 0,
					.dstAccessMask = 0,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = colorImages[j],
					.subresourceRange {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				};
			}
			vkCmdPipelineBarrier(commandBuffer->m_CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, image_count, memoryBarriers);
		}
		m_DirectionalLight.SwapchainCreateCallback(imageCount, commandBuffer->m_CommandBuffer);
		VkResult vkRes = vkEndCommandBuffer(commandBuffer->m_CommandBuffer);
		if (vkRes != VK_SUCCESS) {
			CriticalError(ErrorOrigin::Vulkan, 
				"failed to end command buffer (function vkEndCommandBuffer in function World::SwapchainCreateCallback)!", 
				vkRes);
		}
		commandBuffer->m_Flags = Renderer::CommandBufferFlag_FreeAfterSubmit;
	}

	void Editor::SwapchainCreateCallback(VkExtent2D extent, uint32_t imageCount) {

		assert(imageCount < 5);

		if (m_Pipelines.m_QuadTransformDescriptorSetLayoutSDF == VK_NULL_HANDLE) {

			VkDescriptorSetLayoutBinding binding { 
				m_Renderer.GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
			};

			m_Pipelines.m_QuadTransformDescriptorSetLayoutSDF = m_Renderer.CreateDescriptorSetLayout(nullptr, 1, &binding);

			if (m_Pipelines.m_QuadTransformDescriptorSetLayoutSDF == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create SDF transform descriptor set layout (function Renderer::CreateDescriptorSetLayout in function Editor::SwapchainCreateCallback)!");
			}

			if (!m_QuadTransformBufferSDF.Create(64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create debug render transform buffer (function Renderer::Buffer::Create in function Editor::SwapchainCreateCallback)!");
			}
			if (!m_QuadTransformBufferSDF.MapMemory(0, 64, (void**)&m_QuadTransformBufferMapSDF)) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to map debug render transform buffer (function Renderer::Buffer::MapMemory in function Editor::SwapchainCreateCallback)");
			}
			VkDescriptorPoolSize poolSize {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
			};
			m_QuadTransformBufferDescriptorPoolSDF = m_Renderer.CreateDescriptorPool(0, 1, 1, &poolSize);
			if (m_QuadTransformBufferDescriptorPoolSDF  == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create debug render transform descriptor pool (function Renderer::CreateDescriptorPool in function Editor::SwapchainCreateCallback)!");
			}
			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_QuadTransformBufferDescriptorPoolSDF, 1, 
					&m_Pipelines.m_QuadTransformDescriptorSetLayoutSDF, &m_QuadTransformDescriptorSetSDF)) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to allocate debug render transform descriptor set (function Renderer::AllocateDescriptorSets in function Editor::SwapchainCreateCallback)!");
			}
			VkDescriptorBufferInfo bufferInfo {
				.buffer = m_QuadTransformBufferSDF.m_Buffer,
				.offset = 0,
				.range = 64,
			};
			VkWriteDescriptorSet write = Renderer::GetDescriptorWrite(nullptr, 0, m_QuadTransformDescriptorSetSDF,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufferInfo);
			m_Renderer.UpdateDescriptorSets(1, &write);
		}

		*m_QuadTransformBufferMapSDF = Mat4(1.0f);

		if (m_Pipelines.m_MouseHitDescriptorSetLayoutSDF == VK_NULL_HANDLE) {
			VkDescriptorSetLayoutBinding bindings[1] {
				m_Renderer.GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
			};

			m_Pipelines.m_MouseHitDescriptorSetLayoutSDF = m_Renderer.CreateDescriptorSetLayout(nullptr, 1, bindings);

			if (m_Pipelines.m_MouseHitDescriptorSetLayoutSDF == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer, 
					"failed to create SDF mouse hit image descriptor set layout (funtion Renderer::CreateDescriptorSetLayout in function Editor::SwapchainCreateCallback)");
			}
		}


		if (m_LastImageCount != imageCount) {
			for (uint32_t i = 0; i < 5; i++) {
				m_MouseHitBuffersSDF[i].Terminate();
			}
			m_Renderer.DestroyDescriptorPool(m_MouseHitBufferDescriptorPoolSDF);
			m_MouseHitBufferDescriptorPoolSDF = VK_NULL_HANDLE;
			VkDescriptorPoolSize poolSizes[5];
			VkDescriptorBufferInfo bufferInfos[5];
			VkDescriptorSetLayout setLayouts[5];
			for (uint32_t i = 0; i < imageCount; i++) {
				if (!m_MouseHitBuffersSDF[i].Create(4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to create editor rotator storage buffers (function Renderer::Buffer::Create in function Editor::SwapchainCreateCallback)!");
				}
				if (!m_MouseHitBuffersSDF[i].MapMemory(0, 4, (void**)&m_MouseHitBufferMapsSDF[i])) {
					CriticalError(ErrorOrigin::Renderer,
						"failed to map editor rotator storage buffers (function Renderer::Buffer::MapMemory in function Editor::SwapchainCreateCallback)!");
				}
				poolSizes[i] = {
					.type =VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.descriptorCount = 1,
				};
				bufferInfos[i] = {
					.buffer = m_MouseHitBuffersSDF[i].m_Buffer,
					.offset = 0,
					.range = 4,
				};
				setLayouts[i] = m_Pipelines.m_MouseHitDescriptorSetLayoutSDF;
			}
			m_MouseHitBufferDescriptorPoolSDF = m_Renderer.CreateDescriptorPool(0, imageCount, imageCount, poolSizes);
			if (m_MouseHitBufferDescriptorPoolSDF  == VK_NULL_HANDLE) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to create rotator storage buffer descriptor pool (function Renderer::CreateDescriptorPool in function Editor:.SwapchainCreateCallback)!");
			}
			if (!m_Renderer.AllocateDescriptorSets(nullptr, m_MouseHitBufferDescriptorPoolSDF, imageCount, setLayouts,
					m_MouseHitDescriptorSetsSDF)) {
				CriticalError(ErrorOrigin::Renderer,
					"failed to allocate rotator storage buffer descriptor sets (function Renderer::AllocateDescriptorSets in function Editor:.SwapchainCreateCallback)!");
			}
			VkWriteDescriptorSet writes[5]{};
			for (uint32_t i = 0; i < imageCount; i++) {
				writes[i] = Renderer::GetDescriptorWrite(nullptr, 0, m_MouseHitDescriptorSetsSDF[i],
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfos[i]);
			}
			m_Renderer.UpdateDescriptorSets(imageCount, writes);
		}
		m_LastImageCount = imageCount;
	}
}
