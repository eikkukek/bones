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

		const VkPushConstantRange pbrDrawPushConstantRange {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = 128,
		};

		const VkDescriptorSetLayout drawPbrDescriptorSetLayouts[2] {
			m_CameraDescriptorSetLayout,
			m_TextureDescriptorSetLayoutPBR,
		};

		m_DrawPipelineLayoutPBR 
			= renderer.CreatePipelineLayout(2, drawPbrDescriptorSetLayouts, 1, &pbrDrawPushConstantRange);

		if (m_DrawPipelineLayoutPBR == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create pbr draw pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::Initialize)!");
		}

		const VkPushConstantRange udDrawPipelinePushConstantRange {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = 128,
		};

		m_DrawPipelineLayoutUD = renderer.CreatePipelineLayout(0, nullptr, 1, &udDrawPipelinePushConstantRange);

		if (m_DrawPipelineLayoutUD == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to create unidirectional light pipeline layout (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		const VkDescriptorSetLayout pbrRenderPipelineDescriptorSetLayouts[2] {
			m_RenderPBRImagesDescriptorSetLayout,
			m_DirectionalLightShadowMapDescriptorSetLayout,
		};

		m_RenderPipelineLayoutPBR 
				= renderer.CreatePipelineLayout(2, pbrRenderPipelineDescriptorSetLayouts, 0, nullptr);

		if (m_RenderPipelineLayoutPBR == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer, 
				"faileld to create pbr render pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::Initialize)!");
		}

		const VkPushConstantRange debugPushConstantRanges[2] {
			Renderer::GetPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64),
			Renderer::GetPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 64, 16),
		};

		m_DebugPipelineLayout = renderer.CreatePipelineLayout(1, &m_CameraDescriptorSetLayout, 2, debugPushConstantRanges);

		if (m_DebugPipelineLayout == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create debug pipeline layout for world (function Renderer::CreatePipelineLayout in function pipelines::World::initialize)");
		}

		Renderer::Shader pbrDrawShaders[2] {
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

		const VkPipelineShaderStageCreateInfo pbrDrawPipelineShaderStageInfos[2] {
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrDrawShaders[1]),
		};

		Renderer::Shader udDrawVertexShader(renderer, VK_SHADER_STAGE_VERTEX_BIT);

		if (!udDrawVertexShader.Compile(shaders::World::ud_draw_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer, 
				"failed to compile unidirectional light draw vertex shader (function Renderer::CreateDescriptorSetLayout in function pipelines::World::Initialize)!");
		}

		const VkPipelineShaderStageCreateInfo udDrawPipelineShaderStageInfo 
			= Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(udDrawVertexShader);

		Renderer::Shader pbrRenderShaders[2] {
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

		const VkPipelineShaderStageCreateInfo pbrRenderPipelineShaderStageInfos[2] {
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(pbrRenderShaders[1]),
		};

		Renderer::Shader debugShaders[2] {
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

		const VkPipelineShaderStageCreateInfo debugPipelineShaderStageInfos[2] {
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[0]),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(debugShaders[1]),
		};

		static constexpr uint32_t pbr_draw_color_attachment_count = 3;

		const VkFormat pbrDrawRenderingColorFormats[pbr_draw_color_attachment_count] {
			colorImageResourceFormat,
			colorImageResourceFormat,
			colorImageResourceFormat,
		};

		const VkPipelineRenderingCreateInfo pbrDrawPipelineRenderingInfo 
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(pbr_draw_color_attachment_count, pbrDrawRenderingColorFormats, renderer.m_DepthOnlyFormat);

		const VkPipelineRenderingCreateInfo udPipelineRenderingCreateInfo 
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(0, nullptr, renderer.m_DepthOnlyFormat);

		const VkPipelineRenderingCreateInfo pbrRenderPipelineRenderingInfo
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, VK_FORMAT_UNDEFINED);

		const VkPipelineRenderingCreateInfo debugPipelineRenderingInfo 
			= Renderer::GraphicsPipelineDefaults::GetRenderingCreateInfo(1, &renderer.m_SwapchainSurfaceFormat.format, 
				renderer.m_DepthOnlyFormat);

		VkPipelineColorBlendStateCreateInfo pbrDrawPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		pbrDrawPipelineColorBlendState.attachmentCount = pbr_draw_color_attachment_count;
		VkPipelineColorBlendAttachmentState pbrDrawPipelineColorAttachmentStates[pbr_draw_color_attachment_count] {
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
			Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend,
		};
		pbrDrawPipelineColorBlendState.pAttachments = pbrDrawPipelineColorAttachmentStates;

		VkPipelineColorBlendStateCreateInfo pbrRenderPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		pbrRenderPipelineColorBlendState.attachmentCount = 1;
		pbrRenderPipelineColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state_no_blend;

		VkPipelineColorBlendStateCreateInfo debugPipelineColorBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		debugPipelineColorBlendState.attachmentCount = 1;
		debugPipelineColorBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state;

		VkPipelineRasterizationStateCreateInfo udPipelineRasterizationState = Renderer::GraphicsPipelineDefaults::rasterization_state;
		udPipelineRasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		VkPipelineRasterizationStateCreateInfo wirePipelineRasterizationState = Renderer::GraphicsPipelineDefaults::rasterization_state;
		wirePipelineRasterizationState.polygonMode = VK_POLYGON_MODE_LINE;

		static constexpr uint32_t pipeline_count = 4;

		VkGraphicsPipelineCreateInfo graphicsPipelineInfos[pipeline_count] = { 
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &pbrDrawPipelineRenderingInfo,
				.stageCount = 2,
				.pStages = pbrDrawPipelineShaderStageInfos,
				.pVertexInputState = &Vertex::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &pbrDrawPipelineColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_DrawPipelineLayoutPBR,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &udPipelineRenderingCreateInfo,
				.flags = 0,
				.stageCount = 1,
				.pStages = &udDrawPipelineShaderStageInfo,
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
				.pNext = &pbrRenderPipelineRenderingInfo,
				.stageCount = 2,
				.pStages = pbrRenderPipelineShaderStageInfos,
				.pVertexInputState = &Vertex2D::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state_no_depth_tests,
				.pColorBlendState = &pbrRenderPipelineColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_RenderPipelineLayoutPBR,
				.renderPass = nullptr,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &debugPipelineRenderingInfo,
				.stageCount = 2,
				.pStages = debugPipelineShaderStageInfos,
				.pVertexInputState = &Vertex::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &wirePipelineRasterizationState,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &debugPipelineColorBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_DebugPipelineLayout,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
		};

		VkPipeline pipelines[pipeline_count];

		if (!renderer.CreateGraphicsPipelines(pipeline_count, graphicsPipelineInfos, pipelines)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create world graphics pipeline (function Renderer::CreateGraphicsPipelines in function pipelines::World::Initialize)!");
		}

		m_DrawPipelinePBR = pipelines[0];
		m_DrawPipelineUD = pipelines[1];
		m_RenderPipelinePBR = pipelines[2];
		m_WirePipeline = pipelines[3];
	}

	void Editor::Initialize(engine::Renderer& renderer) {
		using namespace engine;

		VkDescriptorSetLayoutBinding rotatorBindings[1] {
			Renderer::GetDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT),
		};

		m_RotatorInfoDescriptorSetLayoutSDF = renderer.CreateDescriptorSetLayout(nullptr, 1, rotatorBindings);

		if (m_RotatorInfoDescriptorSetLayoutSDF == VK_NULL_HANDLE) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create SDF rotator info descriptor set layout (function Renderer::CreateDescriptorSetLayout in function pipelines::Editor::Initialize)!");
		}

		VkDescriptorSetLayout sdfSetLayouts[3] {
			m_QuadTransformDescriptorSetLayoutSDF,
			m_RotatorInfoDescriptorSetLayoutSDF,
			m_MouseHitDescriptorSetLayoutSDF,
		};

		const VkPushConstantRange sdfPushConstantRanges[2] {
			{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
				.offset = 0,
				.size = 72,
			},
			{
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.offset = 80,
				.size = 20,
			},
		};

		m_PipelineLayoutSDF = renderer.CreatePipelineLayout(3, sdfSetLayouts, 2, sdfPushConstantRanges);

		if (m_PipelineLayoutSDF == VK_NULL_HANDLE) { 
			CriticalError(ErrorOrigin::Renderer,
				"failed to create SDF pipeline layout (function Renderer::CreatePipelineLayout in function pipelines::Editor::Initialize)!");
		}

		Renderer::Shader sdfVertexShader(renderer, VK_SHADER_STAGE_VERTEX_BIT);
		Renderer::Shader sdfFragmentShader(renderer, VK_SHADER_STAGE_FRAGMENT_BIT);

		if (!sdfVertexShader.Compile(shaders::Editor::sdf_pipeline_vertex_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile SDF vertex shader (function Renderer::Shader::Compile in function pipelines::Editor::Initialize)!");
		}

		if (!sdfFragmentShader.Compile(shaders::Editor::sdf_pipeline_fragment_shader)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to compile SDF fragment shader (function Renderer::Shader::Compile in function pipelines::Editor::Initialize)!");
		}

		VkPipelineShaderStageCreateInfo sdfShaderStageInfos[2] {
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(sdfVertexShader),
			Renderer::GraphicsPipelineDefaults::GetShaderStageInfo(sdfFragmentShader),
		};

		VkFormat sdfAttachmentFormats[1] {
			renderer.m_SwapchainSurfaceFormat.format,
		};

		VkPipelineRenderingCreateInfo sdfRenderingInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = sdfAttachmentFormats,
			.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

		VkPipelineColorBlendStateCreateInfo sdfBlendState = Renderer::GraphicsPipelineDefaults::color_blend_state;
		sdfBlendState.attachmentCount = 1;
		sdfBlendState.pAttachments = &Renderer::GraphicsPipelineDefaults::color_blend_attachment_state;
;

		VkGraphicsPipelineCreateInfo graphicsPipelineInfos[1] {
			{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &sdfRenderingInfo,
				.stageCount = 2,
				.pStages = sdfShaderStageInfos,
				.pVertexInputState = &Vertex2D::GetVertexInputState(),
				.pInputAssemblyState = &Renderer::GraphicsPipelineDefaults::input_assembly_state,
				.pTessellationState = nullptr,
				.pViewportState = &Renderer::GraphicsPipelineDefaults::viewport_state,
				.pRasterizationState = &Renderer::GraphicsPipelineDefaults::rasterization_state,
				.pMultisampleState = &Renderer::GraphicsPipelineDefaults::multisample_state,
				.pDepthStencilState = &Renderer::GraphicsPipelineDefaults::depth_stencil_state,
				.pColorBlendState = &sdfBlendState,
				.pDynamicState = &Renderer::GraphicsPipelineDefaults::dynamic_state,
				.layout = m_PipelineLayoutSDF,
				.renderPass = VK_NULL_HANDLE,
				.subpass = 0,
				.basePipelineHandle = VK_NULL_HANDLE,
				.basePipelineIndex = 0,
			},
		};

		if (!renderer.CreateGraphicsPipelines(1, graphicsPipelineInfos, &m_PipelineSDF)) {
			CriticalError(ErrorOrigin::Renderer,
				"failed to create graphis pipelines for editor (function Renderer::CreateGraphicsPipelines in function pipelines::Editor::Initialize)!");
		}
	}
}
