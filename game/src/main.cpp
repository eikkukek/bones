#include "engine.hpp"
#include <chrono>

class Player {
public:

	static inline Player* s_Instance = nullptr;
	
	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Quaternion m_Rotation;
	engine::Engine::PersistentReference<engine::Creature> m_Creature;
	engine::Engine::PersistentReference<engine::World::RenderData> m_RenderData;

	static engine::Vec3 MovementVectorUpdate(const engine::Creature& creature) {
		using namespace engine;
		using Key = Input::Key;
		return Vec3(
				Input::ReadKeyValue(Key::D) - Input::ReadKeyValue(Key::A),
				0.0f,
				Input::ReadKeyValue(Key::W) - Input::ReadKeyValue(Key::S)) * (5 * Time::DeltaTime());
	}

	static void MoveCallback(const engine::Creature& creature, const engine::Vec3& position, const engine::Vec3& deltaPosition) {
		using namespace engine;
		s_Instance->m_RenderData.m_Val->m_Transform[3] = Vec4(position, 1.0f);
	}

	static void CameraFollowCallback(const engine::Creature& creature, engine::Vec3& outCameraPosition, engine::Vec3& outCameraLookAt) {
		using namespace engine;
		const engine::Vec3& creaturePos = creature.GetPosition();
		static const Vec3 cameraOffset(20.0f, 20.0f, -20.0f);
		static const Vec3 lookAtOffset(0.0f, 0.0f, 10.0f);
		outCameraPosition = creaturePos + cameraOffset;
		outCameraLookAt = creaturePos + lookAtOffset;
	}

	Player(engine::World& world, engine::StaticMesh& mesh) 
		: m_World(world), 
			m_Creature(world.AddCreature({ 3.0f, 0.0f, 0.0f }, 
				{ 
					.m_LocalPosition {}, 
					.m_Type { engine::Collider::Type::Pole },
					.u_TypeInfo { .m_PoleInfo { .m_Radius = 1, .m_Height = 2 } },
				}
			)), 
			m_Mesh(mesh), m_RenderData(world.AddRenderData(*m_Creature, {}, {})) {
		using namespace engine;
		s_Instance = this;
		World::RenderData& renderData = *m_RenderData;
		renderData.m_Transform = engine::Mat4(1);
		renderData.m_Transform[3].z = 3;
		renderData.m_MeshData = m_Mesh.GetMeshData();
		Creature& creature = *m_Creature;
		creature.m_MovementVectorUpdate = MovementVectorUpdate;
		creature.m_MoveCallback = MoveCallback;
		creature.m_CameraFollowCallback = CameraFollowCallback;
		world.SetCameraFollowCreature(*m_Creature);
	}	
};

/*

class NPC {
public:

	static inline NPC* s_Instance = nullptr;

	engine::World& m_World;
	engine::StaticMesh& m_Mesh;
	engine::Engine::Reference<engine::Creature> m_Creature;

	NPC(engine::World& world, engine::StaticMesh& mesh)
		: m_World(world), m_Creature(world.AddCreature(engine::Vec3(0.0f, 0.0f, 0.0f))), m_Mesh(mesh) {
		using namespace engine;
		s_Instance = this;
		World::RenderData& renderData = *m_World.AddRenderData(*m_Creature, engine::Mat4(1), m_Mesh.GetMeshData());
		renderData.m_MeshData = m_Mesh.GetMeshData();
		renderData.m_Transform = Mat4(1);
	}
};

*/

