#include "builtin_pipelines.hpp"
#include "builtin_shaders.hpp"
#include "engine.hpp"

namespace pipelines {

	void World::Initialize(engine::Renderer& renderer, VkFormat colorImageResourceFormat) {

		using namespace engine;

		static constexpr VkDescriptorSetLayoutBinding camera_descriptor_set_layout_binding
			= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

		m_CameraDescriptorSetLayout = renderer.CreateDescriptorSetLayout(nullptr, 1, &camera_descriptor_set_layout_binding);

		if (m_CameraDescriptorSetLayout == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create camera descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		static constexpr VkDescriptorSetLayoutBinding texture_descriptor_set_layout_binding
			= Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

		m_TextureDescriptorSetLayoutPBR = renderer.CreateDescriptorSetLayout(nullptr, 1, &texture_descriptor_set_layout_binding);

		if (m_TextureDescriptorSetLayoutPBR == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create albedo descriptor set layout for world (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		const VkPushConstantRange pbrDrawPushConstantRange{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = 128,
		};

		const VkDescriptorSetLayout drawPbrDescriptorSetLayouts[2]{
			m_CameraDescriptorSetLayout,
			m_TextureDescriptorSetLayoutPBR,
		};

		m_DrawPipelineLayoutPBR
			= renderer.CreatePipelineLayout(2, drawPbrDescriptorSetLayouts, 1, &pbrDrawPushConstantRange);

		if (m_DrawPipelineLayoutPBR == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create pbr draw pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::Initialize)!");
		}

		const VkPushConstantRange udDrawPushConstantRange{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = 128,
		};

		m_DrawPipelineLayoutUD = renderer.CreatePipelineLayout(0, nullptr, 1, &udDrawPushConstantRange);

		if (m_DrawPipelineLayoutUD == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create unidirectional light pipeline layout (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		const VkDescriptorSetLayout pbrRenderDescriptorSetLayouts[2]{
			m_RenderPBRImagesDescriptorSetLayout,
			m_DirectionalLightShadowMapDescriptorSetLayout,
		};

		m_RenderPipelineLayoutPBR
			= renderer.CreatePipelineLayout(2, pbrRenderDescriptorSetLayouts, 0, nullptr);

		if (m_RenderPipelineLayoutPBR == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"faileld to create pbr render pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::Initialize)!");
		}

		const VkPushConstantRange debugPushConstantRanges[2]{
			Renderer::GetPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64),
			Renderer::GetPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16),
		};

		m_DebugPipelineLayout = renderer.CreatePipelineLayout(1, &m_CameraDescriptorSetLayout, 2, debugPushConstantRanges);

		if (m_DebugPipelineLayout == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create debug pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::initialize)");
		}

		Renderer::Shader pbrDrawShaders[2]{
			{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
			{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
		};

		if (!pbrDrawShaders[0].Compile(shaders::World::pbr_draw_pipeline_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile pbr draw vertex shader code (function Renderer::Shader::Compile in function pipelines::World::Initialize)!");
		}

		if (!pbrDrawShaders[1].Compile(shaders::World::pbr_draw_pipeline_fragment_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile pbr draw fragment shader code (function Renderer::Shader::Compile in function pipelines::World::Initialize)!");
		}

		const VkPipelineShaderStageCreateInfo pbrDrawShaderStageInfos[2]{
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[1]),
		};

		Renderer::Shader udDrawVertexShader(renderer, VK_SHADER_STAGE_VERTEX_BIT);

		if (!udDrawVertexShader.Compile(shaders::World::ud_draw_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile unidirectional light draw vertex shader (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		const VkPipelineShaderStageCreateInfo udDrawShaderStageInfo
			= Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(udDrawVertexShader);

		Renderer::Shader pbrRenderShaders[2]{
			{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
			{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
		};

		if (!pbrRenderShaders[0].Compile(shaders::World::pbr_render_pipeline_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile pbr render vertex shader code (function Renderer::Shader::Compile in function pipelines::World::Initialize)!");
		}

		if (!pbrRenderShaders[1].Compile(shaders::World::pbr_render_pipeline_fragment_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile pbr render fragment shader code (function Renderer::Shader::Compile in function pipelines::World:.Initialize)");
		}

		const VkPipelineShaderStageCreateInfo pbrRenderShaderStageInfos[2]{
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[1]),
		};

		Renderer::Shader debugShaders[2]{
			{ renderer, VK_SHADER_STAGE_VERTEX_BIT, },
			{ renderer, VK_SHADER_STAGE_FRAGMENT_BIT, },
		};

		if (!debugShaders[0].Compile(shaders::World::debug_pipeline_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile vertex shader code (function Renderer::Shader::Compile in function pipelines::World::Initialize)!");
		}

		if (!debugShaders[1].Compile(shaders::World::debug_pipeline_fragment_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile fragment shader code (function Renderer::Shader::Compile in function pipelines::World::Initialize)!");
		}

		const VkPipelineShaderStageCreateInfo debugShaderStageInfos[2]{
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[1]),
		};

		static constexpr uint32_t pbr_draw_color_attachment_count = 3;

		const VkFormat pbrDrawRenderingColorFormats[pbr_draw_color_attachment_count]{
			colorImageResourceFormat,
			colorImageResourceFormat,
			colorImageResourceFormat,
		};

		static constexpr uint32_t debug_pipelines_color_formats = 2;

		const VkPipelineRenderingCreateInfo pbrDrawRenderingInfo
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(pbr_draw_color_attachment_count, pbrDrawRenderingColorFormats, renderer.m_DepthOnlyFormat);

		const VkPipelineRenderingCreateInfo udRenderingCreateInfo
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(0, nullptr, renderer.m_DepthOnlyFormat);

		const VkPipelineRenderingCreateInfo pbrRenderRenderingInfo
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, VK_FORMAT_UNDEFINED);

		const VkPipelineRenderingCreateInfo debugRenderingInfo
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format,
				renderer.m_DepthOnlyFormat);

		VkPipelineColorBlendStateCreateInfo pbrDrawColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		pbrDrawColorBlendState.attachmentCount = pbr_draw_color_attachment_count;
		VkPipelineColorBlendAttachmentState pbrDrawColorAttachmentStates[pbr_draw_color_attachment_count]{
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
		};
		pbrDrawColorBlendState.pAttachments = pbrDrawColorAttachmentStates;

		VkPipelineColorBlendStateCreateInfo pbrRenderColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		pbrRenderColorBlendState.attachmentCount = 1;
		pbrRenderColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend;

		VkPipelineColorBlendStateCreateInfo debugColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		debugColorBlendState.attachmentCount = 1;
		debugColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state;

		VkPipelineRasterizationStateCreateInfo udPipelineRasterizationState = Renderer::GraphicsPipelineDefaults::rasterization_state;
		udPipelineRasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		VkPipelineRasterizationStateCreateInfo wirePipelineRasterizationState = Renderer::GraphicsPipelineDefaults::rasterization_state;
		wirePipelineRasterizationState.polygonMode = VK_POLYGON_MODE_LINE;

		static constexpr uint32_t pipeline_count = 5;

		VkGraphicsPipelineCreateInfo graphicsPipelineInfos[pipeline_count] = {
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &pbrDrawRenderingInfo,
				.stageCount = 2,
				.pStages = pbrDrawShaderStageInfos,
				.pVertexInputState = &Vertex::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &pbrDrawColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_DrawPipelineLayoutPBR,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &udRenderingCreateInfo,
				.flags = 0,
				.stageCount = 1,
				.pStages = &udDrawShaderStageInfo,
				.pVertexInputState = &Vertex::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &udPipelineRasterizationState,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &Renderer::GraphicsPipelineDefaults::color_blend_state,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_DrawPipelineLayoutUD,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &pbrRenderRenderingInfo,
				.stageCount = 2,
				.pStages = pbrRenderShaderStageInfos,
				.pVertexInputState = &Vertex2D::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
				.pColorBlendState = &pbrRenderColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_RenderPipelineLayoutPBR,
				.renderPass = nullptr,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &debugRenderingInfo,
				.stageCount = 2,
				.pStages = debugShaderStageInfos,
				.pVertexInputState = &Vertex::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &wirePipelineRasterizationState,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &debugColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_DebugPipelineLayout,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
		};

		graphicsPipelineInfos[4] = graphicsPipelineInfos[3];
		graphicsPipelineInfos[4].pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state;

		VkPipeline pipelines[pipeline_count];

		if (!renderer.CreateGraphicsPipelines(pipeline_count, graphicsPipelineInfos, pipelines)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create world graphics pipeline (function Renderer::CreateGraphicsPipelines in function pipelines::World::Initialize)!");
		}

		m_DrawPipelinePBR = pipelines[0];
		m_DrawPipelineUD = pipelines[1];
		m_RenderPipelinePBR = pipelines[2];
		m_DebugWirePipeline = pipelines[3];
		m_DebugSolidPipeline = pipelines[4];
	}
}
