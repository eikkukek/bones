#include "engine.hpp"

using namespace engine;

struct TestPipeline {

	Renderer& m_Renderer;
	VkPipelineLayout m_GpuPipelineLayout;
	VkPipeline m_GpuPipeline;

	static constexpr const char* cexpr_vertex_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec3 outPosition;

void main() {
	outPosition = inPosition;
	gl_Position = vec4(inPosition, 1.0f);
}
	)";

	static constexpr const char* cexpr_fragment_shader = R"(
#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(inPosition, 1.0f);
}
	)";

	TestPipeline(Renderer& renderer) 
		: m_Renderer(renderer), m_GpuPipelineLayout(VK_NULL_HANDLE),
			m_GpuPipeline(VK_NULL_HANDLE) {

		m_GpuPipelineLayout = m_Renderer.CreatePipelineLayout(0, nullptr, 0, nullptr);
		assert(m_GpuPipelineLayout != VK_NULL_HANDLE);

		Renderer::Shader vertexShader(m_Renderer);
		Renderer::Shader fragmentShader(m_Renderer);

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
			.stride = sizeof(Engine::Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};

		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexBinding,
		};

		Engine::Vertex::GetVertexAttributes(vertexInputStateInfo.vertexAttributeDescriptionCount, &vertexInputStateInfo.pVertexAttributeDescriptions);

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineTessellationStateCreateInfo tessellationStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.patchControlPoints = 1,
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
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 0.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilState {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		VkPipelineColorBlendAttachmentState colorAttachment {
			.blendEnable = VK_FALSE,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_TRUE,
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

		VkGraphicsPipelineCreateInfo pipelineInfo {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputStateInfo,
			.pInputAssemblyState = &inputAssemblyStateInfo,
			.pTessellationState = &tessellationStateInfo,
			.pViewportState = &viewPortStateInfo,
			.pRasterizationState = &rasterizationStateInfo,
			.pMultisampleState = &multisampleStateInfo,
			.pDepthStencilState = &depthStencilState,
			.pDynamicState = &dynamicStateInfo,
			.layout = m_GpuPipelineLayout,
			.renderPass = VK_NULL_HANDLE,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0,
		};

		m_Renderer.CreateGraphicsPipelines(1, &pipelineInfo, &m_GpuPipeline);

		vkDestroyShaderModule(m_Renderer.m_GpuDevice, shaderStages[0].module, m_Renderer.m_GpuAllocationCallbacks);
		vkDestroyShaderModule(m_Renderer.m_GpuDevice, shaderStages[1].module, m_Renderer.m_GpuAllocationCallbacks);
	}

	void Terminate() {
		vkDestroyPipelineLayout(m_Renderer.m_GpuDevice, m_GpuPipelineLayout, m_Renderer.m_GpuAllocationCallbacks);
		vkDestroyPipeline(m_Renderer.m_GpuDevice, m_GpuPipeline, m_Renderer.m_GpuAllocationCallbacks);
	}
};

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);
	Engine engine("Test", pWindow, 0, nullptr, 1000);
	TestPipeline testPipeline(engine.m_Renderer);
	engine.AddGraphicsPipeline(testPipeline.m_GpuPipeline, testPipeline.m_GpuPipelineLayout, 0, 1000);
	Engine::Mesh<Engine::MeshType::Static> mesh(engine);
	Engine::Vertex vertices[3]{};
	uint32_t indices[3]{ 0, 1, 2 };
	mesh.CreateBuffers(3, vertices, 3, indices);
	while (!glfwWindowShouldClose(pWindow)) {
		glfwPollEvents();
		engine.DrawLoop();
	}
	vkDeviceWaitIdle(engine.m_Renderer.m_GpuDevice);	
	mesh.Terminate();
	testPipeline.Terminate();
	glfwTerminate();
}
