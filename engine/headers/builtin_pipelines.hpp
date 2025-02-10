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

		VkPipeline m_WirePipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_DebugPipelineLayout = VK_NULL_HANDLE;

		VkDescriptorSetLayout m_DirectionalLightShadowMapDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_CameraDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_TextureDescriptorSetLayoutPBR = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_RenderPBRImagesDescriptorSetLayout = VK_NULL_HANDLE;

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
			renderer.DestroyPipeline(m_WirePipeline);
			m_WirePipeline = VK_NULL_HANDLE;
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
		}
	};

	class Editor {
	public:
	
		VkPipeline m_PipelineSDF = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayoutSDF = VK_NULL_HANDLE;

		VkDescriptorSetLayout m_QuadTransformDescriptorSetLayoutSDF = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_RotatorInfoDescriptorSetLayoutSDF = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_MouseHitDescriptorSetLayoutSDF = VK_NULL_HANDLE;

		void Initialize(engine::Renderer& renderer);

		void Terminate(engine::Renderer& renderer) {
			renderer.DestroyPipeline(m_PipelineSDF);
			m_PipelineSDF = VK_NULL_HANDLE;
			renderer.DestroyPipelineLayout(m_PipelineLayoutSDF);
			m_PipelineLayoutSDF = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_QuadTransformDescriptorSetLayoutSDF);
			m_QuadTransformDescriptorSetLayoutSDF = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_RotatorInfoDescriptorSetLayoutSDF);
			m_RotatorInfoDescriptorSetLayoutSDF = VK_NULL_HANDLE;
			renderer.DestroyDescriptorSetLayout(m_MouseHitDescriptorSetLayoutSDF);
			m_MouseHitDescriptorSetLayoutSDF = VK_NULL_HANDLE;
		}
	};
}