int main() {
	using namespace engine;

	Mat4 m0 = Mat4::LookAt(0.0f, Vec3::Up(), Vec3::Forward());
	Mat4 m1 = Mat4::LookAt(Vec3(10.0f, 10.0f, 10.0f), Vec3::Up(), Vec3::Backward());
	Vec4 v0 = Vec3(3.0f, 0.0f, 3.0f);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);

	engine::Engine engine(engine::EngineMode_Play, "Test", pWindow, 1000);

	TextRenderer& textRenderer = engine.GetTextRenderer();

	GlyphAtlas atlas{};
	textRenderer.CreateGlyphAtlas("resources\\fonts\\arial_mt.ttf", 30.0f, atlas);
	const char* text = "Hello, how\nis it going? AVAVAVA";
	TextRenderer::RenderTextInfo renderInfo {
		.m_GlyphAtlas = atlas,
		.m_Spacing = { 2, 2 },
		.m_TextColor = PackColorRBGA({ 1.0f, 1.0f, 1.0f, 0.8f }),
		.m_BackGroundColor = PackColorRBGA({ 0.0f, 0.0f, 1.0f, 0.5f }),
	};
	TextImage textImage 
			= textRenderer.RenderText<TextAlignment::Middle>(text, renderInfo, { 540, 540 });
	StaticTexture texture(engine);

	texture.Create(VK_FORMAT_R8G8B8A8_SRGB, 4, textImage.m_Extent, textImage.m_Image);
	VkImageView textureImageView = texture.CreateImageView();
	UI& UI = engine.GetUI();
	static VkDescriptorSet testUIDescriptorSet;
	VkDescriptorPool testUIDescriptorPool;
	UI.CreateTexture2DArray<1>(&textureImageView, testUIDescriptorSet, testUIDescriptorPool);
	UI::Window* uiWindow = UI.AddWindow("Moi", UI::WindowState::Focused, { 0, 0 }, { 270, 270 });
	uiWindow->m_Pipeline2DRenderCallback = [](const UI::Window& window, VkDescriptorSet& outDescriptorSet, uint32_t& outTextureIndex) -> bool {
		outDescriptorSet = testUIDescriptorSet;
		outTextureIndex = 0;
		return true;
	};

	Engine::GroundInfo groundInfo {
		.m_TopViewBoundingRect {
			.m_Min { -8.0f, -8.0f },
			.m_Max { 8.0f, 8.0f },
		}
	};

	Engine::ObstacleInfo obstacleInfo {
		.m_Position = {},
		.m_Rotation = Quaternion::AxisRotation(Vec3(0.0f, 1.0f, 0.0), pi / 4),
		.m_Dimensions = { 2.0f, 2.0f, 2.0f },
		.m_ColliderInfo {
			.m_LocalPosition = {},
			.m_Type = Collider::Type::Fence,
			.u_TypeInfo {
				.m_FenceInfo { .m_Dimensions { 4, 4, 4 }, .m_YRotation = pi / 4 },
			}
		},
	};

	World& world = engine.LoadWorld({ 128, 128 }, { 8, 8 }, 1, &groundInfo, 1, &obstacleInfo);

	const Engine::DynamicArray<Engine::Ground>& grounds = world.GetGrounds();

	MeshData groundMesh = engine.GetQuadMesh().GetMeshData();

	Mat4 groundTransform = Quaternion::AxisRotation(Vec3(1.0f, 0.0f, 0.0f), -pi / 2).AsMat4();
	groundTransform[0] *= 10.0f;
	groundTransform[1] *= 10.0f;
	groundTransform[2] *= 10.0f;
	groundTransform[3] = { 0.0f, 1.0f, 0.0f, 1.0f };

	world.AddRenderData(grounds[0], groundTransform, groundMesh);

	Engine::Obj cubeObj{};
	FILE* fileStream = fopen("resources\\meshes\\cube.obj", "r");
	assert(cubeObj.Load(fileStream));
	fclose(fileStream);

	Engine::DynamicArray<uint32_t> cubeIndices{};
	Engine::DynamicArray<Engine::Vertex> cubeVertices{};

	assert(cubeObj.GetMesh(Engine::Vertex::SetPosition, Engine::Vertex::SetUV, 
		Engine::Vertex::SetNormal, cubeVertices, cubeIndices));	

	StaticMesh cubeMesh(engine);
	cubeMesh.CreateBuffers(cubeVertices.m_Size, cubeVertices.m_Data, cubeIndices.m_Size, cubeIndices.m_Data);

	const Engine::DynamicArray<Engine::Obstacle>& obstacles = world.GetObstacles();

	Mat4 obstacleTransform = obstacleInfo.m_Rotation.AsMat4();
	for (size_t i = 0; i < 3; i++) {
		obstacleTransform[i] *= 2;
	}

	world.AddRenderData(obstacles[0], obstacleTransform, cubeMesh.GetMeshData());
	world.AddDebugRenderData(obstacles[0], obstacleTransform, Vec4(0.0f, 0.8f, 0.3f, 1.0f), cubeMesh.GetMeshData());

	Player player(world, cubeMesh);
	//NPC npc(world, playerMesh);

	while (engine.Loop()) {
		float deltaTime = Time::DeltaTime();
		uiWindow->SetPosition(UI.m_CursorPosition);
	}

	Renderer& renderer = engine.GetRenderer();
	vkDeviceWaitIdle(renderer.m_VulkanDevice);	
	player.m_Mesh.Terminate();
	renderer.DestroyDescriptorPool(testUIDescriptorPool);
	glfwTerminate();
	free(atlas.m_Atlas);
	textRenderer.DestroyTextImage(textImage);
	texture.Terminate();
	renderer.DestroyImageView(textureImageView);
}
