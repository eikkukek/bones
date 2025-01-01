#include "engine.hpp"

struct TestPipeline {

	engine::Renderer& m_Renderer;
	VkDescriptorSetLayout m_DescriptorSetLayout;
	VkPipelineLayout m_GpuPipelineLayout;
	VkPipeline m_GpuPipeline;

	static constexpr const char* cexpr_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 outUV;

void main() {
	outUV = inUV;
	gl_Position = vec4(inPosition, 1.0f);
}
	)";

	static constexpr const char* cexpr_fragment_shader = R"(
#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
	outColor = texture(tex, inUV);
}
	)";

	TestPipeline(engine::Renderer& renderer) 
		: m_Renderer(renderer), m_GpuPipelineLayout(VK_NULL_HANDLE),
			m_GpuPipeline(VK_NULL_HANDLE) {

		VkDescriptorSetLayoutBinding descriptorSetBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		m_DescriptorSetLayout = renderer.CreateDescriptorSetLayout(1, &descriptorSetBinding);

		m_GpuPipelineLayout = m_Renderer.CreatePipelineLayout(1, &m_DescriptorSetLayout, 0, nullptr);
		assert(m_GpuPipelineLayout != VK_NULL_HANDLE);

		engine::Renderer::Shader vertexShader(m_Renderer);
		engine::Renderer::Shader fragmentShader(m_Renderer);

		vertexShader.Compile(cexpr_vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
		fragmentShader.Compile(cexpr_fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);

		VkPipelineShaderStageCreateInfo shaderStages[2] {
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertexShader.CreateShaderModule(),
				.pName = "main",
				.pSpecializationInfo = nullptr,
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragmentShader.CreateShaderModule(),
				.pName = "main",
				.pSpecializationInfo = nullptr,
			},
		};

		VkVertexInputBindingDescription vertexBinding {
			.binding = 0,
			.stride = sizeof(engine::Engine::Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexBinding,
		};

		engine::Engine::Vertex::GetVertexAttributes(vertexInputStateInfo.vertexAttributeDescriptionCount,
			&vertexInputStateInfo.pVertexAttributeDescriptions);

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineViewportStateCreateInfo viewPortStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr,
		};

		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasClamp = VK_FALSE,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 0.2f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilState {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_NEVER,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front {},
			.back {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		VkPipelineColorBlendAttachmentState colorAttachment {
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &colorAttachment,
			.blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
		};

		VkDynamicState dynamicStates[2] {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = 2,
			.pDynamicStates = dynamicStates,
		};

		VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.pNext = nullptr,
			.viewMask = 0,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &m_Renderer.m_SwapchainSurfaceFormat.format,
			.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

		VkGraphicsPipelineCreateInfo pipelineInfo {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingInfo,
			.flags = 0,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputStateInfo,
			.pInputAssemblyState = &inputAssemblyStateInfo,
			.pTessellationState = nullptr,
			.pViewportState = &viewPortStateInfo,
			.pRasterizationState = &rasterizationStateInfo,
			.pMultisampleState = &multisampleStateInfo,
			.pDepthStencilState = &depthStencilState,
			.pColorBlendState = &colorBlendStateInfo,
			.pDynamicState = &dynamicStateInfo,
			.layout = m_GpuPipelineLayout,
			.renderPass = VK_NULL_HANDLE,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0,
		};

		m_Renderer.CreateGraphicsPipelines(1, &pipelineInfo, &m_GpuPipeline);

		vkDestroyShaderModule(m_Renderer.m_VulkanDevice, shaderStages[0].module, m_Renderer.m_VulkanAllocationCallbacks);
		vkDestroyShaderModule(m_Renderer.m_VulkanDevice, shaderStages[1].module, m_Renderer.m_VulkanAllocationCallbacks);
	}

	void Terminate() {
		vkDestroyPipelineLayout(m_Renderer.m_VulkanDevice, m_GpuPipelineLayout, m_Renderer.m_VulkanAllocationCallbacks);
		vkDestroyPipeline(m_Renderer.m_VulkanDevice, m_GpuPipeline, m_Renderer.m_VulkanAllocationCallbacks);
		vkDestroyDescriptorSetLayout(m_Renderer.m_VulkanDevice, m_DescriptorSetLayout, m_Renderer.m_VulkanAllocationCallbacks);
	}
};

class TestEntity : public engine::Entity {

public:

	VkDeviceSize m_VertexOffset = 0;
	engine::MeshData m_MeshData{};
	engine::Renderer::DescriptorPool m_ImageDescriptorPool;
	VkDescriptorSetLayout m_ImageDescriptorSetLayout;
	VkDescriptorSet m_ImageDescriptorSet;
	VkSampler m_ImageSampler;
	VkImageView m_ImageView;

	TestEntity(engine::Engine& engine, const engine::StaticTexture& texture) 
		: Entity(engine, "Test", 0, sizeof(TestEntity)), m_ImageDescriptorPool(engine.m_Renderer, 1) {
		m_MeshData = engine.m_StaticQuadMesh.GetMeshData();
		m_ImageView = texture.CreateImageView();
		VkSamplerCreateInfo samplerInfo {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.compareEnable = VK_FALSE,
			.minLod = 0.0f,
			.maxLod = VK_LOD_CLAMP_NONE,
			.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		vkCreateSampler(m_Engine.m_Renderer.m_VulkanDevice, &samplerInfo, m_Engine.m_Renderer.m_VulkanAllocationCallbacks, &m_ImageSampler);
		VkDescriptorSetLayoutBinding setBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};
		m_ImageDescriptorSetLayout = engine.m_Renderer.CreateDescriptorSetLayout(1, &setBinding);
		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
		};
		m_ImageDescriptorPool.Create(1, &poolSize);
		engine.m_Renderer.AllocateDescriptorSets(m_ImageDescriptorPool, 1, &m_ImageDescriptorSetLayout, &m_ImageDescriptorSet);
		VkDescriptorImageInfo imageInfo {
			.sampler = m_ImageSampler,
			.imageView = m_ImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		VkWriteDescriptorSet write {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_ImageDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo,
		};
		engine.m_Renderer.UpdateDescriptorSets(1, &write);
	}

	bool LogicUpdate() {
		return true;
	}

	void RenderUpdate(const engine::GraphicsPipeline& pipeline, const engine::CameraData& camera, const uint32_t descriptorCount,
		VkDescriptorSet** outDescriptorSets, uint32_t& meshCount, engine::MeshData** meshes) {
		*meshes = &m_MeshData;
		meshCount = 1;
		*outDescriptorSets = &m_ImageDescriptorSet;
	}

	void WriteToFile(FILE* file) {}

	void OnTerminate() {
		const engine::Renderer& renderer = m_Engine.m_Renderer;
		vkDestroyImageView(renderer.m_VulkanDevice, m_ImageView, renderer.m_VulkanAllocationCallbacks);
		vkDestroySampler(renderer.m_VulkanDevice, m_ImageSampler, renderer.m_VulkanAllocationCallbacks);
		m_ImageDescriptorPool.Terminate();
		vkDestroyDescriptorSetLayout(renderer.m_VulkanDevice, m_ImageDescriptorSetLayout, renderer.m_VulkanAllocationCallbacks);
	}
};

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);
	engine::Engine engine("Test", pWindow, false, 100, 0, nullptr, 1000);
	engine::TextRenderer& textRenderer = engine.m_TextRenderer;
	engine::GlyphAtlas atlas{};
	textRenderer.CreateGlyphAtlas("fonts\\arial_mt.ttf", 30.0f, atlas);
	const char* text = "Hello, how\nis it going? AVAVAVA";
	engine::TextImage textImage 
			= textRenderer.RenderText<engine::TextAlignment::Middle>(text, atlas, engine::PackColorRBGA({ 1.0f, 1.0f, 1.0f, 1.0f }),
				{ 540, 540 }, { 2, 2 });
	engine::StaticTexture texture(engine);
	texture.Create(VK_FORMAT_R8G8B8A8_SRGB, 4, textImage.m_Extent, textImage.m_Image);	
	TestPipeline testPipeline(engine.m_Renderer);
	engine::Engine::GraphicsPipeline& testPipelineData =
			engine.AddGraphicsPipeline(testPipeline.m_GpuPipeline, testPipeline.m_GpuPipelineLayout, 1, 1000);
	TestEntity testEntity(engine, texture);
	testPipelineData.m_Entites.Insert(&testEntity);
	while (!glfwWindowShouldClose(pWindow)) {
		glfwPollEvents();
		engine.Render();
	}
	vkDeviceWaitIdle(engine.m_Renderer.m_VulkanDevice);	
	testEntity.OnTerminate();
	testPipeline.Terminate();
	glfwTerminate();
	free(atlas.m_Atlas);
	textRenderer.DestroyTextImage(textImage);
	texture.Terminate();
}
