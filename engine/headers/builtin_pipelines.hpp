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

		VkPipeline m_DebugPipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_DebugPipelineLayout = VK_NULL_HANDLE;

		VkDescriptorSetLayout m_DirectionalLightShadowMapDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_SingleTextureDescriptorSetLayoutPBR = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;

		void Initialize(engine::Renderer& renderer, VkFormat colorImageResourceFormat);

		void Terminate(engine::Renderer& renderer) {
			renderer.DestroyDescriptorSetLayout(m_CameraDescriptorSetLayout);
			renderer.DestroyDescriptorSetLayout(m_RenderPBRImagesDescriptorSetLayout);
			renderer.DestroyDescriptorSetLayout(m_DirectionalLightShadowMapDescriptorSetLayout);
			renderer.DestroyDescriptorSetLayout(m_SingleTextureDescriptorSetLayoutPBR);
			renderer.DestroyPipeline(m_DrawPipelinePBR);
			renderer.DestroyPipelineLayout(m_DrawPipelineLayoutPBR);
			renderer.DestroyPipeline(m_DrawPipelineUD);
			renderer.DestroyPipelineLayout(m_DrawPipelineLayoutUD);
			renderer.DestroyPipeline(m_RenderPipelinePBR);
			renderer.DestroyPipelineLayout(m_RenderPipelineLayoutPBR);
			renderer.DestroyPipeline(m_DebugPipeline);
			renderer.DestroyPipelineLayout(m_DebugPipelineLayout);
		}
	};	
}
