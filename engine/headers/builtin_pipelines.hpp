#pragma once

#include "renderer.hpp"

namespace engine {
	class World;
}

namespace pipelines {

	class World {
	public:

		VkPipeline m_DrawPipelinePBR = VK_NULL_HANDLE;
		VkPipelineLayout m_DrawPipelineLayoutPBR = VK_NULL_HANDLE;

		VkPipeline m_DrawPipelineUD{};
		VkPipelineLayout m_DrawPipelineLayoutUD{};

		VkPipeline m_RenderPipelinePBR = VK_NULL_HANDLE;
		VkPipelineLayout m_RenderPipelineLayoutPBR = VK_NULL_HANDLE;

		VkPipeline m_DebugWirePipeline = VK_NULL_HANDLE;
		VkPipeline m_DebugSolidPipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_DebugPipelineLayout = VK_NULL_HANDLE;

		VkDescriptorSetLayout m_DirectionalLightShadowMapDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_TextureDescriptorSetLayoutPBR = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_DebugMouseHitDescriptorSetLayout = VK_NULL_HANDLE;

	private:

		bool m_Initialized = false;

	public:

		bool Initialized() const {
			return m_Initialized;
		}

		void Initialize(engine::Renderer& renderer, VkFormat colorImageResourceFormat);

		void Terminate(engine::Renderer& renderer) {
			renderer.DestroyPipeline(m_DrawPipelinePBR);
			m_DrawPipelinePBR = VK_NULL_HANDLE;
			renderer.DestroyPipelineLayout(m_DrawPipelineLayoutPBR);
			m_DrawPipelineLayoutPBR = VK_NULL_HANDLE;
			renderer.DestroyPipeline(m_DrawPipelineUD);
			m_DrawPipelineUD = VK_NULL_HANDLE;
			renderer.DestroyPipelineLayout(m_DrawPipelineLayoutUD);
			m_DrawPipelineLayoutUD = VK_NULL_HANDLE;
			renderer.DestroyPipeline(m_RenderPipelinePBR);
			m_RenderPipelinePBR = VK_NULL_HANDLE;
			renderer.DestroyPipelineLayout(m_RenderPipelineLayoutPBR);
			m_RenderPipelineLayoutPBR = VK_NULL_HANDLE;
			renderer.DestroyPipeline(m_DebugWirePipeline);
			m_DebugWirePipeline = VK_NULL_HANDLE;
			renderer.DestroyPipeline(m_DebugSolidPipeline);
			m_DebugSolidPipeline = VK_NULL_HANDLE;
			renderer.DestroyPipelineLayout(m_DebugPipelineLayout);
			m_DebugPipelineLayout = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_CameraDescriptorSetLayout);
			m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_RenderPBRImagesDescriptorSetLayout);
			m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_DirectionalLightShadowMapDescriptorSetLayout);
			m_DirectionalLightShadowMapDescriptorSetLayout = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_TextureDescriptorSetLayoutPBR);
			m_TextureDescriptorSetLayoutPBR = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_DebugMouseHitDescriptorSetLayout);
			m_DebugMouseHitDescriptorSetLayout = VK_NULL_HANDLE;
		}
	};
}
